def member_n(s_n_prev, x, member_prev, n_prev):
  factor = 2 * n_prev
  return (-1) * member_prev * x * x / (factor * (factor + 1))

def sin_series(arg, eps):
  s = arg
  n = 1
  x_n = arg
  while abs(x_n) > eps:
    x_n = member_n(s, arg, x_n, n)
    s = s + x_n
    # print("x_%d = %.4f, s=%.4f" %(n, x_n, s))
    n = n + 1
  return s

eps = 0.0001
print("epsilon = %f\n" %(eps))
arg = 17.0
sum = 0.0

for x in range(10000000):
  sum = sin_series(arg, eps)

print("Answer: sin(%f) = %f, eps = %f" %(arg, sum, eps))
