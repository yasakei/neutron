#!/usr/bin/env python3
import os
import sys
import platform
import subprocess
import shutil
import glob
import tarfile
import argparse

# Colors
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

    @staticmethod
    def print(text, color=NC):
        if sys.stdout.isatty() and platform.system() != "Windows":
             print(f"{color}{text}{Colors.NC}")
        else:
             print(text)

def run_command(command, cwd=None, fail_exit=True):
    try:
        Colors.print(f"Running: {' '.join(command)}", Colors.BLUE)
        subprocess.check_call(command, cwd=cwd)
        return True
    except subprocess.CalledProcessError:
        Colors.print(f"Command failed: {' '.join(command)}", Colors.RED)
        if fail_exit:
            sys.exit(1)
        return False

def get_os_info():
    system = platform.system().lower()
    machine = platform.machine().lower()
    
    os_type = "unknown"
    arch_type = "unknown"
    package_name = ""

    if system == "darwin":
        os_type = "macos"
        if machine == "arm64":
            arch_type = "arm64"
        elif machine == "x86_64":
            arch_type = "intel"
        else:
            Colors.print(f"Unsupported macOS architecture: {machine}", Colors.RED)
            sys.exit(1)
    elif system == "linux":
        os_type = "linux"
        if machine == "x86_64":
            arch_type = "x64"
        elif machine in ["aarch64", "arm64"]:
            arch_type = "arm64"
        else:
            Colors.print(f"Unsupported Linux architecture: {machine}", Colors.RED)
            sys.exit(1)
    elif system == "windows":
        os_type = "windows"
        arch_type = "x64" # Assuming x64 for Windows usually
    else:
        Colors.print(f"Unsupported operating system: {system}", Colors.RED)
        sys.exit(1)
    
    return os_type, arch_type

def build_neutron(os_type, arch_type, build_dir="build"):
    Colors.print("Configuring and building Neutron...", Colors.BLUE)
    os.makedirs(build_dir, exist_ok=True)
    
    cmake_cmd = ["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"]
    
    if os_type == "windows":
        # Let CMake choose default generator (likely VS or Ninja)
        # Check for vcpkg toolchain environment variable from CI
        if "CMAKE_TOOLCHAIN_FILE" in os.environ:
             cmake_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={os.environ['CMAKE_TOOLCHAIN_FILE']}")
             
        # Also respect VCPKG_TARGET_TRIPLET if set
        if "VCPKG_TARGET_TRIPLET" in os.environ:
             cmake_cmd.append(f"-DVCPKG_TARGET_TRIPLET={os.environ['VCPKG_TARGET_TRIPLET']}")
             
        pass 
    elif os_type == "macos":
        if arch_type == "arm64":
            cmake_cmd.append("-DCMAKE_OSX_ARCHITECTURES=arm64")
        elif arch_type == "intel":
            cmake_cmd.append("-DCMAKE_OSX_ARCHITECTURES=x86_64")
            
        # Try to find jsoncpp (simplified logic compared to bash, assuming user environment is sane or vcpkg used)
        # If user has specific paths, they should set env vars or CMAKE_PREFIX_PATH
    
    # Configure
    if not run_command(cmake_cmd, cwd=build_dir):
        return False

    # Build
    build_cmd = ["cmake", "--build", ".", "--config", "Release"]
    # Parallel build
    import multiprocessing
    try:
        cpu_count = multiprocessing.cpu_count()
        build_cmd.append(f"-j{cpu_count}")
    except:
        pass

    if not run_command(build_cmd, cwd=build_dir):
        return False
    
    return True

def build_box(os_type, arch_type, build_dir="nt-box/build"):
    Colors.print("Configuring and building Box package manager...", Colors.BLUE)
    os.makedirs(build_dir, exist_ok=True)
    
    cmake_cmd = ["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"]
    
    if os_type == "windows":
        if "CMAKE_TOOLCHAIN_FILE" in os.environ:
             cmake_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={os.environ['CMAKE_TOOLCHAIN_FILE']}")
        if "VCPKG_TARGET_TRIPLET" in os.environ:
             cmake_cmd.append(f"-DVCPKG_TARGET_TRIPLET={os.environ['VCPKG_TARGET_TRIPLET']}")
    
    if os_type == "windows":
        if "CMAKE_TOOLCHAIN_FILE" in os.environ:
             cmake_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={os.environ['CMAKE_TOOLCHAIN_FILE']}")
        if "VCPKG_TARGET_TRIPLET" in os.environ:
             cmake_cmd.append(f"-DVCPKG_TARGET_TRIPLET={os.environ['VCPKG_TARGET_TRIPLET']}")
    
    # Configure
    if not run_command(cmake_cmd, cwd=build_dir):
        return False

    # Build
    build_cmd = ["cmake", "--build", ".", "--config", "Release"]
    if not run_command(build_cmd, cwd=build_dir):
        return False
        
    return True

def main():
    parser = argparse.ArgumentParser(description="Neutron Packaging Script")
    parser.add_argument("--output", help="Output directory name for the package", default=None)
    args = parser.parse_args()

    os_type, arch_type = get_os_info()
    Colors.print(f"Detected {os_type} {arch_type}", Colors.YELLOW)
    
    # Paths
    root_dir = os.getcwd()
    build_dir = os.path.join(root_dir, "build")
    box_build_dir = os.path.join(root_dir, "nt-box", "build")
    
    # Binaries check
    neutron_exe = "neutron.exe" if os_type == "windows" else "neutron"
    box_exe = "box.exe" if os_type == "windows" else "box"
    
    # On Windows with MSVC, binaries might be in Release/ subdirectory
    neutron_bin_paths = [
        os.path.join(build_dir, neutron_exe),
        os.path.join(build_dir, "Release", neutron_exe),
        os.path.join(build_dir, "Debug", neutron_exe)
    ]
    
    box_bin_paths = [
        os.path.join(box_build_dir, box_exe),
        os.path.join(box_build_dir, "Release", box_exe),
        os.path.join(box_build_dir, "Debug", box_exe)
    ]
    
    # Check if we need to build
    need_build = True
    real_neutron_path = None
    real_box_path = None

    for p in neutron_bin_paths:
        if os.path.exists(p):
            real_neutron_path = p
            break
            
    for p in box_bin_paths:
        if os.path.exists(p):
            real_box_path = p
            break
            
    if real_neutron_path and real_box_path:
        # Prompt logic could go here, but for now we auto-build if missing or just rebuild to be safe?
        # The bash script checks if files strictly don't exist.
        Colors.print("Binaries found, but rebuilding to ensure latest version...", Colors.YELLOW)
        
    build_neutron(os_type, arch_type, build_dir)
    build_box(os_type, arch_type, box_build_dir)
    
    # Re-find binaries after build
    real_neutron_path = None
    real_box_path = None
    
    for p in neutron_bin_paths:
        if os.path.exists(p):
            real_neutron_path = p
            break
    for p in box_bin_paths:
        if os.path.exists(p):
            real_box_path = p
            break
            
    if not real_neutron_path or not real_box_path:
        Colors.print("Build seemed successful but could not locate binaries!", Colors.RED)
        sys.exit(1)

    # Packaging
    if args.output:
        target_name = args.output
    else:
        target_name = f"neutron-{os_type}-{arch_type}"
        
    Colors.print(f"Creating package: {target_name}", Colors.BLUE)
    
    if os.path.exists(target_name):
        shutil.rmtree(target_name)
    os.makedirs(target_name)
    
    # Copy binaries
    shutil.copy2(real_neutron_path, target_name)
    shutil.copy2(real_box_path, target_name)
    
    # Libraries stuff
    if os_type == "windows":
        # Copy DLLs from build dir if any
        # With MSVC dynamic linking, we might rely on system installed runtimes or vcpkg dlls
        # Search for .dll in build dir
        for dll in glob.glob(os.path.join(build_dir, "*.dll")):
             shutil.copy2(dll, target_name)
        for dll in glob.glob(os.path.join(build_dir, "Release", "*.dll")):
             shutil.copy2(dll, target_name)
    else:
        # Unix
        # Copy libneutron_runtime
        lib_extensions = ["st", "so", "dylib", "dll", "a"]
        for ext in lib_extensions:
            for f in glob.glob(os.path.join(build_dir, f"*.{ext}*")):
                shutil.copy2(f, target_name)
    
    # Copy assets
    for item in ["README.md", "LICENSE", "docs", "include", "src", "libs"]:
        src_path = os.path.join(root_dir, item)
        if os.path.exists(src_path):
            dst_path = os.path.join(target_name, item)
            if os.path.isdir(src_path):
                shutil.copytree(src_path, dst_path)
            else:
                shutil.copy2(src_path, target_name)

    # Version check
    try:
        output = subprocess.check_output([real_neutron_path, "--version"]).decode().strip()
        # Simple extraction if output is like "Neutron 1.2.3"
        import re
        version = "unknown"
        m = re.search(r'(\d+\.\d+\.\d+)', output)
        if m:
            version = m.group(1)
    except:
        version = "unknown"

    Colors.print(f"Version: {version}", Colors.GREEN)
    
    # Archive
    # Archive skipped as per user request
    # Instead of creating zip/tar, we just leave the directory there
    Colors.print(f"Build artifacts are in directory: {target_name}", Colors.GREEN)
    
    # Optional: NSIS Installer (keep if simple, or remove if considered "packaging")
    # User said "remove zipping support", but installer is useful. 
    # Let's keep NSIS check if on Windows but DO NOT ZIP.
    if os_type == "windows" and shutil.which("makensis"):
         Colors.print("Building Windows Installer...", Colors.BLUE)
         if os.path.exists("installer.nsi"):
             subprocess.call(["makensis", f"/DVERSION={version}", "installer.nsi"])
             if os.path.exists("NeutronInstaller.exe"):
                 inst_name = f"neutron-{version}-installer.exe"
                 if os.path.exists(inst_name):
                     os.remove(inst_name)
                 shutil.move("NeutronInstaller.exe", inst_name)
                 Colors.print(f"Installer created: {inst_name}", Colors.GREEN)

    Colors.print("Packaging flow completed successfully!", Colors.GREEN)

if __name__ == "__main__":
    main()
