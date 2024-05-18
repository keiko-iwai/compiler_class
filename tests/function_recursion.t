int factorial(int n) {
  if (n <= 1) {
    return 1;
  }

  return n * factorial(n-1);
}

int n = 5;
println("factorial(%d) = %d", n, factorial(n));
