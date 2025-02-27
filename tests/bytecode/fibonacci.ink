{ fib(10) }

== function fib(n)
  {
    - n == 0:
      ~ return 0
    - n == 1:
      ~ return 1
    - else:
      ~ return fib(n - 1) + fib(n - 2)
  }
