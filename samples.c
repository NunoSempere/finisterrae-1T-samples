#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "mpi.h"
#include <omp.h>

/* Definitions */
#define MILLION (1000 * 1000)
#define AMERICAN_BILLION (1000 * 1000 * 1000)
#define AMERICAN_HUNDRED_BILLION (100 * AMERICAN_BILLION)

/* Utils */
double sample_0(uint64_t * seed) { UNUSED(seed); return 0; }
double sample_1(uint64_t * seed) { UNUSED(seed); return 1; }

/* Existential risk in the UK */
double sample_ai_existential_risk_uk(uint64_t * seed){
  double p_transformative_ai = sample_beta(26.5528563290474, 1.7763655277372, seed); // 85% to 99%
  double p_existential_conditional_transformative_ai = sample_beta(2.27126465410532, 22.5911969267949, seed); // 2% to 20%; mean 9%
  double p_existential_ai = p_transformative_ai * p_existential_conditional_transformative_ai;
  double fraction_uk = sample_beta(4.05715325375548, 25.7675530361467, seed); 
  double p_existential_ai_uk = p_transformative_ai * p_existential_conditional_transformative_ai * fraction_uk;
  return p_existential_ai_uk;
}

double sample_conditional_impact_cser(uint64_t * seed){
  double share_uk_risk_reduced_uk_community = sample_beta(1.86052521618428, 2.46603754984314, seed); // 10% to 80%
  double fraction_cser = sample_beta(2.53017245520986, 7.36092868549274, seed); // 7% to 50%
  double conditional_impact_cser = share_uk_risk_reduced_uk_community * fraction_cser;
  return conditional_impact_cser;
}


double sample_value_if_not_minuscule(uint64_t * seed) {
    double fraction_if_not_minuscule = sample_beta(1.9872200324266, 6.36630125578423, seed);
    // ^ 90% confidence interval: 0.05 to 0.5, i.e., 5% to 50%
    return fraction_if_not_minuscule;
}

double sample_relative_value_of_less_valuable_part_of_cser(uint64_t * seed){
    // relative to the value of the more valuable part

    double p_minuscule = 0.2;
    double p_not_minuscule = 0.8;

    int n_dists = 2;
    double weights[] = { p_minuscule, p_not_minuscule };
    double (*samplers[])(uint64_t*) = { sample_0, sample_value_if_not_minuscule };
    double relative_value_of_less_valuable_part_of_cser = sample_mixture(samplers, weights, n_dists, seed);
    return relative_value_of_less_valuable_part_of_cser;
}

double sample_fraction_funged_if_neither_zero_nor_full_funging(uint64_t* seed){
    return sample_beta(2.23634269185645, 3.73532102339597, seed); // 10% to 70% 
}

double sample_fraction_funged_within_cser(uint64_t * seed){
    // Chance that it fully funges
    double p_full_funge = 0.2;

    // Chance that it's not funged at all
    double p_no_funge = 0.05;

    // Degree of funging if it's funged.
    double p_some_funging = 1 - p_full_funge - p_no_funge;

    int n_dists = 3;
    double weights[]  = {p_full_funge, p_no_funge, p_some_funging};
    double (*samplers[])(uint64_t*) = { sample_1, sample_0, sample_fraction_funged_if_neither_zero_nor_full_funging};

    double fraction_funged_within_cser = sample_mixture(samplers, weights, n_dists, seed);
    return fraction_funged_within_cser;
}

double sample_relative_value_after_funging_within_cser(uint64_t* seed){
    double fraction_funged_within_cser = sample_fraction_funged_within_cser(seed);
    double relative_value = fraction_funged_within_cser * sample_relative_value_of_less_valuable_part_of_cser(seed) +
        (1-fraction_funged_within_cser) * 1; 
    return relative_value;
}

double sample_relative_value_after_funging_with_open_philanthropy(uint64_t* seed){
    double p_chance_op_would_fund_if_sogive_doesnt_fund = sample_beta(3,8, seed); 
    double value_open_phil_marginal_grant = sample_to(0.1, 2, seed);
    double result = p_chance_op_would_fund_if_sogive_doesnt_fund * value_open_phil_marginal_grant +
        (1-p_chance_op_would_fund_if_sogive_doesnt_fund);
    return result;
}

double sample_relative_value_after_funging(uint64_t * seed){
    double value_after_funging_within_cser = sample_relative_value_after_funging_within_cser(seed);
    return value_after_funging_within_cser;
}

double sample_relative_value_current_personnel(uint64_t * seed) { UNUSED(seed); return 1; }
double sample_relative_value_marginal_new_person(uint64_t * seed){ return sample_beta(3,2, seed); }
double sample_value_cser_bps_no_funging(uint64_t * seed){
    double ai_existential_risk_uk = sample_ai_existential_risk_uk(seed);
    double conditional_impact_cser = sample_conditional_impact_cser(seed);

    double value_cser_bps_uk = ai_existential_risk_uk * conditional_impact_cser / (0.01/100);

    double international_influence_multiplier = 1 + sample_beta(1.9872200324266, 6.36630125578423, seed); // 5% to 50%
    double value_cser_bps_international = value_cser_bps_uk * international_influence_multiplier;

    int n_dists = 2;
    double weights[] = { 0.2, 0.8 };
    double (*samplers[])(uint64_t*) = { sample_relative_value_current_personnel, sample_relative_value_marginal_new_person};
    double marginal_room_for_funding_multiplier = sample_mixture(samplers, weights, n_dists, seed);
    double value_cser_bps = value_cser_bps_international * marginal_room_for_funding_multiplier;
    return value_cser_bps; 
}

double sample_value_cser_bps(uint64_t * seed){

    double value_cser_bps = sample_value_cser_bps_no_funging(seed) * sample_relative_value_after_funging(seed);
    return value_cser_bps; 
}


double sample_cost_effectiveness_cser_bps_per_million_no_funging(uint64_t * seed){
    double cost_cser_millions = 6 * 5 * 0.1; 
    // 6 people/year for 100k each for 5 years
    // this is the AI part of CSER
    // keep as constant for now before getting more data 
    double value_cser_bps_per_million = sample_value_cser_bps_no_funging(seed)/cost_cser_millions;
    return value_cser_bps_per_million;
}

double sample_cost_effectiveness_cser_bps_per_million(uint64_t * seed){
    double cost_cser_millions = 6 * 5 * 0.1; 
    // 6 people/year for 100k each for 5 years
    // this is the AI part of CSER
    // keep as constant for now before getting more data 
    double value_cser_bps_per_million = sample_value_cser_bps(seed)/cost_cser_millions;
    return value_cser_bps_per_million;
}

void sampler_memory_efficient(double (*sampler)(uint64_t* seed), int n_bins, int n_samples){
    // sample_parallel stores results in a giant array
    // But this is not workable for a trillion samples

    double mean = 0;
    double std = 0;
    double max_value = -DBL_MAX;
    double min_value = DBL_MAX;
    uint64_t* seed = malloc(sizeof(uint64_t));
    *seed = UINT64_MAX/2; // xorshift can't start with 0
    int *bins = (int*) calloc((size_t)n_bins, sizeof(int));
    if (bins == NULL) {
        fprintf(stderr, "Memory allocation for bins failed.\n");
        return;
    }

    /* Do first run to get mean & min & max */
    for(int i=0; i<n_samples; i++){
        double s = sampler(seed);
        mean+=s;
        if(max_value < s){
            max_value = s;
        }
        if(min_value > s){
            min_value = s;
        }
    }

    mean = mean/n_samples;
    if (min_value == max_value) { // avoid division by 0
        max_value++;
    }
    double range = max_value - min_value;
    double bin_width = range / n_bins;

    /* Do second pass */
    *seed = UINT64_MAX/2; // go back to the beginning
    for(int i=0; i<n_samples; i++){
        double s = sampler(seed);
        std += (s - mean) * (s - mean);

        int bin_index = (int)((s - min_value) / bin_width);
        if (bin_index == n_bins) {
            bin_index--; // Last bin includes max_value
        }
        bins[bin_index]++;
    }
    std = sqrt(std/n_samples);

    // Print results
    printf("Mean: %lf\n"
           " Std: %lf\n",
           mean, std);
    print_histogram(bins, n_bins, min_value, bin_width);

    free(bins);
    return;

}

typedef struct _Chunk_stats {
    uint64_t n_samples;
    double min;
    double max;
    double mean;
    double variance;
} Chunk_stats;

void save_chunk_stats_to_struct (Chunk_stats* chunk_stats, double* xs, int n_samples) {
    chunk_stats->n_samples = n_samples;
    chunk_stats->mean = array_mean(xs, n_samples);
    double var = 0.0;
    chunk_stats->min = xs[0];
    chunk_stats->max = xs[0];
    #pragma omp parallel for simd reduction(+:var)
    for (uint64_t i = 0; i < n_samples; i++) {
        var += (xs[i] - chunk_stats->mean) * (xs[i] - chunk_stats->mean);
        if(chunk_stats->min > xs[i]) chunk_stats->min = xs[i];
        if(chunk_stats->max < xs[i]) chunk_stats->max = xs[i];
    }
    chunk_stats->variance = var / n_samples;
}

inline double combine_variances (Chunk_stats *x, Chunk_stats *y) {
    double term1 = ((x->n_samples) * x->variance + (y->n_samples)*y->variance)/(x->n_samples + y->n_samples) ;
    double term2 = (x->n_samples * y->n_samples * (x->mean - y->mean)*(x->mean - y->mean)) / ((x->n_samples + y->n_samples) * (x->n_samples + y->n_samples));
    double result = term1 + term2;
    return result;
    // https://math.stackexchange.com/questions/2971315/how-do-i-combine-standard-deviations-of-two-groups
    // but use the standard deviation of the samples, no the estimate of the standard deviation of the underlying distribution
}

void reduce_chunk_stats (Chunk_stats* output, Chunk_stats* cs, int n_chunks) {
    
    double sum_weighted_means = output->mean * output->n_samples;
    for (int i=0; i<n_chunks; i++) {
        sum_weighted_means += cs[i].mean * cs[i].n_samples;
        output->n_samples += cs[i].n_samples;
        output->mean = sum_weighted_means / output->n_samples; // need this for the variance calculations
        output->variance = combine_variances(output, cs+i);
        if(output->min > cs[i]) output->min = cs[i];
        if(output->max < cs[i]) output->max = cs[i];
    }
    output->mean = sum_weighted_means / output->n_samples;
}

void print_stats(Chunk_stats* result) {
    printf("Result {\n  N_samples: %lu\n  Min:  %4.3lf\n  Max:  %4.3lf\n  Mean: %4.3lf\n  Var:  %4.3lf\n}\n", result->n_samples, result->min, result->max, result->mean, result->variance);
}


int main(int argc,char **argv)
{
    uint64_t n_samples = 100000000; //25000000000;// 25000* MILLION; If I write it in this fashion (25000* MILLION), compiler whines
    double* samples = (double*) malloc((size_t)n_samples * sizeof(double));
    int n_threads = 64;

    //START MPI ENVIRONMENT
    int mpi_id, npes;
    MPI_Status status;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &npes);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_id);


    // Get one sample
    uint64_t* seed = malloc(sizeof(uint64_t));
    *seed = UINT64_MAX/2;
    double s = sample_cost_effectiveness_cser_bps_per_million(seed);
    Chunk_stats aggregate_result_data = {.n_samples=1, .min=s , .max=s , .mean=s, .variance=0};
    
    Chunk_stats chunk_stats;
    Chunk_stats *chunk_stats_array = (Chunk_stats*) malloc(npes * sizeof(Chunk_stats));

    //GET NUMBER OF THREADS
    #pragma omp parallel 
    #pragma omp single
    {
        n_threads = omp_get_num_threads();
        // printf("Num threads on process %d: %d\n", mpi_id, n_threads);
    }

    for (int i=0; i<10; i++) {
        sampler_parallel(sample_cost_effectiveness_cser_bps_per_million, samples, n_threads, n_samples, mpi_id+1+i*npes);

        save_chunk_stats_to_struct(&chunk_stats, samples, n_samples);
            // Debugging
            // for (int i=0; i<npes; i++){
            //     if (mpi_id==i)
            //         print_stats(&chunk_stats);
            //     MPI_Barrier(MPI_COMM_WORLD);
            // }
        MPI_Gather(&chunk_stats, sizeof(Chunk_stats), MPI_CHAR, chunk_stats_array, sizeof(Chunk_stats), MPI_CHAR, 0, MPI_COMM_WORLD);

        if (mpi_id==0) {
            reduce_chunk_stats(&aggregate_result_data, chunk_stats_array, npes);
            printf("Iter %2d:\n", i);
            print_stats(&aggregate_result_data);
        }
    }

    // printf("\nStats: \n");
    // array_print_stats(samples, n_samples);
    // int n_bins = 50;
    // printf("\nHistogram: \n");
    // array_print_histogram(samples, n_samples, n_bins);

    free(samples);

    //END MPI ENVIRONMENT
    MPI_Finalize();
    return 0;
}
