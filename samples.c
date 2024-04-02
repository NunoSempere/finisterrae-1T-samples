#include "model.h"
#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"
#include <float.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

/* Macro trick to be able to test part of the code in a system without MPI */
// #define NO_MPI
#ifdef NO_MPI
#define IF_MPI(x)
#define IF_NO_MPI(x) x
#define MPI_Status int
#else
#include "mpi.h" /* N: why is this "mpi.h" and not <mpi.h> ??? */
#define IF_MPI(x) x
#define IF_NO_MPI(x)
#endif

/* Structs */

#define CACHE_LINE_SIZE 64
typedef struct _seed_cache_box {
    uint64_t seed;
    char padding[CACHE_LINE_SIZE - sizeof(uint64_t)];
    // Cache line size is 64 *bytes*, uint64_t is 64 *bits* (8 bytes). Different units!
} seed_cache_box;

typedef struct _Histogram {
    double min;
    double max;
    double interval_size;
    double* histogram_ticks;
    double* histogram_counts;
} Histogram;

typedef struct _Summary_stats {
    uint64_t n_samples;
    double min;
    double max;
    double mean;
    double variance;
    Histogram histogram;
} Summary_stats;

Summary_stats get_chunk_stats_from_one_sample_of_sampler(double (*sampler)(uint64_t* seed), uint64_t* seed)
{

    double s = sampler(seed);
    Summary_stats stats_of_one = { .n_samples = 1, .min = s, .max = s, .mean = s, .variance = 0 };
    return stats_of_one;
}

double combine_variances(Summary_stats* x, Summary_stats* y)
{
    double term1 = ((x->n_samples) * x->variance + (y->n_samples) * y->variance) / (x->n_samples + y->n_samples);
    double term2 = (x->n_samples * y->n_samples * (x->mean - y->mean) * (x->mean - y->mean)) / ((x->n_samples + y->n_samples) * (x->n_samples + y->n_samples));
    double result = term1 + term2;
    return result;
    // https://math.stackexchange.com/questions/2971315/how-do-i-combine-standard-deviations-of-two-groups
    // but use the standard deviation of the samples, no the estimate of the standard deviation of the underlying distribution
}

void reduce_chunk_stats(Summary_stats* accumulator, Summary_stats* cs, int n_chunks)
{

    double sum_weighted_means = accumulator->mean * accumulator->n_samples;
    for (int i = 0; i < n_chunks; i++) {
        sum_weighted_means += cs[i].mean * cs[i].n_samples;
        accumulator->n_samples += cs[i].n_samples;
        accumulator->mean = sum_weighted_means / accumulator->n_samples; // need this for the variance calculations
        accumulator->variance = combine_variances(accumulator, cs + i);
        if (accumulator->min > cs[i].min) accumulator->min = cs[i].min;
        if (accumulator->max < cs[i].max) accumulator->max = cs[i].max;
        for (int j = 0; j < 1000; j++) {
            accumulator->histogram.histogram_counts[j] += cs[i].histogram.histogram_counts[j];
        }
    }
    accumulator->mean = sum_weighted_means / accumulator->n_samples;
}

void print_stats(Summary_stats* result)
{
    printf("Result {\n  N_samples: %lu\n  Min:  %4.3lf\n  Max:  %4.3lf\n  Mean: %4.3lf\n  Var:  %4.3lf\n}\n", result->n_samples, result->min, result->max, result->mean, result->variance);
}

Summary_stats sampler_finisterrae(double (*sampler)(uint64_t* seed))
{
    // Function modelled after the sampler_parallel

    // START MPI ENVIRONMENT
    int mpi_id = 0, n_processes = 1;
    MPI_Status status;
    IF_MPI(MPI_Init(&argc, &argv));
    IF_MPI(MPI_Comm_size(MPI_COMM_WORLD, &n_processes));
    IF_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_id));

    Summary_stats individual_mpi_process_stats; // each mpi process will have one of these
    Summary_stats* mpi_processes_stats_array; // mpi will later aggregate them into one array
    Summary_stats aggregated_mpi_processes_stats; // and we will reduce the array into one global stats again

    mpi_processes_stats_array = (Summary_stats*)malloc(n_processes * sizeof(Summary_stats));
    // Use a seed to initialize this global variable
    uint64_t* seed = malloc(sizeof(uint64_t));
    *seed = UINT64_MAX / 2;
    double s = sampler(seed);
    double* histogram_counts = (double*)calloc((size_t)1000, sizeof(double));
    Histogram aggregate_histogram = { .min = 0, .max = 1000, .interval_size = 1, .histogram_counts = histogram_counts };
    Summary_stats _aggregated_mpi_processes_stats = { .n_samples = 1, .min = s, .max = s, .mean = s, .variance = 0, .histogram = aggregate_histogram };
    aggregated_mpi_processes_stats = _aggregated_mpi_processes_stats;
    free(seed);

    // Get the number of threads
    int n_threads;
    #pragma omp parallel /* N: I don't quite understand what this directive is doing. */
    #pragma omp single
    {
        n_threads = omp_get_num_threads();
        // printf("Num threads on process %d: %d\n", mpi_id, n_threads);
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
    int n_samples = 1000000; /* these are per mpi process, distributed between threads  */
    double* xs = (double*)malloc((size_t)n_samples * sizeof(double));
    // By persisting these variables rather than recreating them with each loop, we
    // 1. Get slightly better pseudo-randomness, I think, as the threads continue and we reduce our reliance on srand
    // 2. Become more slightly more efficient, as we don't have to call and free memory constantly

    for (int i = 0;; i++) {
        // Wait until the finisterrae allocator kills this

        // sampler_parallel(sample_cost_effectiveness_cser_bps_per_million, samples, n_threads, n_samples, mpi_id+1+i*n_processes);
        // do this inline instead of calling to the sampler_parallel function

        #pragma omp parallel for
        for (int j = 0; j < n_samples; j++) {
            int thread_id = omp_get_thread_num();
            // Can we get the minimum and maximum here?
            xs[j] = sampler(&(cache_box[thread_id].seed));
        }

        // save stats to a struct so that they can be aggregated
        individual_mpi_process_stats.n_samples = n_samples;
        individual_mpi_process_stats.mean = array_mean(xs, n_samples); // we could also parallelize this
        double var = 0.0;
        individual_mpi_process_stats.min = xs[0];
        individual_mpi_process_stats.max = xs[0];
        double* individual_histogram_counts = (double*)calloc((size_t)1000, sizeof(double));
        Histogram _histogram = {
            .min = 0,
            .max = 1000,
            .interval_size = 1,
            .histogram_counts = individual_histogram_counts,
        };
        individual_mpi_process_stats.histogram = _histogram;

        #pragma omp parallel for simd reduction(+:var) // unclear if the for simd reduction applies after we've added other items to the for loop
        for (uint64_t i = 0; i < n_samples; i++) {
            var += (xs[i] - individual_mpi_process_stats.mean) * (xs[i] - individual_mpi_process_stats.mean);
            if (individual_mpi_process_stats.min > xs[i]) individual_mpi_process_stats.min = xs[i];
            if (individual_mpi_process_stats.max < xs[i]) individual_mpi_process_stats.max = xs[i];
            if (xs[i] < individual_mpi_process_stats.histogram.min || xs[i] > individual_mpi_process_stats.histogram.max) {
                printf("xs[i] outside histogram domain: %lf", xs[i]);
            } else {
                double rounded = floor(xs[i]);
                individual_mpi_process_stats.histogram.histogram_counts[(int)rounded]++;
            }
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
            printf("\nIter %3d:\n", i);
            print_stats(&aggregated_mpi_processes_stats);
        }
    }
    free(cache_box); // should never be reached, really
}

int main(int argc, char** argv)
{
    sampler_finisterrae(sample_cost_effectiveness_cser_bps_per_million);
    return 0;
}
