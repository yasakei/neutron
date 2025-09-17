import time

# Conditional Statements Benchmark
# Tests the speed of if-else conditional evaluations

print("===========================================")
print("         CONDITIONAL STATEMENTS BENCHMARK")
print("===========================================")
print("")

# Test simple conditional performance
print("Testing simple conditional performance...")
start_time = time.time()

i = 0
result = 0

# Perform 100,000 conditional evaluations
while i < 100000:
    if i % 2 == 0:
        result = result + 1
    else:
        result = result - 1
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 conditional evaluations")
print("Final result:", result)
print("Duration:", duration, "milliseconds")
print("Average time per evaluation:", duration / 100000, "milliseconds")
print("")

# Test nested conditional performance
print("Testing nested conditional performance...")
start_time = time.time()

i = 0
result = 0

# Perform 100,000 nested conditional evaluations
while i < 100000:
    if i % 2 == 0:
        if i % 4 == 0:
            result = result + 2
        else:
            result = result + 1
    else:
        if i % 3 == 0:
            result = result - 2
        else:
            result = result - 1
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 nested conditional evaluations")
print("Final result:", result)
print("Duration:", duration, "milliseconds")
print("Average time per evaluation:", duration / 100000, "milliseconds")
print("")

print("===========================================")
print("         BENCHMARK COMPLETE")
print("===========================================")