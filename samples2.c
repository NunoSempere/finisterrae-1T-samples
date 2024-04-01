#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "mpi.h" /* N: why is this "mpi.h" and not <mpi.h> ??? */
#include <omp.h>
#include "model.h"

typedef struct _Chunk_stats {
    uint64_t n_samples;
    double min;
    double max;
    double mean;
    double variance;
} Chunk_stats;

Chunk_stats get_chunk_stats_from_one_sample_of_sampler(double (*sampler)(uint64_t* seed), uint64_t* seed){

  double s = sampler(seed);
  Chunk_stats stats_of_one = {.n_samples=1, .min=s , .max=s , .mean=s, .variance=0};
  return stats_of_one; 

}

Chunk_stats sampler_finisterrae(double (*sampler)(uint64_t* seed)){
  // Function modelled after the sampler_parallel

  //START MPI ENVIRONMENT
  int mpi_id, n_processes;
  MPI_Status status;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &n_processes);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_id);

  Chunk_stats individual_mpi_process_stats;   // each mpi process will have one of these
  Chunk_stats* mpi_processes_stats_array;     // mpi will later aggregate them into one array
  Chunk_stats aggregated_mpi_processes_stats; // and we will reduce the array into one global stats again

  mpi_processes_stats_array = (Chunk_stats*) malloc(n_processes * sizeof(Chunk_stats)); 
  uint64_t* seed = malloc(sizeof(uint64_t)); *seed = UINT64_MAX/2;
  aggregated_mpi_processes_stats = get_chunk_stats_from_one_sample_of_sampler(sampler, seed); 
  free(seed);

  // Get the number of threads
  #pragma omp parallel /* N: I don't understand what this directive is doing */
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
  Chunk_stats* omp_thread_stats_array = (Chunk_stats*) malloc(sizeof(Chunk_stats) * (size_t)n_threads)

  int mpi_seed = mpi_id+1+i*n_processes;
  srand(mpi_seed);
  for (int thread_id = 0; thread_id < n_threads; thread_id++) {
    // Nuño to Jorge: you can't do this in parallel, since rand() is not thread safe
    cache_box[thread_id].seed = (uint64_t)rand() * (UINT64_MAX / RAND_MAX);
    omp_thread_stats_array[i] = get_chunk_stats_from_one_sample_of_sampler(sampler, cache_box[thread_id].seed);
  }
  // By persisting these variables rather than recreating them with each loop, we
  // 1. Get slightly better pseudo-randomness, I think, as the threads continue and we reduce our reliance on srand
  // 2. Become more slightly more efficient, as we don't have to call and free memory constantly 

  int n_samples = 1000000; /* these are per mpi process, distributed between threads  */ 
  double* samples = (double*) malloc((size_t)n_samples * sizeof(double));
  for(int i=0; ; i++){
    // Wait until the finisterrae allocator kills this
    
    // sampler_parallel(sample_cost_effectiveness_cser_bps_per_million, samples, n_threads, n_samples, mpi_id+1+i*n_processes);
    // do this inline instead of calling to the sampler_parallel function

    int i;
    #pragma omp parallel private(i)
    {
        int thread_id = omp_get_thread_num();
        #pragma omp for
        for (i = 0; i < n_samples; i++) {
            double samples[i] = sampler(&(cache_box[thread_id].seed));

            /* 
            Here we do something with results. 
            Some cool ideas which are impractical: 
            - Aggregate these sample by sample. It's a cool concept, but I think we loose too much floating point precision
            - If we were *very* memory constrained, we could generate results once to get the mean, and then *generate them again from the same seed* to get the variance. 
            - 
            */
        }
    }

    // save stats to a struct so that they can be aggregated
    individual_mpi_process_stats->n_samples = n_samples;
    individual_mpi_process_stats->mean = array_mean(xs, n_samples);
    double var = 0.0;
    individual_mpi_process_stats->min = xs[0];
    individual_mpi_process_stats->max = xs[0];
    #pragma omp parallel for simd reduction(+:var)
    for (uint64_t i = 0; i < n_samples; i++) {
        var += (xs[i] - individual_mpi_process_stats->mean) * (xs[i] - individual_mpi_process_stats->mean);
        if(individual_mpi_process_stats->min > xs[i]) individual_mpi_process_stats->min = xs[i];
        if(individual_mpi_process_stats->max < xs[i]) individual_mpi_process_stats->max = xs[i];
    }
    individual_mpi_process_stats->variance = var / n_samples;

    /*    
    for (int i=0; i<n_processes; i++){
      if (mpi_id==i)
          print_stats(&chunk_stats);
      MPI_Barrier(MPI_COMM_WORLD);
    }
    */

    MPI_Gather(&individual_mpi_process_stats, sizeof(Chunk_stats), MPI_CHAR, mpi_processes_stats_array, sizeof(Chunk_stats), MPI_CHAR, 0, MPI_COMM_WORLD);

    if (mpi_id==0) {
        reduce_chunk_stats(&aggregated_mpi_processes_stats, mpi_processes_stats_array, n_processes);
        printf("\nIter %3d:\n", i);
        print_stats(&aggregated_mpi_processes_stats);
    }
  }
  free(cache_box); // should never be reached, really 

}


int main(int argc,char **argv){
  sampler_finisterrae();
  return 0;
}
