#!/usr/bin/env python3

# Array Reduction benchmark in Python

def reduce_sum(arr):
    result = 0
    for val in arr:
        result += val
    return result

def reduce_product(arr):
    result = 1
    for val in arr:
        result *= val
    return result

def reduce_max(arr):
    result = arr[0]
    for val in arr[1:]:
        if val > result:
            result = val
    return result

def reduce_min(arr):
    result = arr[0]
    for val in arr[1:]:
        if val < result:
            result = val
    return result

def reduce_average(arr):
    total = 0
    count = 0
    for val in arr:
        total += val
        count += 1
    return total // count if count > 0 else 0

def reduce_filter_positive(arr):
    result = []
    for val in arr:
        if val > 0:
            result.append(val)
    return result

def reduce_count_even(arr):
    count = 0
    for val in arr:
        if val % 2 == 0:
            count += 1
    return count

# Create test array
arr = list(range(-5000, 5001))

# Perform reductions
sum_result = reduce_sum(arr)
max_result = reduce_max(arr)
min_result = reduce_min(arr)
avg_result = reduce_average(arr)
even_count = reduce_count_even(arr)
positive = reduce_filter_positive(arr)

print(f"Sum: {sum_result}, Max: {max_result}, Min: {min_result}, Avg: {avg_result}, Even: {even_count}, Positive: {len(positive)}")
