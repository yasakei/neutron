// Array Reduction benchmark in JavaScript

function reduceSum(arr) {
    let result = 0;
    for (const val of arr) {
        result += val;
    }
    return result;
}

function reduceProduct(arr) {
    let result = 1;
    for (const val of arr) {
        result *= val;
    }
    return result;
}

function reduceMax(arr) {
    let result = arr[0];
    for (let i = 1; i < arr.length; i++) {
        if (arr[i] > result) {
            result = arr[i];
        }
    }
    return result;
}

function reduceMin(arr) {
    let result = arr[0];
    for (let i = 1; i < arr.length; i++) {
        if (arr[i] < result) {
            result = arr[i];
        }
    }
    return result;
}

function reduceAverage(arr) {
    let total = 0;
    let count = 0;
    for (const val of arr) {
        total += val;
        count++;
    }
    return count > 0 ? Math.floor(total / count) : 0;
}

function reduceFilterPositive(arr) {
    const result = [];
    for (const val of arr) {
        if (val > 0) {
            result.push(val);
        }
    }
    return result;
}

function reduceCountEven(arr) {
    let count = 0;
    for (const val of arr) {
        if (val % 2 === 0) {
            count++;
        }
    }
    return count;
}

// Create test array
const arr = [];
for (let i = -5000; i <= 5000; i++) {
    arr.push(i);
}

// Perform reductions
const sumResult = reduceSum(arr);
const maxResult = reduceMax(arr);
const minResult = reduceMin(arr);
const avgResult = reduceAverage(arr);
const evenCount = reduceCountEven(arr);
const positive = reduceFilterPositive(arr);

console.log(`Sum: ${sumResult}, Max: ${maxResult}, Min: ${minResult}, Avg: ${avgResult}, Even: ${evenCount}, Positive: ${positive.length}`);
