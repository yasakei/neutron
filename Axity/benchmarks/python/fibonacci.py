#!/usr/bin/env python3

# Fibonacci benchmark in Python

def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

# Calculate fibonacci of 35 (reasonable time for benchmarking)
result = fibonacci(35)
print(result)