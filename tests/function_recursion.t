int factorial(int n) {
  if (n <= 1) {
    return 1;
  }

  return n * factorial(n-1);
}
print("Factorial example. Input x: ");
int x = readi();
println("factorial(%d) = %d", x, factorial(x));
