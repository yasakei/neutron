#!/usr/bin/env python3
# Array/List operations benchmark
# Tests array creation, access, and manipulation

# Create and populate a large array
numbers = list(range(1000))
print("Created array with 1000 elements")

# Access elements and sum
sum_val = sum(numbers)
print(f"Sum of all numbers: {sum_val}")

# Count even numbers (manual filter)
even_count = sum(1 for x in numbers if x % 2 == 0)
print(f"Filtered {even_count} even numbers")

# Find specific element (manual find)
found = next((x for x in numbers if x == 500), -1)
print(f"Found element: {found}")

# Sorting
unsorted = [64, 34, 25, 12, 22, 11, 90]
sorted_arr = sorted(unsorted)
print(f"Sorted array: {', '.join(map(str, sorted_arr))}")

# Reverse
reversed_arr = list(reversed(sorted_arr))
print(f"Reversed array: {', '.join(map(str, reversed_arr))}")
