#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

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

    uint64_t* seed = malloc(sizeof(uint64_t));
    *seed = UINT64_MAX/2; // xorshift can't start with 0

    /* Do first run to get mean & min & max */
    double mean = 0;
    double max = -DBL_MAX;
    double min = DBL_MAX;
    for(int i=0; i<n_samples; i++){
        double s = sampler(seed);
        mean+=s;
        if(max < s){
            max = s;
        }
        if(min > s){
            min = s;
        }
    }
    mean = mean/n_samples;

    /* Set up histogram */
    int *bins = (int*) calloc((size_t)n_bins, sizeof(int));
    if (bins == NULL) {
        fprintf(stderr, "Memory allocation for bins failed.\n");
        return;
    }

    if (min == max) { // avoid division by 0
        max++;
    }
    double range = max - min;
    double bin_width = range / n_bins;

    /* Do second pass */
    *seed = UINT64_MAX/2; // go back to the beginning
    double std = 0;
    for(int i=0; i<n_samples; i++){
        double s = sampler(seed);
        std += (s - mean) * (s - mean);

        int bin_index = (int)((s - min) / bin_width);
        if (bin_index == n_bins) {
            bin_index--; // Last bin includes max_value
        }
        bins[bin_index]++;
    }
    std = sqrt(std/n_samples);

    // Print stats
    printf("Mean: %lf\n"
           " Std: %lf\n",
           mean, std);
    // Calculate histogram scaling factor based on the maximum bin count
    int max_bin_count = 0;
    for (int i = 0; i < n_bins; i++) {
        if (bins[i] > max_bin_count) {
            max_bin_count = bins[i];
        }
    }
    const int MAX_WIDTH = 50; // Adjust this to your terminal width
    double scale = max_bin_count > MAX_WIDTH ? (double)MAX_WIDTH / max_bin_count : 1.0;

    // Print the histogram
    for (int i = 0; i < n_bins; i++) {
        double bin_start = min + i * bin_width;
        double bin_end = bin_start + bin_width;
        if(bin_width < 0.01){
            printf("  [%4.3f, %4.3f): ", bin_start, bin_end); 
        } else if(bin_width < 0.1){
            printf("  [%4.2f, %4.2f): ", bin_start, bin_end); 
        } else if(bin_width < 1){
            printf("  [%4.1f, %4.1f): ", bin_start, bin_end); 
        } else if(bin_width < 10){
            printf("  [%4.0f, %4.0f): ", bin_start, bin_end); 
        } else {
            printf("  [%4f, %4f): ", bin_start, bin_end); 
        }
        // number of decimals could depend on the number of bins
        // or on the size of the smallest bucket

        int marks = (int)(bins[i] * scale);
        for (int j = 0; j < marks; j++) {
            printf("█");
        }
        printf(" %d\n", bins[i]);
    }

    // Free the allocated memory for bins
    free(bins);
    return;

}

int main()
{
    int n_samples = 1 * MILLION;
    int n_bins = 50;

    sampler_memory_efficient(sample_cost_effectiveness_cser_bps_per_million, n_bins, n_samples);

}

