#!/usr/bin/env python3
import os
import sys
import platform
import subprocess
import glob
import shutil

# Colors
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    BOLD = '\033[1m'
    NC = '\033[0m'

    @staticmethod
    def print(text, color=NC, end='\n'):
        # Handle encoding issues on Windows
        try:
            if sys.stdout.isatty() and platform.system() != "Windows":
                print(f"{color}{text}{Colors.NC}", end=end)
            else:
                print(text, end=end)
        except UnicodeEncodeError:
            # Fallback: encode with error handling
            if sys.stdout.isatty() and platform.system() != "Windows":
                safe_text = f"{color}{text}{Colors.NC}".encode(sys.stdout.encoding, errors='replace').decode(sys.stdout.encoding)
            else:
                safe_text = text.encode(sys.stdout.encoding, errors='replace').decode(sys.stdout.encoding)
            print(safe_text, end=end)

def build_neutron(root_dir):
    """Build neutron if it doesn't exist"""
    is_windows = platform.system() == "Windows"
    exe_name = "neutron.exe" if is_windows else "neutron"
    
    # Check if neutron binary exists
    possible_paths = [
        os.path.join(root_dir, "build", exe_name),
        os.path.join(root_dir, "build", "Release", exe_name),
        os.path.join(root_dir, "build", "Debug", exe_name),
        os.path.join(root_dir, exe_name)
    ]
    
    for p in possible_paths:
        if os.path.exists(p):
            Colors.print(f"Found existing neutron binary: {p}", Colors.CYAN)
            return p
    
    # Binary not found, try to build it
    Colors.print("Neutron binary not found. Building...", Colors.YELLOW)
    
    build_dir = os.path.join(root_dir, "build")
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
    
    try:
        # Configure
        Colors.print("Configuring build...", Colors.BLUE)
        cmake_cmd = ["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"]
        result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
        if result.returncode != 0:
            Colors.print("CMake configure failed:", Colors.RED)
            print(result.stderr)
            return None
        
        # Build
        Colors.print("Building neutron...", Colors.BLUE)
        build_cmd = ["cmake", "--build", ".", "--config", "Release"]
        result = subprocess.run(build_cmd, cwd=build_dir, capture_output=True, text=True)
        if result.returncode != 0:
            Colors.print("Build failed:", Colors.RED)
            print(result.stderr)
            return None
        
        # Check if binary was created
        for p in possible_paths:
            if os.path.exists(p):
                Colors.print(f"Successfully built neutron: {p}", Colors.GREEN)
                return p
        
        Colors.print("Build completed but binary not found", Colors.RED)
        return None
        
    except Exception as e:
        Colors.print(f"Build failed with exception: {e}", Colors.RED)
        return None

def run_quark_test(neutron_bin, root_dir):
    """Run quark project test"""
    Colors.print("Testing: quark project", Colors.BLUE)
    
    quark_dir = os.path.join(root_dir, "tests", "quark")
    if not os.path.exists(quark_dir):
        print(f"  ", end="")
        Colors.print("[SKIP]", Colors.YELLOW, end="")
        print(f" quark directory not found")
        return 0, 0
    
    # Check if .quark file exists
    quark_file = os.path.join(quark_dir, ".quark")
    if not os.path.exists(quark_file):
        print(f"  ", end="")
        Colors.print("[SKIP]", Colors.YELLOW, end="")
        print(f" .quark file not found")
        return 0, 0
    
    # Check if main.nt exists
    main_file = os.path.join(quark_dir, "main.nt")
    if not os.path.exists(main_file):
        print(f"  ", end="")
        Colors.print("[SKIP]", Colors.YELLOW, end="")
        print(f" main.nt file not found")
        return 0, 0
    
    try:
        # Run neutron with main.nt in the quark project directory
        result = subprocess.run([neutron_bin, "main.nt"], cwd=quark_dir, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"  ", end="")
            Colors.print("[PASS]", Colors.GREEN, end="")
            print(f" quark project")
            return 1, 0
        else:
            print(f"  ", end="")
            Colors.print("[FAIL]", Colors.RED, end="")
            print(f" quark project")
            output = result.stdout + result.stderr
            for line in output.splitlines():
                print(f"    {line}")
            return 0, 1
            
    except Exception as e:
        print(f"  ", end="")
        Colors.print("[FAIL]", Colors.RED, end="")
        print(f" quark project (Exception)")
        print(f"    {str(e)}")
        return 0, 1

def run_box_test(neutron_bin, root_dir):
    """Run box module installation and test"""
    Colors.print("Testing: box module", Colors.BLUE)
    
    # Find box binary
    is_windows = platform.system() == "Windows"
    box_name = "box.exe" if is_windows else "box"
    
    box_paths = [
        os.path.join(root_dir, "build", box_name),
        os.path.join(root_dir, "build", "Release", box_name),
        os.path.join(root_dir, "build", "Debug", box_name),
        os.path.join(root_dir, "nt-box", "build", box_name),
        os.path.join(root_dir, "nt-box", "build", "Release", box_name),
        os.path.join(root_dir, "nt-box", "build", "Debug", box_name),
    ]
    
    box_bin = None
    for p in box_paths:
        if os.path.exists(p):
            box_bin = p
            break
    
    # Check system PATH
    if not box_bin:
        try:
            which_cmd = "where" if is_windows else "which"
            result = subprocess.run([which_cmd, "box"], capture_output=True)
            if result.returncode == 0:
                box_bin = "box"
        except:
            pass
    
    if not box_bin:
        print(f"  ", end="")
        Colors.print("[SKIP]", Colors.YELLOW, end="")
        print(f" box binary not found")
        return 0, 0
    
    try:
        # Install base64 module
        result = subprocess.run([box_bin, "install", "base64"], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  ", end="")
            Colors.print("[FAIL]", Colors.RED, end="")
            print(f" box install base64")
            print(f"    {result.stderr}")
            return 0, 1
        
        # Run the test
        test_file = os.path.join(root_dir, "tests", "box_base64_test.nt")
        result = subprocess.run([neutron_bin, test_file], capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"  ", end="")
            Colors.print("[PASS]", Colors.GREEN, end="")
            print(f" box_base64_test")
            return 1, 0
        else:
            print(f"  ", end="")
            Colors.print("[FAIL]", Colors.RED, end="")
            print(f" box_base64_test")
            output = result.stdout + result.stderr
            for line in output.splitlines():
                print(f"    {line}")
            return 0, 1
            
    except Exception as e:
        print(f"  ", end="")
        Colors.print("[FAIL]", Colors.RED, end="")
        print(f" box_base64_test (Exception)")
        print(f"    {str(e)}")
        return 0, 1

def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root_dir)

    # Build neutron if needed
    neutron_bin = build_neutron(root_dir)
    if not neutron_bin:
        Colors.print("Error: Could not find or build neutron binary.", Colors.RED)
        sys.exit(1)
        
    Colors.print(f"Using binary: {neutron_bin}", Colors.CYAN)

    test_dirs = [
        "tests/fixes",
        "tests/core", 
        "tests/operators",
        "tests/control-flow",
        "tests/functions",
        "tests/classes",
        "tests/modules",
        "benchmarks/neutron"
    ]
    
    total_passed = 0
    total_failed = 0
    failed_tests = []
    
    padding = "+" + "=" * 32 + "+"
    Colors.print(padding, Colors.CYAN)
    Colors.print("|  Neutron Test Suite v2.0 (Py)  |", Colors.CYAN)
    Colors.print("+" + "=" * 32 + "+", Colors.CYAN)
    print() # newline

    for test_dir in test_dirs:
        full_dir = os.path.join(root_dir, test_dir)
        if not os.path.isdir(full_dir):
            continue
            
        test_files = glob.glob(os.path.join(full_dir, "*.nt"))
        test_files += glob.glob(os.path.join(full_dir, "*.ntsc"))
        if not test_files:
            continue
            
        dir_name = os.path.basename(test_dir)
        Colors.print(f"Testing: {dir_name}", Colors.BLUE)
        
        dir_passed = 0
        dir_failed = 0
        
        for test_file in test_files:
            test_name = os.path.splitext(os.path.basename(test_file))[0]
            
            try:
                # Run the test
                # Capture output
                result = subprocess.run(
                    [neutron_bin, test_file],
                    capture_output=True,
                    text=True,
                    encoding='utf-8',
                    errors='replace'
                )
                
                if result.returncode == 0:
                    print(f"  ", end="")
                    Colors.print("[PASS]", Colors.GREEN, end="")
                    print(f" {test_name}")
                    dir_passed += 1
                    total_passed += 1
                else:
                    print(f"  ", end="")
                    Colors.print("[FAIL]", Colors.RED, end="")
                    print(f" {test_name}")
                    
                    # Print stderr/stdout indented
                    output = result.stdout + result.stderr
                    for line in output.splitlines():
                        print(f"    {line}")
                        
                    failed_tests.append(os.path.relpath(test_file, root_dir))
                    dir_failed += 1
                    total_failed += 1
                    
            except Exception as e:
                print(f"  ", end="")
                Colors.print("[FAIL]", Colors.RED, end="")
                print(f" {test_name} (Exception)")
                print(f"    {str(e)}")
                failed_tests.append(os.path.relpath(test_file, root_dir))
                dir_failed += 1
                total_failed += 1
        
        print("  Summary: ", end="")
        Colors.print(f"{dir_passed} passed", Colors.GREEN, end="")
        print(", ", end="")
        Colors.print(f"{dir_failed} failed", Colors.RED)
        print()

    # Run box test
    box_passed, box_failed = run_box_test(neutron_bin, root_dir)
    total_passed += box_passed
    total_failed += box_failed
    if box_failed > 0:
        failed_tests.append("tests/box_base64_test.nt")
    print("  Summary: ", end="")
    Colors.print(f"{box_passed} passed", Colors.GREEN, end="")
    print(", ", end="")
    Colors.print(f"{box_failed} failed", Colors.RED)
    print()

    # Run quark test
    quark_passed, quark_failed = run_quark_test(neutron_bin, root_dir)
    total_passed += quark_passed
    total_failed += quark_failed
    if quark_failed > 0:
        failed_tests.append("tests/quark")
    print("  Summary: ", end="")
    Colors.print(f"{quark_passed} passed", Colors.GREEN, end="")
    print(", ", end="")
    Colors.print(f"{quark_failed} failed", Colors.RED)
    print()

    # Final summary
    Colors.print("==== FINAL SUMMARY ====", Colors.CYAN)
    print(f"Total: {total_passed + total_failed}")
    print("Passed: ", end="")
    Colors.print(f"{total_passed}", Colors.GREEN)
    
    if total_failed > 0:
        print("Failed: ", end="")
        Colors.print(f"{total_failed}", Colors.RED)
        print()
        Colors.print("Failed tests:", Colors.RED)
        for t in failed_tests:
            Colors.print(f"  - {t}", Colors.RED)
        sys.exit(1)
    else:
        print("Failed: ", end="")
        Colors.print("0", Colors.RED)
        print()
        Colors.print("*** All tests passed! ***", Colors.GREEN)
        sys.exit(0)

if __name__ == "__main__":
    main()
