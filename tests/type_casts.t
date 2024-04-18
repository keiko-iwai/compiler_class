
int x = 4.0;
printi(x);
double y = 3;
printd(y);

printi(12.0 / 3.0);
printd(7 / 2);


int fi() {
  return 2;
}

double fd() {
  return 3.0;
}

int x2 = 3 + 1.0;
printi(x2);
double y2 = 3.0 + fi();
printd(y2);

printi(5.0 + 1);


int fi1(int a) {
  return 2 + a;
}

double fd1(double b) {
  return 3.0 + b;
}

int x3 = fi1(4.0);
printi(x3);
double y3 = fd1(4);
printd(y3);
