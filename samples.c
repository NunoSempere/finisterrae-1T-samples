#include <float.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "model.h"
#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"

// Macro to be able to run part of the code in a system without MPI
// #define NO_MPI // Comment out if running on an MPI system
#ifdef NO_MPI
#define IF_MPI(x)
#define IF_NO_MPI(x) x
#define MPI_Status int
#define N_SAMPLES_PER_PROCESS MILLION
#else
#include "mpi.h" /* N: why is this "mpi.h" and not <mpi.h> ??? */
#define IF_MPI(x) x
#define IF_NO_MPI(x)
#define N_SAMPLES_PER_PROCESS BILLION
#define uint64_t u_int64_t
#endif

/* Collect outliers manually? */
#define COLLECT_OUTLIERS 0

/* External interface struct */
typedef struct _Finisterrae_params {
    const double (*sampler)(uint64_t* seed);
    const uint64_t n_samples_per_process;
    const uint64_t n_samples_total;
    const double histogram_min;
    const double histogram_sup;
    const double histogram_bin_width;
    const int histogram_n_bins;
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
    int n_bins;
    uint64_t* bins;
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
    double n = (double)(x->n_samples + y->n_samples);
    double term1 = (x->variance * ((double)x->n_samples / n)) + (y->variance * ((double)y->n_samples / n));
    double term2 = (((double)x->n_samples / n) * ((double)y->n_samples / n)) * (x->mean - y->mean) * (x->mean - y->mean);
    double result = term1 + term2;
    return result;
    // https://math.stackexchange.com/questions/2971315/how-do-i-combine-standard-deviations-of-two-groups
    // but use the standard deviation of the samples, not the estimate of the standard deviation of the underlying distribution
}

void reduce_chunk_stats(Summary_stats* accumulator, Summary_stats* new, int n_chunks)
{
    double sum_weighted_means = accumulator->mean * accumulator->n_samples;
    for (int i = 0; i < n_chunks; i++) {
        sum_weighted_means += new[i].mean* new[i].n_samples;
        accumulator->n_samples += new[i].n_samples;
        accumulator->mean = sum_weighted_means / accumulator->n_samples; // need this for the variance calculations
        accumulator->variance = combine_variances(accumulator, new + i);
        if (accumulator->min > new[i].min) accumulator->min = new[i].min;
        if (accumulator->max < new[i].max) accumulator->max = new[i].max;
        for (int j = 0; j < accumulator->histogram.n_bins; j++) {
            accumulator->histogram.bins[j] += new[i].histogram.bins[j];
        }
        if (COLLECT_OUTLIERS) {
            if (accumulator->outliers.n + new[i].outliers.n >= accumulator->outliers.capacity) {
                int new_capacity = accumulator->outliers.capacity * 2;
                double* new_os = (double*)realloc(accumulator->outliers.os, new_capacity * sizeof(double));
                if (new_os == NULL) {
                    printf("Memory reallocation for outliers failed\n");
                    return;
                }
                accumulator->outliers.os = new_os;
                accumulator->outliers.capacity = new_capacity;
            }
            for (int k = 0; k < new[i].outliers.n && accumulator->outliers.n < accumulator->outliers.capacity; k++) {
                accumulator->outliers.os[accumulator->outliers.n] = new[i].outliers.os[k];
                accumulator->outliers.n++;
            }
        }
    }
    accumulator->mean = sum_weighted_means / accumulator->n_samples;
}

void print_stats(Summary_stats* result)
{
    printf("Result {\n  N_samples: %luM\n  Min:  %15.10lf\n  Max:  %15.10lf\n  Mean: %15.10lf\n  Var:  %15.10lf\n}\n", result->n_samples / MILLION, result->min, result->max, result->mean, result->variance);

    print_histogram(result->histogram.bins, result->histogram.n_bins, result->histogram.min, result->histogram.bin_width);

    if (COLLECT_OUTLIERS) {
        printf("\nOutliers: ");
        if (result->outliers.n == 0) {
            printf("Ø");
        }
        for (int i = 0; i < result->outliers.n; i++) {
            printf("%lf, ", result->outliers.os[i]);
        }
        printf("\n");
    }
}

int sampler_finisterrae(Finisterrae_params finisterrae)
{
    // Histogram parameters: histogram_min, histogram_sup
    // START MPI ENVIRONMENT
    int mpi_id = 0, n_processes = 1;
    MPI_Status status;
    IF_MPI(MPI_Init(NULL, NULL));
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

    uint64_t* aggregate_histogram_bins = (uint64_t*)calloc((size_t)finisterrae.histogram_n_bins, sizeof(uint64_t));
    Histogram aggregate_histogram = {
        .min = finisterrae.histogram_min,
        .sup = finisterrae.histogram_sup,
        .bin_width = finisterrae.histogram_bin_width,
        .n_bins = finisterrae.histogram_n_bins,
        .bins = aggregate_histogram_bins,
    };
    uint64_t* seed = malloc(sizeof(uint64_t));
    *seed = UINT64_MAX / 2 + mpi_id;
    double s = finisterrae.sampler(seed);
    free(seed);
    double* os = NULL;
    if (COLLECT_OUTLIERS) {
        os = (double*)malloc((size_t)100 * sizeof(double));
    }
    Outliers aggregate_histogram_outliers = { .os = os, .n = 0, .capacity = 100 };
    aggregated_mpi_processes_stats = (Summary_stats) {
        .n_samples = 1,
        .min = s,
        .max = s,
        .mean = s,
        .variance = 0.0,
        .histogram = aggregate_histogram,
        .outliers = aggregate_histogram_outliers,
    };
    // Get the number of threads
    int n_threads;
    #pragma omp parallel // Create a parallel environment to see how many threads are in it
    #pragma omp single // But only print it one time in that paralllel region
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
    double* samples = (double*)malloc((size_t)n_samples * sizeof(double));
    if (samples == NULL) {
        fprintf(stderr, "Memory allocation for samples failed.\n");
        return 1;
    }
    // By persisting these variables rather than recreating them with each loop, we
    // 1. Get slightly better pseudo-randomness, I think, as the threads continue and we reduce our reliance on srand
    // 2. Become more slightly more efficient, as we don't have to call and free memory constantly

    uint64_t* all_bins = (uint64_t*)calloc(finisterrae.histogram_n_bins * n_processes, sizeof(uint64_t));
    for (uint64_t i = 0; i < finisterrae.n_samples_total / (finisterrae.n_samples_per_process*(uint64_t)n_processes)+(uint64_t)1 ; i++) {
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
        uint64_t* individual_mpi_process_histogram_bins = (uint64_t*)calloc((size_t)finisterrae.histogram_n_bins, sizeof(uint64_t));
        Histogram individual_mpi_process_histogram = {
            .min = finisterrae.histogram_min,
            .sup = finisterrae.histogram_sup,
            .bin_width = finisterrae.histogram_bin_width,
            .n_bins = finisterrae.histogram_n_bins,
            .bins = individual_mpi_process_histogram_bins,
        };
        double* os = NULL;
        if (COLLECT_OUTLIERS) {
            os = (double*)malloc((size_t)100 * sizeof(double));
        }
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
        double min = DBL_MAX; // Use float.h's DBL_MAX for initialization
        double max = -DBL_MAX;

	#pragma omp parallel for reduction (+ : mean) reduction(min : min) reduction(max : max) 
        for (int k = 0; k < n_samples; k++) {
            mean += xs[k];
            if (min > xs[k]) min = xs[k];
            if (max < xs[k]) max = xs[k];
        }
	for (int k=0; k < n_samples; k++) { // do this serially to avoid race conditions
	    if (COLLECT_OUTLIERS && (xs[k] < individual_mpi_process_stats.histogram.min || xs[k] >= individual_mpi_process_stats.histogram.sup)) {
                if (individual_mpi_process_stats.outliers.n >= individual_mpi_process_stats.outliers.capacity) {
                    int new_capacity = individual_mpi_process_stats.outliers.capacity * 2;
                    double* new_os = (double*)realloc(individual_mpi_process_stats.outliers.os, new_capacity * sizeof(double));
                    if (new_os == NULL) {
                        printf("Memory reallocation for outliers failed\n");
                        return 1;
                    }
                    individual_mpi_process_stats.outliers.os = new_os;
                    individual_mpi_process_stats.outliers.capacity = new_capacity;
                }
                individual_mpi_process_stats.outliers.os[individual_mpi_process_stats.outliers.n] = xs[k];
                individual_mpi_process_stats.outliers.n++;
            } else {
                double bin_double = (xs[k] - individual_mpi_process_stats.histogram.min) / individual_mpi_process_stats.histogram.bin_width;
                int bin_int = (int)floor(bin_double);
                individual_mpi_process_stats.histogram.bins[bin_int]++;
            }
        }
        individual_mpi_process_stats.mean = mean / n_samples;
	    individual_mpi_process_stats.min = min;
	    individual_mpi_process_stats.max = max;
        // One parallel loop for variance
        double var = 0.0;
        #pragma omp parallel for simd reduction(+ : var) // unclear if the for simd reduction applies after we've added other items to the for loop
        for (int m = 0; m < n_samples; m++) {
            var += (xs[m] - individual_mpi_process_stats.mean) * (xs[m] - individual_mpi_process_stats.mean);
        }
        individual_mpi_process_stats.variance = var / n_samples;

        /*
        for (int i=0; i<n_processes; i++){
          if (mpi_id==i)
              print_stats(&chunk_stats);
        }
        */

        IF_MPI(MPI_Barrier(MPI_COMM_WORLD));
        IF_MPI(MPI_Gather(&individual_mpi_process_stats, sizeof(Summary_stats), MPI_CHAR, mpi_processes_stats_array, sizeof(Summary_stats), MPI_CHAR, 0, MPI_COMM_WORLD));
        IF_MPI(MPI_Gather(individual_mpi_process_stats.histogram.bins, finisterrae.histogram_n_bins * sizeof(uint64_t), MPI_CHAR, all_bins, finisterrae.histogram_n_bins * sizeof(uint64_t), MPI_CHAR, 0, MPI_COMM_WORLD));

        IF_NO_MPI(mpi_processes_stats_array[0] = individual_mpi_process_stats);

        if (mpi_id == 0) {
            for (int p = 0; p < n_processes; p++) {
                IF_MPI(mpi_processes_stats_array[p].histogram.bins = all_bins + p * finisterrae.histogram_n_bins);
                // print_stats(mpi_processes_stats_array+p);
            }
            reduce_chunk_stats(&aggregated_mpi_processes_stats, mpi_processes_stats_array, n_processes);
            if (i % finisterrae.print_every_n_iters == 0) {
                printf("\nIter %3ld:\n", i);
                print_stats(&aggregated_mpi_processes_stats);
            }
        }
    }
    free(cache_box); // should never be reached, really
    free(all_bins);

	if (mpi_id == 0) {
		printf("\nLast iter:\n");
		print_stats(&aggregated_mpi_processes_stats);
	}

	return 0;
}

int main(int argc, char** argv)
{
    sampler_finisterrae((Finisterrae_params) {
        .sampler = sample_cost_effectiveness_sentinel_bps_per_million,
        .n_samples_per_process = (uint64_t)1 * BILLION,
        .n_samples_total = (uint64_t)1 * TRILLION,
        .histogram_min = 0,
        .histogram_sup = 300,
        .histogram_bin_width = 1,
        .histogram_n_bins = 300,
        .print_every_n_iters = 20,
    });
    // Two types of histogram:
    // 1. Exploring the main part of the distribution
    // 2. Exploring the long tail.
    // We are interested in both, but for the 1T samples we are interested in the long tail
    /* For main part of the distribution:
    sampler_finisterrae((Finisterrae_params) {
        .sampler = sample_cost_effectiveness_sentinel_bps_per_million,
        .n_samples_per_process = N_SAMPLES_PER_PROCESS,
        .histogram_min = 0,
        .histogram_sup = 1,
        .histogram_bin_width = 0.01,
        .histogram_n_bins = 100,
        .print_every_n_iters = 10,
    });
    */
    return 0;
}
