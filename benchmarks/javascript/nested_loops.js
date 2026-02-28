#!/usr/bin/env bun

// Nested loops benchmark
// Tests deeply nested iteration performance

let total = 0;
let iterations = 0;

// Triple nested loop
let i = 0;
while (i < 50) {
    let j = 0;
    while (j < 50) {
        let k = 0;
        while (k < 50) {
            total += i + j + k;
            iterations++;
            k++;
        }
        j++;
    }
    i++;
}

console.log("Triple nested loop completed");
console.log(`Iterations: ${iterations}`);
console.log(`Total sum: ${total}`);

// Nested loop with conditionals
let count = 0;
let x = 0;
while (x < 100) {
    let y = 0;
    while (y < 100) {
        if (x % 2 === 0) {
            if (y % 3 === 0) {
                count++;
            }
        }
        y++;
    }
    x++;
}
console.log(`Conditional nested loop count: ${count}`);

// Nested loop with break
let found = 0;
let a = 0;
while (a < 100) {
    let b = 0;
    while (b < 100) {
        if (a * b === 2500) {
            found++;
            break;
        }
        b++;
    }
    a++;
}
console.log(`Found matches: ${found}`);