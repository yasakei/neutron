#!/usr/bin/env bun

// Fibonacci benchmark in JavaScript

function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// Calculate fibonacci of 35 (reasonable time for benchmarking)
const result = fibonacci(35);
console.log(result);