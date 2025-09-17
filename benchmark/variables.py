import time

# Variable Assignments Benchmark
# Tests the speed of variable creation and assignment

print("===========================================")
print("         VARIABLE ASSIGNMENTS BENCHMARK")
print("===========================================")
print("")

# Test variable assignment performance
print("Testing variable assignment performance...")
start_time = time.time()

i = 0
a = 0
b = 0
c = 0
d = 0
e = 0

# Perform 100,000 variable assignments
while i < 100000:
    a = i
    b = i + 1
    c = i + 2
    d = i + 3
    e = i + 4
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 cycles of 5 variable assignments each")
print("Total assignments:", 100000 * 5)
print("Final values: a={}, b={}, c={}, d={}, e={}".format(a, b, c, d, e))
print("Duration:", duration, "milliseconds")
print("Average time per cycle:", duration / 100000, "milliseconds")
print("")

# Test variable access performance
print("Testing variable access performance...")
start_time = time.time()

i = 0
sum_val = 0

# Perform 100,000 variable accesses
while i < 100000:
    sum_val = a + b + c + d + e
    i = i + 1

end_time = time.time()
duration = (end_time - start_time) * 1000  # Convert to milliseconds

print("Performed 100,000 cycles of 5 variable accesses each")
print("Total accesses:", 100000 * 5)
print("Final sum:", sum_val)
print("Duration:", duration, "milliseconds")
print("Average time per cycle:", duration / 100000, "milliseconds")
print("")

print("===========================================")
print("         BENCHMARK COMPLETE")
print("===========================================")