#!/usr/bin/env python3

# Loop operations benchmark in Python

def sum_with_for_loop(n):
    total = 0
    for i in range(n):
        total += i
    return total

def sum_with_while_loop(n):
    total = 0
    i = 0
    while i < n:
        total += i
        i += 1
    return total

def nested_loops(depth, size):
    count = 0
    for i in range(depth):
        for j in range(size):
            for k in range(size):
                count += 1
    return count

# Perform loop operations
for_result = sum_with_for_loop(100000)
while_result = sum_with_while_loop(100000)
nested_result = nested_loops(10, 50)

print(f"Loops: for={for_result} while={while_result} nested={nested_result}")