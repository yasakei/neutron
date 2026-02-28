#!/usr/bin/env bun

// Prime number generation benchmark in JavaScript

function isPrime(n) {
    if (n < 2) {
        return false;
    }
    if (n === 2) {
        return true;
    }
    if (n % 2 === 0) {
        return false;
    }
    for (let i = 3; i * i <= n; i += 2) {
        if (n % i === 0) {
            return false;
        }
    }
    return true;
}

function generatePrimes(limit) {
    const primes = [];
    for (let num = 2; num < limit; num++) {
        if (isPrime(num)) {
            primes.push(num);
        }
    }
    return primes;
}

// Generate primes up to 1000 (reduced from 5000 to match Neutron)
const result = generatePrimes(1000);
console.log(`Found ${result.length} primes`);