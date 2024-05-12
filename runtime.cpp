#include <cstdio>
#include <cstdarg>
#include <cmath>
/* Compile runtime.cpp separately when compiling to an object file */

extern "C"
{
  int printi(int X)
  {
    return printf("%d\n", X);
  }

  int printd(double X)
  {
    return printf("%f\n", X);
  }

  int print(const char *fmt, ...) {
    int result = 0;
    va_list args;
    va_start(args, fmt);
    result = vprintf(fmt, args);
    va_end(args);
    return result;
  }

  int println(const char *fmt, ...) {
    int result = 0;
    va_list args;
    va_start(args, fmt);
    result = vprintf(fmt, args);
    va_end(args);
    putchar('\n');
    return result;
  }
  double sqrt(double X);
}
