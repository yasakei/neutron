// Binary Search benchmark in JavaScript

function binarySearch(arr, left, right, x) {
    while (left <= right) {
        const mid = left + Math.floor((right - left) / 2);
        if (arr[mid] === x) {
            return mid;
        } else if (arr[mid] < x) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

// Create sorted array
const size = 100000;
const arr = new Array(size);
for (let i = 0; i < size; i++) {
    arr[i] = i;
}

// Perform multiple binary searches
let count = 0;
for (let i = 0; i < size; i += 100) {
    const result = binarySearch(arr, 0, size - 1, i);
    if (result !== -1) {
        count++;
    }
}

console.log(`Found ${count} elements`);
