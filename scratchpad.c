#define THOUSAND 1000
#define MILLION (1000 * 1000)
#define BILLION (1000 * MILLION)
#define TRILLION (1000 * BILLION)

#include <stdio.h>


int main(){
  double x = TRILLION * 1.0;
  double y = BILLION * 1.0;
  double z = x * y;
  printf("%lf\n", z);
}
