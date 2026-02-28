#!/usr/bin/env python3
# Nested loops benchmark
# Tests deeply nested iteration performance

total = 0
iterations = 0

# Triple nested loop
i = 0
while i < 50:
    j = 0
    while j < 50:
        k = 0
        while k < 50:
            total += i + j + k
            iterations += 1
            k += 1
        j += 1
    i += 1

print("Triple nested loop completed")
print(f"Iterations: {iterations}")
print(f"Total sum: {total}")

# Nested loop with conditionals
count = 0
x = 0
while x < 100:
    y = 0
    while y < 100:
        if x % 2 == 0:
            if y % 3 == 0:
                count += 1
        y += 1
    x += 1
print(f"Conditional nested loop count: {count}")

# Nested loop with break
found = 0
a = 0
while a < 100:
    b = 0
    while b < 100:
        if a * b == 2500:
            found += 1
            break
        b += 1
    a += 1
print(f"Found matches: {found}")
