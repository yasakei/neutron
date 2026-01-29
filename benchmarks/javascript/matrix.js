#!/usr/bin/env bun

// Matrix operations benchmark in JavaScript

function multiplyMatrices(a, b, size) {
    const result = Array(size).fill().map(() => Array(size).fill(0));
    for (let i = 0; i < size; i++) {
        for (let j = 0; j < size; j++) {
            for (let k = 0; k < size; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}

function createMatrix(size) {
    const matrix = [];
    for (let i = 0; i < size; i++) {
        const row = [];
        for (let j = 0; j < size; j++) {
            row.push(i * size + j + 1);
        }
        matrix.push(row);
    }
    return matrix;
}

// Create 20x20 matrices
const size = 20;
const matrixA = createMatrix(size);
const matrixB = createMatrix(size);

// Multiply matrices
const result = multiplyMatrices(matrixA, matrixB, size);
console.log(`Matrix ${result.length}x${result[0].length} = ${result[0][0]}`);