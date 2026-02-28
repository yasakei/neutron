#!/usr/bin/env bun

// String operations benchmark in JavaScript

function reverseString(s) {
    return s.split('').reverse().join('');
}

function countOccurrences(haystack, needleChar) {
    let count = 0;
    for (const char of haystack) {
        if (char === needleChar) {
            count++;
        }
    }
    return count;
}

function simpleConcatenate(base, addition, times) {
    let result = base;
    for (let i = 0; i < times; i++) {
        result += addition;
    }
    return result;
}

// Create a long string
const longString = "Neutron programming language is fast and efficient. ".repeat(100);

// Perform string operations
const reversedResult = reverseString(longString);
const charCount = countOccurrences(longString, "a");
const concatenated = simpleConcatenate("", "Hello World from JavaScript! ", 50);

// Output results
console.log(`String ops: len=${longString.length} rev=${reversedResult.length} count=${charCount}`);