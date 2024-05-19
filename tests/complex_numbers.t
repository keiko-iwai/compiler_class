print("Arg (degrees) = ");
double arg = pi() / 180 * readd();
print("Absolute value = ");
double abs = readd();
double Re = abs*cos(arg);
double Im = abs*sin(arg);
println("z = %.2f + i * %.2f", Re, Im);
