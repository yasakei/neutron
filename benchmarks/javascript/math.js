#!/usr/bin/env bun

// Math operations benchmark in JavaScript

function power(base, exp) {
    let result = 1;
    for (let i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

function sqrt(n) {
    if (n < 0) {
        return 0;
    }
    if (n === 0 || n === 1) {
        return n;
    }
    
    let guess = n / 2;
    const epsilon = 0.00001;
    let iterations = 0;
    
    while (iterations < 100) {
        const newGuess = (guess + n / guess) / 2;
        if (Math.abs(guess - newGuess) < epsilon) {
            return newGuess;
        }
        guess = newGuess;
        iterations++;
    }
    return guess;
}

function gcd(a, b) {
    while (b !== 0) {
        [a, b] = [b, a % b];
    }
    return a;
}

// Run math operations
const pow1 = power(2, 20);
const pow2 = power(3, 15);
const sq1 = sqrt(144);
const sq2 = sqrt(2);
const gcd1 = gcd(48, 18);
const gcd2 = gcd(100, 75);

console.log("Math operations completed");
console.log(`Results: ${pow1}, ${pow2}, ${gcd1}, ${gcd2}`);