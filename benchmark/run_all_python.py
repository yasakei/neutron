import subprocess
import sys

# Run All Benchmarks
# Executes all benchmark scripts in sequence

print("===========================================")
print("         PYTHON LANGUAGE BENCHMARK SUITE")
print("===========================================")
print("")
print("This script will run all benchmark tests in sequence.")
print("Please be patient as some tests may take a moment to complete.")
print("")
input("Press Enter to begin...")
print("")

# Run arithmetic benchmark
print("1. Running Arithmetic Operations Benchmark...")
subprocess.run([sys.executable, "benchmark/arithmetic.py"])
print("")

# Run string benchmark
print("2. Running String Operations Benchmark...")
subprocess.run([sys.executable, "benchmark/string.py"])
print("")

# Run variables benchmark
print("3. Running Variable Assignments Benchmark...")
subprocess.run([sys.executable, "benchmark/variables.py"])
print("")

# Run functions benchmark
print("4. Running Function Calls Benchmark...")
subprocess.run([sys.executable, "benchmark/functions.py"])
print("")

# Run conditionals benchmark
print("5. Running Conditional Statements Benchmark...")
subprocess.run([sys.executable, "benchmark/conditionals.py"])
print("")

# Run time benchmark
print("6. Running Time Module Benchmark...")
subprocess.run([sys.executable, "benchmark/time.py"])
print("")

print("===========================================")
print("         ALL BENCHMARKS COMPLETE")
print("===========================================")
print("")
print("Thank you for running the Python benchmark suite!")