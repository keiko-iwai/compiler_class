#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
/* Compile runtime.cpp separately when compiling to an object file */

extern "C"
{
  int MAX_STRLEN = 1024;
  char *inputbuf = 0;
  void initialize() {
    if (!inputbuf)
      inputbuf = (char *)malloc(MAX_STRLEN * sizeof(char));
    if (!inputbuf)
    {
      printf("Malloc failed!\n");
      exit(1);
    }
  }

  /* MATH */
  double fabs(double X);
  double sqrt(double X);
  double sin(double arg);
  double cos(double arg);
  double pow(double base, double exponent);
  double pi()
  {
    return M_PI;
  }
  /* IO */
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
  int readi() {
    int x;
    initialize();
    fgets(inputbuf, MAX_STRLEN, stdin);
    x = atoi(inputbuf);
    return x;
  }
  double readd() {
    double x;
    initialize();
    fgets(inputbuf, MAX_STRLEN, stdin);
    x = atof(inputbuf);
    return x;
  }
  char *readline() {
    char *x;
    initialize();
    fgets(inputbuf, MAX_STRLEN, stdin);
    int length = strlen(inputbuf);
    x = (char *)malloc((strlen(inputbuf) + 1) * sizeof(char));
    strncpy(x, inputbuf, length + 1);
    return x;
  }
}
