#!/usr/bin/env python3

# Prime number generation benchmark in Python

def is_prime(n):
    if n < 2:
        return False
    if n == 2:
        return True
    if n % 2 == 0:
        return False
    i = 3
    while i * i <= n:
        if n % i == 0:
            return False
        i += 2
    return True

def generate_primes(limit):
    primes = []
    for num in range(2, limit):
        if is_prime(num):
            primes.append(num)
    return primes

# Generate primes up to 1000 (reduced from 5000 to match Neutron)
result = generate_primes(1000)
print(f"Found {len(result)} primes")