#!/usr/bin/env python3
import os
import sys
import platform
import subprocess
import time
import io
import shutil

# Force UTF-8 output on Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

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
        # Use ASCII-safe output for CI environments
        try:
            if sys.stdout.isatty() and platform.system() != "Windows":
                print(f"{color}{text}{Colors.NC}", end=end)
            else:
                print(text, end=end)
        except UnicodeEncodeError:
            # Fallback to ASCII representation
            ascii_text = text.encode('ascii', 'replace').decode('ascii')
            print(ascii_text, end=end)

def run_benchmark(name, neutron_bin, neutron_file, python_file, js_file=None):
    results = {}
    
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
            results['python'] = {
                'time': python_time,
                'success': True,
                'output': python_result.stdout.strip(),
                'display': f"{python_time:.3f}s"
            }
        else:
            results['python'] = {
                'time': 0,
                'success': False,
                'output': "",
                'display': "FAILED"
            }
    except Exception as e:
        results['python'] = {
            'time': 0,
            'success': False,
            'output': "",
            'display': "FAILED"
        }

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
            results['neutron'] = {
                'time': neutron_time,
                'success': True,
                'output': neutron_result.stdout.strip(),
                'display': f"{neutron_time:.3f}s"
            }
        else:
            results['neutron'] = {
                'time': 0,
                'success': False,
                'output': "",
                'display': "FAILED"
            }
    except Exception as e:
        results['neutron'] = {
            'time': 0,
            'success': False,
            'output': "",
            'display': "FAILED"
        }

    # Run JS version with Bun (if available and file exists)
    if js_file and os.path.exists(js_file) and shutil.which('bun'):
        js_start = time.time()
        try:
            js_result = subprocess.run(
                ['bun', 'run', js_file],
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace'
            )
            js_end = time.time()
            
            if js_result.returncode == 0:
                js_time = js_end - js_start
                results['javascript'] = {
                    'time': js_time,
                    'success': True,
                    'output': js_result.stdout.strip(),
                    'display': f"{js_time:.3f}s"
                }
            else:
                results['javascript'] = {
                    'time': 0,
                    'success': False,
                    'output': "",
                    'display': "FAILED"
                }
        except Exception as e:
            results['javascript'] = {
                'time': 0,
                'success': False,
                'output': "",
                'display': "FAILED"
            }
    elif js_file and os.path.exists(js_file):
        results['javascript'] = {
            'time': 0,
            'success': False,
            'output': "",
            'display': "NO BUN"
        }
    else:
        results['javascript'] = {
            'time': 0,
            'success': False,
            'output': "",
            'display': "N/A"
        }

    # Compare outputs and calculate performance
    global neutron_faster, python_faster, js_faster, failed_benchmarks
    
    # Check if outputs match (only for successful runs)
    successful_runs = [k for k, v in results.items() if v['success']]
    output_match = True
    color = Colors.NC  # Default color
    
    if len(successful_runs) > 1:
        base_output = results[successful_runs[0]]['output']
        for lang in successful_runs[1:]:
            if results[lang]['output'] != base_output:
                output_match = False
                break
    
    if not output_match and len(successful_runs) > 1:
        failed_benchmarks += 1
        result_str = "OUTPUT MISMATCH"
        color = Colors.RED
    elif len(successful_runs) == 0:
        failed_benchmarks += 1
        result_str = "ALL FAILED"
        color = Colors.RED
    elif len(successful_runs) == 1:
        # Only one language succeeded
        fastest_lang = successful_runs[0]
        if fastest_lang == 'neutron':
            neutron_faster += 1
            color = Colors.GREEN
        elif fastest_lang == 'python':
            python_faster += 1
            color = Colors.YELLOW
        elif fastest_lang == 'javascript':
            js_faster += 1
            color = Colors.CYAN
        result_str = f"{fastest_lang[:3].upper()} only"
    else:
        # Find the fastest successful run
        fastest_lang = None
        fastest_time = float('inf')
        
        for lang in successful_runs:
            if results[lang]['time'] < fastest_time:
                fastest_time = results[lang]['time']
                fastest_lang = lang
        
        if fastest_lang == 'neutron':
            neutron_faster += 1
            color = Colors.GREEN
        elif fastest_lang == 'python':
            python_faster += 1
            color = Colors.YELLOW
        elif fastest_lang == 'javascript':
            js_faster += 1
            color = Colors.CYAN
        
        # Calculate ratios
        ratios = []
        for lang in successful_runs:
            if lang != fastest_lang and results[lang]['time'] > 0:
                ratio = results[lang]['time'] / fastest_time
                ratios.append(f"{ratio:.1f}x")
        
        # Create readable language names
        lang_names = {
            'neutron': 'Neutron',
            'python': 'Python', 
            'javascript': 'Bun/JS'
        }
        
        if ratios:
            result_str = f"{lang_names[fastest_lang]} ({'/'.join(ratios)})"
        else:
            result_str = f"{lang_names[fastest_lang]} wins"
    
    # Print table row
    print(f"│ {name:<20} │", end="")
    print(f" {results['python']['display']:<8} │", end="")
    print(f" {results['neutron']['display']:<8} │", end="")
    print(f" {results['javascript']['display']:<8} │", end="")
    Colors.print(f" {result_str:<25} │", color)
    
    if not output_match and len(successful_runs) > 1:
        Colors.print("    Output mismatch detected:", Colors.RED)
        for lang in successful_runs:
            Colors.print(f"    {lang.title()} output:", Colors.RED)
            for line in results[lang]['output'].splitlines():
                print(f"      {line}")

neutron_faster = 0
python_faster = 0
js_faster = 0
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

    # Check for Bun
    bun_available = shutil.which('bun') is not None
    
    Colors.print("┌" + "─" * 78 + "┐", Colors.CYAN)
    Colors.print("│" + " " * 25 + "Neutron Benchmark Suite v3.0" + " " * 24 + "│", Colors.CYAN)
    Colors.print("└" + "─" * 78 + "┘", Colors.CYAN)
    print()
    Colors.print(f"🚀 Neutron: {neutron_bin}", Colors.BLUE)
    Colors.print(f"🐍 Python:  {sys.version.split()[0]}", Colors.BLUE)
    if bun_available:
        try:
            bun_version = subprocess.run(['bun', '--version'], capture_output=True, text=True)
            bun_ver = bun_version.stdout.strip() if bun_version.returncode == 0 else "unknown"
            Colors.print(f"⚡ Bun:     {bun_ver}", Colors.BLUE)
        except:
            Colors.print(f"⚡ Bun:     available", Colors.BLUE)
    else:
        Colors.print(f"⚡ Bun:     not available (install from https://bun.sh)", Colors.YELLOW)
    print()

    # Table header
    Colors.print("┌──────────────────────┬──────────┬──────────┬──────────┬───────────────────────────┐", Colors.CYAN)
    Colors.print("│ Benchmark            │ Python   │ Neutron  │ Bun/JS   │ Result                    │", Colors.CYAN)
    Colors.print("├──────────────────────┼──────────┼──────────┼──────────┼───────────────────────────┤", Colors.CYAN)

    # Define benchmarks with JS files
    categories = [
        ("Algorithms", [
            ("Fibonacci", "neutron/fibonacci.nt", "python/fibonacci.py", "javascript/fibonacci.js"),
            ("Prime Numbers", "neutron/primes.nt", "python/primes.py", "javascript/primes.js"),
            ("Matrix Ops", "neutron/matrix.nt", "python/matrix.py", "javascript/matrix.js")
        ]),
        ("Recursion", [
            ("Deep Recursion", "neutron/ackermann.nt", "python/ackermann.py", "javascript/ackermann.js"),
            ("Recursive Algos", "neutron/recursion.nt", "python/recursion.py", "javascript/recursion.js")
        ]),
         ("Data Structures", [
            ("Sorting", "neutron/sorting.nt", "python/sorting.py", "javascript/sorting.js"),
            ("Array/List Ops", "neutron/list_ops.nt", "python/list_ops.py", "javascript/list_ops.js"),
            ("Object/Dict Ops", "neutron/dict_ops.nt", "python/dict_ops.py", "javascript/dict_ops.js")
        ]),
        ("Math Operations", [
            ("Math Functions", "neutron/math.nt", "python/math.py", "javascript/math.js"),
            ("Bitwise Ops", "neutron/bitwise.nt", "python/bitwise.py", "javascript/bitwise.js")
        ]),
        ("String Operations", [
             ("String Manipulation", "neutron/strings.nt", "python/strings.py", "javascript/strings.js"),
             ("Advanced Strings", "neutron/string_ops.nt", "python/string_ops.py", "javascript/string_ops.js"),
             ("String Concat", "neutron/string_concat.nt", "python/string_concat.py", "javascript/string_concat.js")
        ]),
        ("Object Operations", [
            ("Object Creation", "neutron/objects.nt", "python/objects.py", "javascript/objects.js")
        ]),
        ("Loop Performance", [
            ("Loop Operations", "neutron/loops.nt", "python/loops.py", "javascript/loops.js"),
            ("Nested Loops", "neutron/nested_loops.nt", "python/nested_loops.py", "javascript/nested_loops.js")
        ])
    ]

    total_benchmarks = 0
    
    for category, benches in categories:
        # Category separator
        Colors.print("├──────────────────────┼──────────┼──────────┼──────────┼───────────────────────────┤", Colors.CYAN)
        Colors.print(f"│ {category:<20} │          │          │          │                           │", Colors.BOLD)
        Colors.print("├──────────────────────┼──────────┼──────────┼──────────┼───────────────────────────┤", Colors.CYAN)
        
        for name, n_file, p_file, js_file in benches:
             js_path = os.path.join(bench_dir, js_file) if js_file else None
             run_benchmark(
                 name, 
                 neutron_bin, 
                 os.path.join(bench_dir, n_file), 
                 os.path.join(bench_dir, p_file),
                 js_path
             )
             total_benchmarks += 1

    # Table footer
    Colors.print("└──────────────────────┴──────────┴──────────┴──────────┴───────────────────────────┘", Colors.CYAN)
    print()

    # Summary
    print()
    Colors.print("🎯 BENCHMARK SUMMARY", Colors.CYAN)
    print(f"Total Benchmarks: {total_benchmarks}")
    print()
    print("Winners:")
    Colors.print(f"  🚀 Neutron:  {neutron_faster} ({(neutron_faster * 100 / (total_benchmarks - failed_benchmarks)):.1f}%)" if total_benchmarks > failed_benchmarks else f"  🚀 Neutron:  {neutron_faster}", Colors.GREEN)
    Colors.print(f"  🐍 Python:   {python_faster} ({(python_faster * 100 / (total_benchmarks - failed_benchmarks)):.1f}%)" if total_benchmarks > failed_benchmarks else f"  🐍 Python:   {python_faster}", Colors.YELLOW)
    Colors.print(f"  ⚡ Bun/JS:   {js_faster} ({(js_faster * 100 / (total_benchmarks - failed_benchmarks)):.1f}%)" if total_benchmarks > failed_benchmarks else f"  ⚡ Bun/JS:   {js_faster}", Colors.CYAN)
    
    if failed_benchmarks > 0:
        Colors.print(f"  ❌ Failed:   {failed_benchmarks}", Colors.RED)

    if failed_benchmarks > 0:
        print()
        Colors.print("⚠️  Some benchmarks failed. Check the output above for details.", Colors.YELLOW)
        sys.exit(1)
    else:
        print()
        Colors.print("🎉 All benchmarks completed successfully! 🎉", Colors.GREEN)

if __name__ == "__main__":
    main()
