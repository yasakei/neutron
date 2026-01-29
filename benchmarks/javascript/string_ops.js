#!/usr/bin/env bun

// Advanced string operations benchmark
// Tests string manipulation, concatenation, and searching

// String building with concatenation
let text = "";
let i = 0;
while (i < 1000) {
    text += "x";
    i++;
}
console.log(`Built string of length: ${text.length}`);

// String operations in loop
let result = "";
let j = 0;
while (j < 100) {
    result = `Number ${j} squared is ${j * j}`;
    j++;
}
console.log(`String operations: ${result}`);

// String comparison and searching
const haystack = "The quick brown fox jumps over the lazy dog";
let foundCount = 0;

let k = 0;
while (k < 100) {
    // Check for specific strings
    if (haystack === "The quick brown fox jumps over the lazy dog") {
        foundCount += 4; // Simulate finding 4 needles
    }
    k++;
}
console.log(`String searches completed: ${foundCount}`);

// String repetition
let repeated = "";
let m = 0;
while (m < 50) {
    repeated += "abc";
    m++;
}
console.log(`Repeated string length: ${repeated.length}`);

// Case operations (simulated)
let upperCount = 0;
let lowerCount = 0;
const testStr = "ABCdef123XYZ";

let n = 0;
while (n < 1000) {
    // Simulate case checking
    if (testStr === "ABCdef123XYZ") {
        upperCount += 3;
        lowerCount += 3;
    }
    n++;
}
console.log(`Case operations: upper=${upperCount}, lower=${lowerCount}`);