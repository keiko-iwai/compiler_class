int a = 123;
println("Outer value: %d", a);

int f1(int a) {
  println("Inner value %d", a);
  return a+1;
}

f1(10);
println("Outer value: %d", a);
