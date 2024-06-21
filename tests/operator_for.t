int x;
int sum;

print("i = ");
for (sum = 0, x = 1; x <= 10; x = x+1) {
  sum = sum + x;
  if (x < 10) {
    if (sum >= 10) { print(" "); }
  }
  print("%d ", x);
}
println("");

print("S = ");
for (sum = 0, x = 1; x <= 10; x = x+1) {
  sum = sum + x;
  print("%d ", sum);
}
