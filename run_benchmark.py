#!/usr/bin/env python3
import os
import sys
import platform
import subprocess
import time

# Colors
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    MAGENTA = '\033[0;35m'
    BOLD = '\033[1m'
    NC = '\033[0m'

    @staticmethod
    def print(text, color=NC, end='\n'):
        if sys.stdout.isatty() and platform.system() != "Windows":
             print(f"{color}{text}{Colors.NC}", end=end)
        else:
             print(text, end=end)

def run_benchmark(name, neutron_bin, neutron_file, python_file):
    # Run Python version
    python_start = time.time()
    try:
        python_result = subprocess.run(
            [sys.executable, python_file],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace'
        )
        python_end = time.time()
        
        if python_result.returncode == 0:
            python_time = python_end - python_start
            python_success = True
            python_output = python_result.stdout.strip()
            python_display = f"{python_time:.3f}s"
        else:
            python_time = 0
            python_success = False
            python_output = ""
            python_display = "FAILED"
    except Exception as e:
        python_time = 0
        python_success = False
        python_output = ""
        python_display = "FAILED"

    # Run Neutron version
    neutron_start = time.time()
    try:
        neutron_result = subprocess.run(
            [neutron_bin, neutron_file],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace'
        )
        neutron_end = time.time()
        
        if neutron_result.returncode == 0:
            neutron_time = neutron_end - neutron_start
            neutron_success = True
            neutron_output = neutron_result.stdout.strip()
            neutron_display = f"{neutron_time:.3f}s"
        else:
            neutron_time = 0
            neutron_success = False
            neutron_output = ""
            neutron_display = "FAILED"
    except Exception as e:
        neutron_time = 0
        neutron_success = False
        neutron_output = ""
        neutron_display = "FAILED"

    # Compare
    output_match = True
    result_str = ""
    global neutron_faster, python_faster, failed_benchmarks
    
    if python_success and neutron_success:
        if python_output != neutron_output:
            output_match = False
            failed_benchmarks += 1
            result_str = "OUTPUT MISMATCH"
            Colors.current_color = Colors.RED
        else:
            # Avoid division by zero
            if neutron_time == 0: 
                ratio = 999 
            else:
                ratio = python_time / neutron_time
                
            if neutron_time < python_time:
                neutron_faster += 1
                result_str = f"Neutron {ratio:.2f}x faster"
                Colors.current_color = Colors.GREEN
            else:
                python_faster += 1
                if python_time == 0:
                    ratio_inv = 999
                else: 
                    ratio_inv = neutron_time / python_time
                result_str = f"Python {ratio_inv:.2f}x faster"
                Colors.current_color = Colors.YELLOW
    else:
        failed_benchmarks += 1
        result_str = "BENCHMARK FAILED"
        Colors.current_color = Colors.RED
        
    # Print row
    print(f"  {name:<25} ", end="")
    Colors.print("Python:", Colors.CYAN, end=" ")
    print(f"{python_display:<10} ", end="")
    Colors.print("Neutron:", Colors.MAGENTA, end=" ")
    print(f"{neutron_display:<10} ", end="")
    Colors.print(result_str, Colors.current_color)
    
    if not output_match and python_success and neutron_success:
        Colors.print("    Python output:", Colors.RED)
        for line in python_output.splitlines():
            print(f"      {line}")
        Colors.print("    Neutron output:", Colors.RED)
        for line in neutron_output.splitlines():
            print(f"      {line}")

neutron_faster = 0
python_faster = 0
failed_benchmarks = 0

def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    bench_dir = os.path.join(root_dir, "benchmarks")

    # Find neutron binary
    exe_name = "neutron.exe" if platform.system() == "Windows" else "neutron"
    possible_paths = [
        os.path.join(root_dir, "build", exe_name),
        os.path.join(root_dir, "build", "Release", exe_name),
        os.path.join(root_dir, "build", "Debug", exe_name),
        os.path.join(root_dir, exe_name)
    ]
    
    neutron_bin = None
    for p in possible_paths:
        if os.path.exists(p):
            neutron_bin = p
            break
            
    if not neutron_bin:
        Colors.print(f"Neutron binary not found. Please build the project first.", Colors.RED)
        sys.exit(1)

    Colors.print("╔════════════════════════════════╗", Colors.CYAN)
    Colors.print("║  Neutron Benchmark Suite v2.0 ║", Colors.CYAN)
    Colors.print("╚════════════════════════════════╝", Colors.CYAN)
    print()
    Colors.print(f"Neutron: {neutron_bin}", Colors.BLUE)
    Colors.print(f"Python: {sys.version.split()[0]}", Colors.BLUE)
    print()

    # Define benchmarks (matches run_benchmark.sh)
    categories = [
        ("Algorithms", [
            ("Fibonacci", "neutron/fibonacci.nt", "python/fibonacci.py"),
            ("Prime Numbers", "neutron/primes.nt", "python/primes.py"),
            ("Matrix Operations", "neutron/matrix.nt", "python/matrix.py")
        ]),
        ("Recursion & Function Calls", [
            ("Deep Recursion", "neutron/ackermann.nt", "python/ackermann.py")
        ]),
         ("Data Structures", [
            ("Sorting Algorithms", "neutron/sorting.nt", "python/sorting.py"),
            ("Array/List Operations", "neutron/list_ops.nt", "python/list_ops.py"),
            ("Object/Dictionary Operations", "neutron/dict_ops.nt", "python/dict_ops.py")
        ]),
        ("Math Operations", [
            ("Mathematical Functions", "neutron/math.nt", "python/math.py")
        ]),
        ("String Operations", [
             ("String Manipulation", "neutron/strings.nt", "python/strings.py"),
             ("Advanced String Operations", "neutron/string_ops.nt", "python/string_ops.py")
        ]),
        ("Loop Performance", [
            ("Loop Operations", "neutron/loops.nt", "python/loops.py"),
            ("Nested Loops", "neutron/nested_loops.nt", "python/nested_loops.py")
        ])
    ]

    total_benchmarks = 0
    
    for category, benches in categories:
        Colors.print(f"Benchmarking: {category}", Colors.BOLD)
        for name, n_file, p_file in benches:
             run_benchmark(
                 name, 
                 neutron_bin, 
                 os.path.join(bench_dir, n_file), 
                 os.path.join(bench_dir, p_file)
             )
             total_benchmarks += 1
        print()

    # Summary
    Colors.print("════ BENCHMARK SUMMARY ═══", Colors.CYAN)
    print(f"Total Benchmarks: {total_benchmarks}")
    print("Neutron Faster:   ", end="")
    Colors.print(f"{neutron_faster}", Colors.GREEN)
    print("Python Faster:    ", end="")
    Colors.print(f"{python_faster}", Colors.YELLOW)
    
    if failed_benchmarks > 0:
        print("Failed:           ", end="")
        Colors.print(f"{failed_benchmarks}", Colors.RED)
        
    print()
    
    if total_benchmarks > 0:
        success_rate = total_benchmarks - failed_benchmarks
        if success_rate > 0:
            win_rate = (neutron_faster * 100) / success_rate
            print("Neutron Win Rate: ", end="")
            Colors.print(f"{win_rate:.1f}%", Colors.GREEN)

    if failed_benchmarks > 0:
        sys.exit(1)
    else:
        print()
        Colors.print("🎉 All benchmarks completed successfully! 🎉", Colors.GREEN)

if __name__ == "__main__":
    main()
