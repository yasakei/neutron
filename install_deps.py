#!/usr/bin/env python3
import os
import sys
import platform
import subprocess
import shutil

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

def command_exists(cmd):
    return shutil.which(cmd) is not None

def check_package_config(pkg_name):
    if not command_exists("pkg-config"):
        return False
    try:
        subprocess.check_call(["pkg-config", "--exists", pkg_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except subprocess.CalledProcessError:
        return False

def main():
    Colors.print("Checking for required dependencies...", Colors.BLUE)
    
    system = platform.system()
    missing_deps = []
    
    common_deps = ["cmake", "git"]
    
    # Check common deps
    if not command_exists("cmake"):
        missing_deps.append("cmake")
    if not command_exists("git"):
        missing_deps.append("git")
        
    jsoncpp_installed = False
    
    install_cmd = ""
    
    if system == "Darwin":
        Colors.print("Detected macOS.", Colors.YELLOW)
        if not command_exists("clang++") and not command_exists("g++"):
            missing_deps.append("clang++")
            
        install_cmd = "brew install"
        
        # Check jsoncpp
        common_paths = [
            "/opt/homebrew/lib/libjsoncpp.dylib",
            "/usr/local/lib/libjsoncpp.dylib"
        ]
        if any(os.path.exists(p) for p in common_paths):
            jsoncpp_installed = True
            
        if not jsoncpp_installed:
             missing_deps.append("jsoncpp")

    elif system == "Linux":
        Colors.print("Detected Linux.", Colors.YELLOW)
        if not command_exists("g++") and not command_exists("clang++"):
            missing_deps.append("g++")
        
        if not command_exists("pkg-config"):
            missing_deps.append("pkg-config")

        # Distro detection (simple check)
        if os.path.exists("/etc/os-release"):
            with open("/etc/os-release") as f:
                content = f.read()
                if "ubuntu" in content or "debian" in content:
                    install_cmd = "sudo apt-get install -y"
                    jsoncpp_pkg = "libjsoncpp-dev"
                elif "arch" in content:
                    install_cmd = "sudo pacman -S --noconfirm"
                    jsoncpp_pkg = "jsoncpp"
                elif "fedora" in content:
                    install_cmd = "sudo dnf install -y"
                    jsoncpp_pkg = "jsoncpp-devel"
                else:
                    jsoncpp_pkg = "jsoncpp (dev)"
        else:
             jsoncpp_pkg = "jsoncpp (dev)"
             
        if check_package_config("jsoncpp"):
            jsoncpp_installed = True
            
        if not jsoncpp_installed:
            missing_deps.append(jsoncpp_pkg)

    elif system == "Windows":
        Colors.print("Detected Windows.", Colors.YELLOW)
        
        # Check for MSVC (cl.exe) or MinGW
        # actually for this new plan we prefer MSVC
        if not command_exists("cl") and not command_exists("g++"):
             missing_deps.append("Visual Studio (C++ Desktop Development) or MinGW")
             
        # Check for vcpkg or similar?
        # It's hard to check for libraries on Windows without running CMake.
        # We'll just assume they need to install them if they don't have them.
        
        install_cmd = "winget install"
        
        Colors.print("Note: On Windows, we recommend using Visual Studio 2019+ and vcpkg.", Colors.YELLOW)
        
    else:
        Colors.print(f"Unsupported OS: {system}", Colors.RED)
        sys.exit(1)

    if missing_deps:
        Colors.print("\nThe following dependencies appear to be missing:", Colors.RED)
        for dep in missing_deps:
            Colors.print(f"  - {dep}")
        
        Colors.print("\nYou can try to install them via:", Colors.BLUE)
        if system == "Windows":
             Colors.print("  Install Visual Studio Community with C++ workload.")
             Colors.print("  Install CMake: winget install Kitware.CMake")
             Colors.print("  Install Git: winget install Git.Git")
        else:
             deps_str = " ".join(missing_deps)
             Colors.print(f"  {install_cmd} {deps_str}")
             
        sys.exit(1)
    else:
        Colors.print("All dependencies seem to be present.", Colors.GREEN)
        Colors.print("\nBuild instructions:", Colors.BLUE)
        Colors.print("  python package.py")
        Colors.print("  # or manually:")
        Colors.print("  mkdir build && cd build")
        Colors.print("  cmake ..")
        Colors.print("  cmake --build . --config Release")

if __name__ == "__main__":
    main()
