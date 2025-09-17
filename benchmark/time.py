import time

# Time Module Benchmark
# Tests the performance of time-related functions

print("===========================================")
print("         TIME MODULE BENCHMARK")
print("===========================================")
print("")

# Test time.time() performance
print("Testing time.time() performance...")
start_time = time.time()

i = 0
timestamp = 0

# Call time.time() 1000 times
while i < 1000:
    timestamp = time.time()
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Called time.time() 1000 times")
print("Duration:", duration, "milliseconds")
print("Average time per call:", duration / 1000, "milliseconds")
print("")

# Test time.sleep() performance (with minimal sleep)
print("Testing time.sleep() performance (0.001s sleep)...")
start_time = time.time()

i = 0

# Call time.sleep() 100 times with 0.001s sleep
while i < 100:
    time.sleep(0.001)  # Sleep for 1 millisecond
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Called time.sleep(0.001) 100 times")
print("Duration:", duration, "milliseconds")
print("Average time per call:", duration / 100, "milliseconds")
print("")

print("===========================================")
print("         BENCHMARK COMPLETE")
print("===========================================")