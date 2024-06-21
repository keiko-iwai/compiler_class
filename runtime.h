/* External C functions accessible from inside the language */

extern "C"
{
  /* IO */
  int printi(int X);
  int printd(double X);
  int print(const char *fmt, ...);
  int println(const char *fmt, ...);

  int readi();
  double readd();
  char *readline();

  /* MATH */
  double fabs(double X);
  double sqrt(double X);
  double sin(double arg);
  double cos(double arg);
  double pow(double base, double exponent);
  double pi();
}
