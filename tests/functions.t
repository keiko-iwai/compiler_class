double discriminant(double a, double b, double c) {
  return sqrt(b*b - 4.0*a*c);
}

int solve(double a, double b, double c) {
  double D = discriminant(a, b, c);
  if (D < 0) {
    println("Решений нет.");
    return -1;
  }

  double x_1 = (-2 * b - sqrt(D)) / ( 4 * a * c );
  if (D == 0) {
    println("Два равных корня: %.4f", x_1);
  } else {
    double x_2 = (-2 * b + sqrt(D)) / ( 4 * a * c );
    println("Два решения: x1 = %.4f, x2 = %.4f", x_1, x_2);
  }
}

solve(1.0, 2.0, -15.0);
