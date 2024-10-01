#include "squiggle_c/squiggle.h"
#include "squiggle_c/squiggle_more.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

double sample_cost_effectiveness_sentinel_bps_per_million(uint64_t * seed){
    double total_amount_xrisk = sample_beta(2, 20, seed);
    double black_swans_per_decade = sample_to(1, 7, seed);
    double chance_we_can_identify_black_swan_a_week_to_two_months_beforehand = sample_beta(5,10, seed);
    double chance_black_swan_is_existential = sample_beta(1, 100, seed);
    double chance_we_can_avert_or_mitigate_existential_risk = sample_beta(5, 1 * THOUSAND, seed);
    double cost_of_sentinel_per_year = sample_to(150 * THOUSAND,  500 * THOUSAND, seed);
    double cost_of_sentinel_per_decade = cost_of_sentinel_per_year * 10;
    double basis_point_reduction_in_existential_risk_per_million_dollars = black_swans_per_decade * 
        chance_we_can_identify_black_swan_a_week_to_two_months_beforehand * 
        chance_black_swan_is_existential * 
        chance_we_can_avert_or_mitigate_existential_risk * 
        (100 * 100) / 
        (cost_of_sentinel_per_decade / MILLION);
    // we're actually not doing anything with the estimate of basis points reduction in catastrophic risks. One option might be to have some conversion between catastrophic and existential, e.g., catastrophic being one thousandth as valuable to prevent.
    return basis_point_reduction_in_existential_risk_per_million_dollars;
}
