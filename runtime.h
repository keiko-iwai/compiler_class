/* External C functions accessible from inside the language */

extern "C"
{
  int printi(int X);
  int printd(double X);
  int print(const char *fmt, ...);
  int println(const char *fmt, ...);
  double sqrt(double X);
}
