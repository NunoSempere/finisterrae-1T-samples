#include "squiggle.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy

/* Cache optimizations */
#define CACHE_LINE_SIZE 64
// getconf LEVEL1_DCACHE_LINESIZE
// <https://stackoverflow.com/questions/794632/programmatically-get-the-cache-line-size>
typedef struct seed_cache_box_t {
    uint64_t seed;
    char padding[CACHE_LINE_SIZE - sizeof(uint64_t)];
    // Cache line size is 64 *bytes*, uint64_t is 64 *bits* (8 bytes). Different units!
} seed_cache_box;
// This avoids "false sharing", i.e., different threads competing for the same cache line
// Dealing with this shaves 4ms from a 12ms process, or a third of runtime
// <http://www.nic.uoregon.edu/~khuck/ts/acumem-report/manual_html/ch06s07.html>

/* Parallel sampler */
void sampler_parallel(double (*sampler)(uint64_t* seed), double* results, int n_threads, int n_samples)
{
    seed_cache_box* cache_box = (seed_cache_box*)malloc(sizeof(seed_cache_box) * (size_t)n_threads);

    srand(1);
    for (int thread_id = 0; thread_id < n_threads; thread_id++) {
        // Nuño to Jorge: you can't do this in parallel, since rand() is not thread safe
        cache_box[thread_id].seed = (uint64_t)rand() * (UINT64_MAX / RAND_MAX);
    }

    omp_set_dynamic(0); 
    omp_set_num_threads(n_threads);

    int i;
    #pragma omp parallel private(i)
    {
        int thread_id = omp_get_thread_num();
        #pragma omp for
        for (i = 0; i < n_samples; i++) {
            results[i] = sampler(&(cache_box[thread_id].seed));
        }
    }
    free(cache_box);
}

/* Get confidence intervals, given a sampler */

typedef struct ci_t {
    double low;
    double high;
} ci;

inline static void swp(int i, int j, double xs[])
{
    double tmp = xs[i];
    xs[i] = xs[j];
    xs[j] = tmp;
}

static int partition(int low, int high, double xs[], int length)
{
    if (low > high || high >= length) {
        printf("Invariant violated for function partition in %s (%d)", __FILE__, __LINE__);
        exit(1);
    }
    // Note: the scratchpad/ folder in commit 578bfa27 has printfs sprinkled throughout
    int pivot = low + (int)floor((high - low) / 2);
    double pivot_value = xs[pivot];
    swp(pivot, high, xs);
    int gt = low; /* This pointer will iterate until finding an element which is greater than the pivot. Then it will move elements that are smaller before it--more specifically, it will move elements to its position and then increment. As a result all elements between gt and i will be greater than the pivot. */
    for (int i = low; i < high; i++) {
        if (xs[i] < pivot_value) {
            swp(gt, i, xs);
            gt++;
        }
    }
    swp(high, gt, xs);
    return gt;
}

static double quickselect(int k, double xs[], int n)
{
    // https://en.wikipedia.org/wiki/Quickselect

    double* ys = malloc((size_t)n * sizeof(double));
    memcpy(ys, xs, (size_t)n * sizeof(double));
    // ^: don't rearrange item order in the original array

    int low = 0;
    int high = n - 1;
    for (;;) {
        if (low == high) {
            double result = ys[low];
            free(ys);
            return result;
        }
        int pivot = partition(low, high, ys, n);
        if (pivot == k) {
            double result = ys[pivot];
            free(ys);
            return result;
        } else if (k < pivot) {
            high = pivot - 1;
        } else {
            low = pivot + 1;
        }
    }
}

ci array_get_ci(ci interval, double* xs, int n)
{

    int low_k = (int)floor(interval.low * n);
    int high_k = (int)ceil(interval.high * n);
    ci result = {
        .low = quickselect(low_k, xs, n),
        .high = quickselect(high_k, xs, n),
    };
    return result;
}
ci array_get_90_ci(double xs[], int n)
{
    return array_get_ci((ci) { .low = 0.05, .high = 0.95 }, xs, n);
}

double array_get_median(double xs[], int n){
    int median_k = (int)floor(0.5 * n);
    return quickselect(median_k, xs, n);
}

void array_print_stats(double xs[], int n){
    ci ci_90 = array_get_ci((ci) { .low = 0.05, .high = 0.95 }, xs, n);
    ci ci_80 = array_get_ci((ci) { .low = 0.1, .high = 0.9 }, xs, n);
    ci ci_50 = array_get_ci((ci) { .low = 0.25, .high = 0.75 }, xs, n);
    double median = array_get_median(xs, n);
    double mean = array_mean(xs, n);
    double std = array_std(xs, n);
    printf("Mean: %lf\n"
           " Std: %lf\n"
           "  5%%: %lf\n"
           " 10%%: %lf\n"
           " 25%%: %lf\n"
           " 50%%: %lf\n"
           " 75%%: %lf\n"
           " 90%%: %lf\n"
           " 95%%: %lf\n",
           mean, std, ci_90.low, ci_80.low, ci_50.low, median, ci_50.high, ci_80.high, ci_90.high);
}

void print_histogram(int* bins, int n_bins, double min_value, double bin_width){

    // Calculate the scaling factor based on the maximum bin count
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
        double bin_start = min_value + i * bin_width;
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

}

void array_print_histogram(double* xs, int n_samples, int n_bins) {
    // Generated with the help of an llm; there might be subtle off-by-one errors
    // interface inspired by <https://github.com/red-data-tools/YouPlot>
    if (n_bins <= 0) {
        fprintf(stderr, "Number of bins must be a positive integer.\n");
        return;
    } else if (n_samples <= 0) {
        fprintf(stderr, "Number of samples must be a positive integer.\n");
        return;
    }

    int *bins = (int*) calloc((size_t)n_bins, sizeof(int));
    if (bins == NULL) {
        fprintf(stderr, "Memory allocation for bins failed.\n");
        return;
    }

    double min_value = xs[0], max_value = xs[0];

    // Find the minimum and maximum values from the samples
    for (int i = 0; i < n_samples; i++) {
        if (xs[i] < min_value) {
            min_value = xs[i];
        }
        if (xs[i] > max_value) {
            max_value = xs[i];
        }
    }

    // Avoid division by zero for a single unique value
    if (min_value == max_value) {
        max_value++;
    }

    // Calculate bin width
    double range = max_value - min_value;
    double bin_width = range / n_bins;

    // Fill the bins with sample counts
    for (int i = 0; i < n_samples; i++) {
        int bin_index = (int)((xs[i] - min_value) / bin_width);
        if (bin_index == n_bins) {
            bin_index--; // Last bin includes max_value
        }
        bins[bin_index]++;
    }

    print_histogram(bins, n_bins, min_value, bin_width);
    // Free the allocated memory for bins
    free(bins);
}

// Replicate some of the above functions over samplers
// However, in the future I'll delete this
// There should be a clear boundary between working with samplers and working with an array of samples
ci sampler_get_ci(ci interval, double (*sampler)(uint64_t*), int n, uint64_t* seed)
{
    UNUSED(seed); // don't want to use it right now, but want to preserve ability to do so (e.g., remove parallelism from internals). Also nicer for consistency.
    double* xs = malloc((size_t)n * sizeof(double));
    sampler_parallel(sampler, xs, 16, n);
    ci result = array_get_ci(interval, xs, n);
    free(xs);
    return result;
}
ci sampler_get_90_ci(double (*sampler)(uint64_t*), int n, uint64_t* seed)
{
    return sampler_get_ci((ci) { .low = 0.05, .high = 0.95 }, sampler, n, seed);
}

/* Algebra manipulations */

#define NORMAL90CONFIDENCE 1.6448536269514727

typedef struct normal_params_t {
    double mean;
    double std;
} normal_params;

normal_params algebra_sum_normals(normal_params a, normal_params b)
{
    normal_params result = {
        .mean = a.mean + b.mean,
        .std = sqrt((a.std * a.std) + (b.std * b.std)),
    };
    return result;
}

typedef struct lognormal_params_t {
    double logmean;
    double logstd;
} lognormal_params;

lognormal_params algebra_product_lognormals(lognormal_params a, lognormal_params b)
{
    lognormal_params result = {
        .logmean = a.logmean + b.logmean,
        .logstd = sqrt((a.logstd * a.logstd) + (b.logstd * b.logstd)),
    };
    return result;
}

lognormal_params convert_ci_to_lognormal_params(ci x)
{
    double loghigh = log(x.high);
    double loglow = log(x.low);
    double logmean = (loghigh + loglow) / 2.0;
    double logstd = (loghigh - loglow) / (2.0 * NORMAL90CONFIDENCE);
    lognormal_params result = { .logmean = logmean, .logstd = logstd };
    return result;
}

ci convert_lognormal_params_to_ci(lognormal_params y)
{
    double h = y.logstd * NORMAL90CONFIDENCE;
    double loghigh = y.logmean + h;
    double loglow = y.logmean - h;
    ci result = { .low = exp(loglow), .high = exp(loghigh) };
    return result;
}

/* Scaffolding to handle errors */
// We will sample from an arbitrary cdf
// and that operation might fail
// so we build some scaffolding here

#define MAX_ERROR_LENGTH 500
#define EXIT_ON_ERROR 0
#define PROCESS_ERROR(error_msg) process_error(error_msg, EXIT_ON_ERROR, __FILE__, __LINE__)

typedef struct box_t {
    int empty;
    double content;
    char* error_msg;
} box;

box process_error(const char* error_msg, int should_exit, char* file, int line)
{
    if (should_exit) {
        printf("%s, @, in %s (%d)", error_msg, file, line);
        exit(1);
    } else {
        char error_msg[MAX_ERROR_LENGTH];
        snprintf(error_msg, MAX_ERROR_LENGTH, "@, in %s (%d)", file, line); // NOLINT: We are being carefull here by considering MAX_ERROR_LENGTH explicitly.
        box error = { .empty = 1, .error_msg = error_msg };
        return error;
    }
}

/* Invert an arbitrary cdf at a point */
// Version #1:
// - input: (cdf: double => double, p)
// - output: Box(number|error)
box inverse_cdf_double(double cdf(double), double p)
{
    // given a cdf: [-Inf, Inf] => [0,1]
    // returns a box with either
    // x such that cdf(x) = p
    // or an error
    // if EXIT_ON_ERROR is set to 1, it exits instead of providing an error

    double low = -1.0;
    double high = 1.0;

    // 1. Make sure that cdf(low) < p < cdf(high)
    int interval_found = 0;
    while ((!interval_found) && (low > -DBL_MAX / 4) && (high < DBL_MAX / 4)) {
        // for floats, use FLT_MAX instead
        // Note that this approach is overkill
        // but it's also the *correct* thing to do.

        int low_condition = (cdf(low) < p);
        int high_condition = (p < cdf(high));
        if (low_condition && high_condition) {
            interval_found = 1;
        } else if (!low_condition) {
            low = low * 2;
        } else if (!high_condition) {
            high = high * 2;
        }
    }

    if (!interval_found) {
        return PROCESS_ERROR("Interval containing the target value not found, in function inverse_cdf");
    } else {

        int convergence_condition = 0;
        int count = 0;
        while (!convergence_condition && (count < (INT_MAX / 2))) {
            double mid = (high + low) / 2;
            int mid_not_new = (mid == low) || (mid == high);
            // double width = high - low;
            // if ((width < 1e-8) || mid_not_new){
            if (mid_not_new) {
                convergence_condition = 1;
            } else {
                double mid_sign = cdf(mid) - p;
                if (mid_sign < 0) {
                    low = mid;
                } else if (mid_sign > 0) {
                    high = mid;
                } else if (mid_sign == 0) {
                    low = mid;
                    high = mid;
                }
            }
        }

        if (convergence_condition) {
            box result = { .empty = 0, .content = low };
            return result;
        } else {
            return PROCESS_ERROR("Search process did not converge, in function inverse_cdf");
        }
    }
}

// Version #2:
// - input: (cdf: double => Box(number|error), p)
// - output: Box(number|error)
box inverse_cdf_box(box cdf_box(double), double p)
{
    // given a cdf: [-Inf, Inf] => Box([0,1])
    // returns a box with either
    // x such that cdf(x) = p
    // or an error
    // if EXIT_ON_ERROR is set to 1, it exits instead of providing an error

    double low = -1.0;
    double high = 1.0;

    // 1. Make sure that cdf(low) < p < cdf(high)
    int interval_found = 0;
    while ((!interval_found) && (low > -DBL_MAX / 4) && (high < DBL_MAX / 4)) {
        // for floats, use FLT_MAX instead
        // Note that this approach is overkill
        // but it's also the *correct* thing to do.
        box cdf_low = cdf_box(low);
        if (cdf_low.empty) {
            return PROCESS_ERROR(cdf_low.error_msg);
        }

        box cdf_high = cdf_box(high);
        if (cdf_high.empty) {
            return PROCESS_ERROR(cdf_low.error_msg);
        }

        int low_condition = (cdf_low.content < p);
        int high_condition = (p < cdf_high.content);
        if (low_condition && high_condition) {
            interval_found = 1;
        } else if (!low_condition) {
            low = low * 2;
        } else if (!high_condition) {
            high = high * 2;
        }
    }

    if (!interval_found) {
        return PROCESS_ERROR("Interval containing the target value not found, in function inverse_cdf");
    } else {

        int convergence_condition = 0;
        int count = 0;
        while (!convergence_condition && (count < (INT_MAX / 2))) {
            double mid = (high + low) / 2;
            int mid_not_new = (mid == low) || (mid == high);
            // double width = high - low;
            if (mid_not_new) {
                // if ((width < 1e-8) || mid_not_new){
                convergence_condition = 1;
            } else {
                box cdf_mid = cdf_box(mid);
                if (cdf_mid.empty) {
                    return PROCESS_ERROR(cdf_mid.error_msg);
                }
                double mid_sign = cdf_mid.content - p;
                if (mid_sign < 0) {
                    low = mid;
                } else if (mid_sign > 0) {
                    high = mid;
                } else if (mid_sign == 0) {
                    low = mid;
                    high = mid;
                }
            }
        }

        if (convergence_condition) {
            box result = { .empty = 0, .content = low };
            return result;
        } else {
            return PROCESS_ERROR("Search process did not converge, in function inverse_cdf");
        }
    }
}

/* Sample from an arbitrary cdf */
// Before: invert an arbitrary cdf at a point
// Now: from an arbitrary cdf, get a sample
box sampler_cdf_box(box cdf(double), uint64_t* seed)
{
    double p = sample_unit_uniform(seed);
    box result = inverse_cdf_box(cdf, p);
    return result;
}
box sampler_cdf_double(double cdf(double), uint64_t* seed)
{
    double p = sample_unit_uniform(seed);
    box result = inverse_cdf_double(cdf, p);
    return result;
}
double sampler_cdf_danger(box cdf(double), uint64_t* seed)
{
    double p = sample_unit_uniform(seed);
    box result = inverse_cdf_box(cdf, p);
    if (result.empty) {
        exit(1);
    } else {
        return result.content;
    }
}

/* array print: potentially useful for debugging */
void array_print(double xs[], int n)
{
    printf("[");
    for (int i = 0; i < n - 1; i++) {
        printf("%f, ", xs[i]);
    }
    printf("%f", xs[n - 1]);
    printf("]\n");
}
