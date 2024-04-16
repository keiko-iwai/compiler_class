int f1(int a, int b) {
  printi(a);
  printi(b);
  return a*b;
}

int result = f1(11, 76);
printi(result);


double discriminant(double a, double b, double c) {
  return sqrt(b*b - 4.0*a*c);
}

double y = discriminant(1.0, 2.0, -15.0);
printd(y);
