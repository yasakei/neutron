import time

# String Operations Benchmark
# Tests the speed of string concatenation and operations

print("===========================================")
print("         STRING OPERATIONS BENCHMARK")
print("===========================================")
print("")

# Test string concatenation performance
print("Testing string concatenation performance...")
start_time = time.time()

i = 0
result = ""

# Perform 10,000 string concatenations
while i < 10000:
    result = result + "a"
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 10,000 string concatenations")
print("Final string length:", len(result))
print("Duration:", duration, "milliseconds")
print("Average time per operation:", duration / 10000, "milliseconds")
print("")

# Test string building with numbers
print("Testing string building with numbers...")
start_time = time.time()

i = 0
result = ""

# Perform 10,000 string concatenations with numbers
while i < 10000:
    result = result + str(i)
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 10,000 string concatenations with numbers")
print("Final string length:", len(result))
print("Duration:", duration, "milliseconds")
print("Average time per operation:", duration / 10000, "milliseconds")
print("")

print("===========================================")
print("         BENCHMARK COMPLETE")
print("===========================================")