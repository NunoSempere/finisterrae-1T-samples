#include <float.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"
#include "model.h"

// Macro to be able to run part of the code in a system without MPI
#define NO_MPI // Comment out if running on an MPI system
#ifdef NO_MPI
    #define IF_MPI(x)
    #define IF_NO_MPI(x) x
    #define MPI_Status int
    #define N_SAMPLES_PER_PROCESS 1000000
#else
    #include "mpi.h" /* N: why is this "mpi.h" and not <mpi.h> ??? */
    #define IF_MPI(x) x
    #define IF_NO_MPI(x)
    #define N_SAMPLES_PER_PROCESS 1000000000
#endif

/* External interface struct */
typedef struct _Finisterrae_params {
    const double (*sampler)(uint64_t* seed);
    const int n_samples_per_process;
    const double histogram_min;
    const double histogram_sup;
    const double histogram_bin_width;
    const double histogram_n_bins;
    const int print_every_n_iters;
} Finisterrae_params;

/* Internal interface structs */ 
#define CACHE_LINE_SIZE 64
typedef struct _seed_cache_box {
    uint64_t seed;
    char padding[CACHE_LINE_SIZE - sizeof(uint64_t)];
    // Cache line size is 64 *bytes*, uint64_t is 64 *bits* (8 bytes). Different units!
} seed_cache_box;


typedef struct _Histogram {
    double min;
    double sup;
    double bin_width;
    double n_bins;
    int* bins;
} Histogram;

typedef struct _Outliers {
    double* os;
    int n;
    int capacity;
} Outliers;

typedef struct _Summary_stats {
    uint64_t n_samples;
    double min;
    double max;
    double mean;
    double variance;
    Histogram histogram;
    Outliers outliers;
} Summary_stats;

/* Helpers */
double combine_variances(Summary_stats* x, Summary_stats* y)
{
    double term1 = ((x->n_samples) * x->variance + (y->n_samples) * y->variance) / (x->n_samples + y->n_samples);
    double term2 = (x->n_samples * y->n_samples * (x->mean - y->mean) * (x->mean - y->mean)) / ((x->n_samples + y->n_samples) * (x->n_samples + y->n_samples));
    double result = term1 + term2;
    return result;
    // https://math.stackexchange.com/questions/2971315/how-do-i-combine-standard-deviations-of-two-groups
    // but use the standard deviation of the samples, no the estimate of the standard deviation of the underlying distribution
}

void reduce_chunk_stats(Summary_stats* accumulator, Summary_stats* new, int n_chunks)
{
    double sum_weighted_means = accumulator->mean * accumulator->n_samples;
    for (int i = 0; i < n_chunks; i++) {
        sum_weighted_means += new[i].mean * new[i].n_samples;
        accumulator->n_samples += new[i].n_samples;
        accumulator->mean = sum_weighted_means / accumulator->n_samples; // need this for the variance calculations
        accumulator->variance = combine_variances(accumulator, new + i);
        if (accumulator->min > new[i].min) accumulator->min = new[i].min;
        if (accumulator->max < new[i].max) accumulator->max = new[i].max;
        for (int j = 0; j < accumulator->histogram.n_bins; j++) {
            accumulator->histogram.bins[j] += new[i].histogram.bins[j];
        }
        if(accumulator->outliers.n + new[i].outliers.n >= accumulator->outliers.capacity){
            int new_capacity = accumulator->outliers.capacity * 2;
            double* new_os = (double*)realloc(accumulator->outliers.os, new_capacity * sizeof(double));
            if (new_os == NULL) {
                printf("Memory reallocation for outliers failed\n");
                return;
            }
            accumulator->outliers.os = new_os;
            accumulator->outliers.capacity = new_capacity;
        }
        for(int k=0; k < new[i].outliers.n && accumulator->outliers.n < accumulator->outliers.capacity; k++){
            accumulator->outliers.os[accumulator->outliers.n] = new[i].outliers.os[k]; 
            accumulator->outliers.n++;
        }
    }
    accumulator->mean = sum_weighted_means / accumulator->n_samples;
    // accumulator->histogram.outliers = new[0].histogram.outliers; // just to text printing
}

void print_stats(Summary_stats* result)
{
    printf("Result {\n  N_samples: %luM\n  Min:  %4.3lf\n  Max:  %4.3lf\n  Mean: %4.3lf\n  Var:  %4.3lf\n}\n", result->n_samples/1000000, result->min, result->max, result->mean, result->variance);

    print_histogram(result->histogram.bins, result->histogram.n_bins, result->histogram.min, result->histogram.bin_width);

    printf("\nOutliers: ");
    for(int i=0; i<result->outliers.n; i++){
        printf("%lf, ", result->outliers.os[i]);
    }
    printf("\n");
    
    /*
    struct Outliers* o = &result->histogram.outliers;
    printf("First outlier node: { .is_last: %d, .next: %p, .val: %lf }\n", o->is_last, o->next, o->val);
    while(o->next != NULL && o->is_last != 1){
        o = o->next;
        printf("Additional outlier node: { .is_last: %d, .next: %p, .val: %lf }\n", o->is_last, o->next, o->val);

        printf("Slept for 1s");

    }*/
}

int sampler_finisterrae(Finisterrae_params finisterrae)
{
    // Histogram parameters: histogram_min, histogram_sup
    // START MPI ENVIRONMENT
    int mpi_id = 0, n_processes = 1;
    MPI_Status status;
    IF_MPI(MPI_Init(&argc, &argv));
    IF_MPI(MPI_Comm_size(MPI_COMM_WORLD, &n_processes));
    IF_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_id));

    /*
    Three levels:
    - individual_mpi_process_stats: each mpi process will have one of these
    - mpi_processes_stats_array:  mpi will later aggregate them into one array
    - aggregated_mpi_processes_stats: and we will reduce the array into one global stats again
    */

    Summary_stats individual_mpi_process_stats;
    Summary_stats* mpi_processes_stats_array = (Summary_stats*)malloc(n_processes * sizeof(Summary_stats));
    Summary_stats aggregated_mpi_processes_stats;

    int* aggregate_histogram_bins = (int*)calloc((size_t) finisterrae.histogram_n_bins, sizeof(int));
    Histogram aggregate_histogram = { .min = finisterrae.histogram_min, .sup = finisterrae.histogram_sup, .bin_width = finisterrae.histogram_bin_width, .n_bins = finisterrae.histogram_n_bins, .bins = aggregate_histogram_bins,};
    uint64_t* seed = malloc(sizeof(uint64_t)); *seed = UINT64_MAX / 2; double s = finisterrae.sampler(seed); free(seed);
    double* os = (double*)malloc((size_t) 100 * sizeof(double)); // to-do: start with a different number of outliers
    Outliers aggregate_histogram_outliers = { .os = os, .n = 0, .capacity = 100 };
    aggregated_mpi_processes_stats = (Summary_stats) { .n_samples = 1, .min = s, .max = s, .mean = s, .variance = 0, .histogram = aggregate_histogram,  .outliers = aggregate_histogram_outliers,  };

    // Get the number of threads
    int n_threads;
    #pragma omp parallel /* N: I don't quite understand what this directive is doing. */
    #pragma omp single
    {
        n_threads = omp_get_num_threads();
        printf("Num threads on process %d: %d\n", mpi_id, n_threads);
    }
    /*
    omp_set_dynamic(0);
    omp_set_num_threads(n_threads);
    // either get num threads or set num threads; either delete this statement or the omp_get_num_threads one
    */

    // Initialize seeds
    seed_cache_box* cache_box = (seed_cache_box*)malloc(sizeof(seed_cache_box) * (size_t)n_threads);

    int mpi_seed = mpi_id + 1; // +i*n_processes;
    srand(mpi_seed); /* Another alternative would be to distribute the seeds evenly, but I'm afraid that if I do this they'll end up somehow correlated */
    for (int thread_id = 0; thread_id < n_threads; thread_id++) {
        // Nuño to Jorge: you can't do this in parallel, since rand() is not thread safe
        cache_box[thread_id].seed = (uint64_t)rand() * (UINT64_MAX / RAND_MAX);
    }
    int n_samples = finisterrae.n_samples_per_process; /* these are per mpi process, distributed between threads  */
    double* xs = (double*)malloc((size_t)n_samples * sizeof(double));
    double* samples = (double*) malloc((size_t)n_samples * sizeof(double));
    if (samples == NULL) {
        fprintf(stderr, "Memory allocation for samples failed.\n");
        return 1;
    }
    // By persisting these variables rather than recreating them with each loop, we
    // 1. Get slightly better pseudo-randomness, I think, as the threads continue and we reduce our reliance on srand
    // 2. Become more slightly more efficient, as we don't have to call and free memory constantly

    for (int iter = 0;; iter++) {
        // Wait until the finisterrae allocator kills this

        // sampler_parallel(sample_cost_effectiveness_cser_bps_per_million, samples, n_threads, n_samples, mpi_id+1+i*n_processes);
        // do this inline instead of calling to the sampler_parallel function

        // One parallel loop to get the samples
        #pragma omp parallel for
        for (int j = 0; j < n_samples; j++) {
            int thread_id = omp_get_thread_num();
            // Can we get the minimum and maximum here? Not quite straightforwardly, because we have different threads operating independently
            xs[j] = finisterrae.sampler(&(cache_box[thread_id].seed));
        }

        // Initialize individual process stats struct
        int* individual_bins = (int*)calloc((size_t)finisterrae.histogram_n_bins, sizeof(int));
        Histogram individual_mpi_process_histogram = { .min = finisterrae.histogram_min, .sup= finisterrae.histogram_sup, .bin_width = finisterrae.histogram_bin_width, .n_bins = finisterrae.histogram_n_bins, .bins = individual_bins, };
        double* os = (double*)malloc((size_t) 100 * sizeof(double)); // to-do: start with a different number of outliers
        Outliers individual_mpi_histogram_outliers = { .os = os, .n = 0, .capacity = 100 };
        individual_mpi_process_stats = (Summary_stats) { 
            .n_samples = n_samples, 
            .min = xs[0], 
            .max = xs[0],
            .mean = 0.0, 
            .variance = 0.0,
            .histogram = individual_mpi_process_histogram, 
            .outliers = individual_mpi_histogram_outliers,
        };

        // One serial loop for mean, min, max & histogram
        // Possibly, these could be done more efficiently with openmp reductions, 
        // but I don't quite understand them 
        double mean = 0.0;
        for (int i = 0; i < n_samples; i++) { // do this serially to avoid race conditions 
            mean += xs[i];
            if (individual_mpi_process_stats.min > xs[i]) individual_mpi_process_stats.min = xs[i];
            if (individual_mpi_process_stats.max < xs[i]) individual_mpi_process_stats.max = xs[i];
            if (xs[i] < individual_mpi_process_stats.histogram.min || xs[i] >= individual_mpi_process_stats.histogram.sup) {
                if(individual_mpi_process_stats.outliers.n >= individual_mpi_process_stats.outliers.capacity){
                    int new_capacity = individual_mpi_process_stats.outliers.capacity * 2;
                    double* new_os = (double*)realloc(individual_mpi_process_stats.outliers.os, new_capacity * sizeof(double));
                    if (new_os == NULL) {
                        printf("Memory reallocation for outliers failed\n");
                        return 1;
                    }
                    individual_mpi_process_stats.outliers.os = new_os;
                    individual_mpi_process_stats.outliers.capacity = new_capacity;
                }
                individual_mpi_process_stats.outliers.os[individual_mpi_process_stats.outliers.n] = xs[i];
                individual_mpi_process_stats.outliers.n++;
                /*
                fprintf(stderr, "xs[i] outside histogram domain: %lf\n", xs[i]);
                FILE* fp = fopen("./outliers.txt", "a");
                fprintf(fp, "%lf\n", xs[i]);
                fclose(fp);
                */
           } else {
                double bin_double = (xs[i] - individual_mpi_process_stats.histogram.min)/individual_mpi_process_stats.histogram.bin_width;
                int bin_int = (int) floor(bin_double);
                individual_mpi_process_stats.histogram.bins[bin_int]++;
            }
        }
        individual_mpi_process_stats.mean = mean/n_samples;

        // One parallel loop for variance
        double var = 0.0;
        #pragma omp parallel for simd reduction(+:var) // unclear if the for simd reduction applies after we've added other items to the for loop
        for (int i = 0; i < n_samples; i++) {
            var += (xs[i] - individual_mpi_process_stats.mean) * (xs[i] - individual_mpi_process_stats.mean);
        }
        individual_mpi_process_stats.variance = var / n_samples;

        /*
        for (int i=0; i<n_processes; i++){
          if (mpi_id==i)
              print_stats(&chunk_stats);
          MPI_Barrier(MPI_COMM_WORLD);
        }
        */

        IF_MPI(MPI_Gather(&individual_mpi_process_stats, sizeof(Summary_stats), MPI_CHAR, mpi_processes_stats_array, sizeof(Summary_stats), MPI_CHAR, 0, MPI_COMM_WORLD));
        IF_NO_MPI(mpi_processes_stats_array[0] = individual_mpi_process_stats);

        if (mpi_id == 0) {
            reduce_chunk_stats(&aggregated_mpi_processes_stats, mpi_processes_stats_array, n_processes);
            if (iter % finisterrae.print_every_n_iters == 0){
                printf("\nIter %3d:\n", iter);
                print_stats(&aggregated_mpi_processes_stats);
            }
        }
    }
    free(cache_box); // should never be reached, really
    return 0;
}

int main(int argc, char** argv)
{
    sampler_finisterrae((Finisterrae_params) {
        .sampler = sample_cost_effectiveness_cser_bps_per_million, 
        .n_samples_per_process = N_SAMPLES_PER_PROCESS,
        .histogram_min = 0,
        .histogram_sup = 200,
        .histogram_bin_width = 1,
        .histogram_n_bins = 200,
        .print_every_n_iters = 10,
    });
    return 0;
}
