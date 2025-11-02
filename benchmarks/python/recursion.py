#!/usr/bin/env python3

# Recursive algorithms benchmark in Python

def factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)

def ackermann(m, n):
    if m == 0:
        return n + 1
    if n == 0:
        return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

def power(base, exp):
    if exp == 0:
        return 1
    if exp == 1:
        return base
    return base * power(base, exp - 1)

# Run recursive benchmarks
fact15 = factorial(15)
ack = ackermann(3, 6)
pow_val = power(2, 10)

print(f"Recursion: fact={fact15} ack={ack} pow={pow_val}")
