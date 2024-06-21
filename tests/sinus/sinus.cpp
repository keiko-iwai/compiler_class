#include <iostream>
#include <cmath>

double member_n(double s_n_prev, double x, double member_prev, int n_prev) {
  int factor = 2 * n_prev;
  return (-1) * member_prev * x * x / (factor * (factor + 1));
}

double sin_series(double arg, double eps) {
  double s = arg;
  int n = 1;
  double x_n;
  for (x_n = arg; fabs(x_n) > eps; n = n + 1) {
    x_n = member_n(s, arg, x_n, n);
    s = s + x_n;
    //printf("x_%d = %.4f, s=%.4f", n, x_n, s);
  }
  return s;
}


int main() {
  double eps = 0.0001;
  printf("epsilon = %f\n", eps);
  double arg = 17;
  double sum;

  int i = 0;
  for (i = 0; i < 10000000; i = i+1) {
    sum = sin_series(arg, eps);
  }

  printf("Answer: sin(%f) = %f, eps = %f\n", arg, sum, eps);
}
