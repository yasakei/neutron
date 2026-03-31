#!/usr/bin/env python3

# Binary Search benchmark in Python

def binary_search(arr, left, right, x):
    while left <= right:
        mid = left + (right - left) // 2
        if arr[mid] == x:
            return mid
        elif arr[mid] < x:
            left = mid + 1
        else:
            right = mid - 1
    return -1

# Create sorted array
size = 100000
arr = list(range(size))

# Perform multiple binary searches
count = 0
for i in range(0, size, 100):
    result = binary_search(arr, 0, size - 1, i)
    if result != -1:
        count += 1

print(f"Found {count} elements")
