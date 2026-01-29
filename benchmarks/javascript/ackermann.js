#!/usr/bin/env bun

// Deep recursion benchmark - Factorial and Power functions
// Tests function call overhead and stack management

function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

function power(base, exp) {
    if (exp === 0) {
        return 1;
    }
    return base * power(base, exp - 1);
}

// Test factorial (deep recursion)
let result = factorial(10);
console.log(`Factorial(10) = ${result}`);

result = factorial(12);
console.log(`Factorial(12) = ${result}`);

result = factorial(15);
console.log(`Factorial(15) = ${result}`);

// Test power function
result = power(2, 10);
console.log(`Power(2, 10) = ${result}`);

result = power(3, 8);
console.log(`Power(3, 8) = ${result}`);

result = power(5, 7);
console.log(`Power(5, 7) = ${result}`);