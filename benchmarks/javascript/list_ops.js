#!/usr/bin/env bun

// Array/List operations benchmark
// Tests array creation, access, and manipulation

// Create and populate a large array
const numbers = Array.from({length: 1000}, (_, i) => i);
console.log("Created array with 1000 elements");

// Access elements and sum
const sumVal = numbers.reduce((acc, num) => acc + num, 0);
console.log(`Sum of all numbers: ${sumVal}`);

// Count even numbers (manual filter)
const evenCount = numbers.filter(x => x % 2 === 0).length;
console.log(`Filtered ${evenCount} even numbers`);

// Find specific element (manual find)
const found = numbers.find(x => x === 500) || -1;
console.log(`Found element: ${found}`);

// Sorting
const unsorted = [64, 34, 25, 12, 22, 11, 90];
const sortedArr = [...unsorted].sort((a, b) => a - b);
console.log(`Sorted array: ${sortedArr.join(', ')}`);

// Reverse
const reversedArr = [...sortedArr].reverse();
console.log(`Reversed array: ${reversedArr.join(', ')}`);