/*
Mandelbrot set visualization.
Based on an example from LLVM tutorial here:
https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl06.html
*/

double printdensity(double d) {
  if (d > 8)
    { print("%c", 32); } // ' '
  else if (d > 4)
    { print("%c", 46); } // '.'
  else if (d > 2)
    { print("%c", 43); } // '+'
  else
    { print("%c", 42); } // '*'
  return d;
}

int mandelconverger(double real, double imag, int iters, double creal, double cimag) {
  if (iters > 255) {
    return iters;
  }
  if (real*real + imag*imag > 4) {
    return iters;
  }
  return mandelconverger(real*real - imag*imag + creal,
                         2*real*imag + cimag,
                         iters+1, creal, cimag);
}

int mandelconverge(double real, double imag) {
  return mandelconverger(real, imag, 0, real, imag);
}

int mandelhelp(double xmin, double xmax, double xstep, double ymin, double ymax, double ystep) {
  double y;
  double x;
  for (y = ymin; y < ymax; y = y + ystep) {
    for (x = xmin; x < xmax; x = x + xstep) {
      printdensity(mandelconverge(x,y));
    }
    print("%c", 10); // backspace
  }
  return 0;
}

int mandel(double realstart, double imagstart, double realmag, double imagmag) {
  return mandelhelp(realstart, realstart+realmag*78, realmag,
                    imagstart, imagstart+imagmag*40, imagmag);
}

mandel(-2.3, -1.3, 0.05, 0.07);
