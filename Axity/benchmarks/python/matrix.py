#!/usr/bin/env python3

# Matrix operations benchmark in Python

def multiply_matrices(a, b, size):
    result = [[0 for _ in range(size)] for _ in range(size)]
    for i in range(size):
        for j in range(size):
            for k in range(size):
                result[i][j] += a[i][k] * b[k][j]
    return result

def create_matrix(size):
    matrix = []
    for i in range(size):
        row = []
        for j in range(size):
            row.append(i * size + j + 1)
        matrix.append(row)
    return matrix

# Create 20x20 matrices
size = 20
matrix_a = create_matrix(size)
matrix_b = create_matrix(size)

# Multiply matrices
result = multiply_matrices(matrix_a, matrix_b, size)
print(f"Matrix {len(result)}x{len(result[0])} = {result[0][0]}")