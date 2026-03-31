#!/usr/bin/env python3

# Math operations benchmark in Python

def power(base, exp):
    result = 1
    for _ in range(exp):
        result *= base
    return result

def sqrt(n):
    if n < 0:
        return 0
    if n == 0 or n == 1:
        return n
    
    guess = n / 2
    epsilon = 0.00001
    iterations = 0
    
    while iterations < 100:
        new_guess = (guess + n / guess) / 2
        if abs(guess - new_guess) < epsilon:
            return new_guess
        guess = new_guess
        iterations += 1
    return guess

def gcd(a, b):
    while b != 0:
        a, b = b, a % b
    return a

# Run math operations
pow1 = power(2, 20)
pow2 = power(3, 15)
sq1 = sqrt(144)
sq2 = sqrt(2)
gcd1 = gcd(48, 18)
gcd2 = gcd(100, 75)

print("Math operations completed")
print(f"Results: {pow1}, {pow2}, {gcd1}, {gcd2}")
