#!/usr/bin/env bun

// Loop operations benchmark in JavaScript

function sumWithForLoop(n) {
    let total = 0;
    for (let i = 0; i < n; i++) {
        total += i;
    }
    return total;
}

function sumWithWhileLoop(n) {
    let total = 0;
    let i = 0;
    while (i < n) {
        total += i;
        i++;
    }
    return total;
}

function nestedLoops(depth, size) {
    let count = 0;
    for (let i = 0; i < depth; i++) {
        for (let j = 0; j < size; j++) {
            for (let k = 0; k < size; k++) {
                count++;
            }
        }
    }
    return count;
}

// Perform loop operations
const forResult = sumWithForLoop(100000);
const whileResult = sumWithWhileLoop(100000);
const nestedResult = nestedLoops(10, 50);

console.log(`Loops: for=${forResult} while=${whileResult} nested=${nestedResult}`);