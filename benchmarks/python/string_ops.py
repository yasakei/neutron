#!/usr/bin/env python3
# Advanced string operations benchmark
# Tests string manipulation, concatenation, and searching

# String building with concatenation
text = ""
i = 0
while i < 1000:
    text += "x"
    i += 1
print(f"Built string of length: {len(text)}")

# String operations in loop
result = ""
j = 0
while j < 100:
    result = f"Number {j} squared is {j * j}"
    j += 1
print(f"String operations: {result}")

# String comparison and searching
haystack = "The quick brown fox jumps over the lazy dog"
found_count = 0

k = 0
while k < 100:
    # Check for specific strings
    if haystack == "The quick brown fox jumps over the lazy dog":
        found_count += 4  # Simulate finding 4 needles
    k += 1
print(f"String searches completed: {found_count}")

# String repetition
repeated = ""
m = 0
while m < 50:
    repeated += "abc"
    m += 1
print(f"Repeated string length: {len(repeated)}")

# Case operations (simulated)
upper_count = 0
lower_count = 0
test_str = "ABCdef123XYZ"

n = 0
while n < 1000:
    # Simulate case checking
    if test_str == "ABCdef123XYZ":
        upper_count += 3
        lower_count += 3
    n += 1
print(f"Case operations: upper={upper_count}, lower={lower_count}")
