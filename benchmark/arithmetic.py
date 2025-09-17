import time

# Arithmetic Operations Benchmark
# Tests the speed of basic arithmetic operations

print("===========================================")
print("         ARITHMETIC OPERATIONS BENCHMARK")
print("===========================================")
print("")

# Test addition performance
print("Testing addition performance...")
start_time = time.time()

i = 0
result = 0

# Perform 100,000 additions
while i < 100000:
    result = result + i
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 additions")
print("Final result: (large number)")
print("Duration:", duration, "milliseconds")
print("Average time per operation:", duration / 100000, "milliseconds")
print("")

# Test multiplication performance
print("Testing multiplication performance...")
start_time = time.time()

i = 0
result = 1

# Perform 100,000 multiplications
while i < 100000:
    result = result * 2
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 multiplications")
print("Final result: (too large to display)")
print("Duration:", duration, "milliseconds")
print("Average time per operation:", duration / 100000, "milliseconds")
print("")

# Test mixed arithmetic operations
print("Testing mixed arithmetic operations...")
start_time = time.time()

i = 0
result = 0

# Perform 100,000 mixed operations
while i < 100000:
    result = (result + i) * 2 - i
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 mixed arithmetic operations")
print("Final result: (too large to display)")
print("Duration:", duration, "milliseconds")
print("Average time per operation:", duration / 100000, "milliseconds")
print("")

print("===========================================")
print("         BENCHMARK COMPLETE")
print("===========================================")