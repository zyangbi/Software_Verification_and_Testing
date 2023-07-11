tainted:
1. z = x (x is tainted, then z is tainted)
2. z = f(x, y)

untainted:
1. z = y (y is untainted, and this is in straight line code)

Iteration:
have to iterate until all entrySet and exitSet don't change.


Question:
1. All function arguments are passed by value. Also, a function call always returns a tainted value if at least one argument is tainted. Otherwise, a function call always returns an untainted value. So we only care about tainted variables in main function only?    