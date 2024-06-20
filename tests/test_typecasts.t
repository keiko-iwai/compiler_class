int x = 2.5;
println("int x = 2.5 => x = %d", x);
double y = 2;
println("double y = 2 => y = %.2f", y);

double f2() { return -2.2; }
int x1 = f2();
println("int x = -2.2 => x = %d", x1);

int f1() { return 5; }
double y1 = f1();
println("double y = 5 => y = %.2f", y1);

int f3(int n) { return n * n; }
int x3 = f3(7.5);
println("Converting argument to int: %.2f ^2 = %d", 7.5, x3);

double f4(double n) { return n * n; }
double y3 = f4(7);
println("Converting argument to double: %d ^2 = %.2f", 7, y3);

int f5(double n) { return n * n; }
int x4 = f5(7.5);
println("Converting return value to int %.2f ^2 = %d", 7.5, x4);

double f6(int n) { return n * n; }
double y4 = f6(7);
println("Converting return value to double: %d ^2 = %.2f", 7, y4);
