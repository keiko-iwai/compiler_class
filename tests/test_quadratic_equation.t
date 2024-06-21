double discriminant(double a, double b, double c) {
  return b*b - 4.0*a*c;
}

int solve(double a, double b, double c) {
  double D = discriminant(a, b, c);
  if (D < 0) {
    println("Решений нет: D = %.4f", D);
    return -1;
  }

  double x_1 = (-b - sqrt(D)) / ( 2 * a );
  if (D == 0) {
    println("Два равных корня: %.4f, D = %.4f", x_1, D);
  } else {
    double x_2 = (-b + sqrt(D)) / ( 2 * a );
    println("Два решения: x1 = %.4f, x2 = %.4f, D = %.4f", x_1, x_2, D);
  }
  return 0;
}

solve(1.0, -5.0, 5.0);
