import time

# Function Calls Benchmark
# Tests the speed of function definitions and calls

print("===========================================")
print("         FUNCTION CALLS BENCHMARK")
print("===========================================")
print("")

# Define a simple function
def add(a, b):
    return a + b

# Test function call performance
print("Testing function call performance...")
start_time = time.time()

i = 0
result = 0

# Perform 100,000 function calls
while i < 100000:
    result = add(i, i + 1)
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 function calls")
print("Final result:", result)
print("Duration:", duration, "milliseconds")
print("Average time per call:", duration / 100000, "milliseconds")
print("")

# Test nested function calls
def multiply(a, b):
    return a * b

def complex_calc(a, b, c):
    return add(multiply(a, b), c)

print("Testing nested function calls...")
start_time = time.time()

i = 0
result = 0

# Perform 100,000 nested function calls
while i < 100000:
    result = complex_calc(i, 2, 10)
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 nested function calls")
print("Final result:", result)
print("Duration:", duration, "milliseconds")
print("Average time per call:", duration / 100000, "milliseconds")
print("")

print("===========================================")
print("         BENCHMARK COMPLETE")
print("===========================================")