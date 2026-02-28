#!/usr/bin/env bun

// Sorting algorithms benchmark in JavaScript

function bubbleSort(arr) {
    const n = arr.length;
    for (let i = 0; i < n - 1; i++) {
        for (let j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                [arr[j], arr[j + 1]] = [arr[j + 1], arr[j]];
            }
        }
    }
    return arr;
}

// Create test array
const arr = [64, 34, 25, 12, 22, 11, 90, 88, 45, 50, 33, 77, 99, 18, 7];

// Bubble sort
bubbleSort(arr);

console.log("Sorting completed");
console.log(`First 5: ${arr[0]}, ${arr[1]}, ${arr[2]}, ${arr[3]}, ${arr[4]}`);