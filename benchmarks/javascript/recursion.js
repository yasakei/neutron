#!/usr/bin/env bun

// Recursive algorithms benchmark in JavaScript

function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

function ackermann(m, n) {
    if (m === 0) {
        return n + 1;
    }
    if (n === 0) {
        return ackermann(m - 1, 1);
    }
    return ackermann(m - 1, ackermann(m, n - 1));
}

function power(base, exp) {
    if (exp === 0) {
        return 1;
    }
    if (exp === 1) {
        return base;
    }
    return base * power(base, exp - 1);
}

// Run recursive benchmarks
const fact15 = factorial(15);
const ack = ackermann(3, 4);
const powVal = power(2, 10);

console.log(`Recursion: fact=${fact15} ack=${ack} pow=${powVal}`);