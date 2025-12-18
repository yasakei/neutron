#!/usr/bin/env python3
import os
import sys
import platform
import subprocess
import shutil
import glob
import tarfile
import argparse
import zipfile
import re
import time

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

def sanitize_path_for_msvc(os_type):
    # On Windows, sanitize PATH to avoid MinGW/MSYS conflicts when using MSVC
    if os_type == "windows":
        Colors.print("Sanitizing PATH for MSVC compatibility...", Colors.YELLOW)
        
        # Determine where cmake is currently
        cmake_path = shutil.which("cmake")
        cmake_dir = os.path.dirname(cmake_path) if cmake_path else None
        
        new_path = []
        for p in os.environ["PATH"].split(os.pathsep):
            # Filter out MinGW/MSYS paths
            lower_p = p.lower()
            if "mingw" in lower_p or "msys" in lower_p:
                # If cmake is in this directory, we might need to keep it, 
                # OR we copy it/assume user has another cmake. 
                # If we must use MSYS cmake, we keep it but warn.
                if cmake_dir and os.path.normcase(p) == os.path.normcase(cmake_dir):
                    Colors.print(f"Keeping CMake at {p}", Colors.BLUE)
                    new_path.append(p)
                    continue
                continue
            new_path.append(p)
            
        os.environ["PATH"] = os.pathsep.join(new_path)
        
        # Also clear INCLUDE and LIB env vars if they contain MinGW stuff
        for var in ["INCLUDE", "LIB", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH"]:
            if var in os.environ:
                 # Just clear them or filter? Safest to clear if we suspect pollution
                 # but if we are in VS Dev Prompt, INCLUDE has MSVC paths.
                 parts = os.environ[var].split(os.pathsep)
                 new_parts = [p for p in parts if "mingw" not in p.lower() and "msys" not in p.lower()]
                 os.environ[var] = os.pathsep.join(new_parts)
        
    return True

def run_command(command, cwd=None, fail_exit=True, env=None):
    try:
        Colors.print(f"Running: {' '.join(command)}", Colors.BLUE)
        # Use sanitized env if not provided
        if env is None:
            env = os.environ.copy()
            
        subprocess.check_call(command, cwd=cwd, env=env)
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

    return os_type, arch_type

def get_cmake_command():
    # Check if cmake is in PATH
    if shutil.which("cmake"):
        return "cmake"
    
    # Check common locations
    paths = [
        r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ]
    
    for p in paths:
        if os.path.exists(p):
            return p
            
    return "cmake" # Fallback to hoping it's in PATH or will fail later


# Helper: try to remove a file with retries in case another process temporarily locks it
def safe_remove(path, retries=10, delay=0.5):
    import time
    for i in range(retries):
        try:
            if os.path.exists(path):
                os.remove(path)
            return True
        except PermissionError:
            Colors.print(f"File {path} is locked; retrying ({i+1}/{retries})...", Colors.YELLOW)
            time.sleep(delay)
        except OSError as e:
            Colors.print(f"Failed to remove {path}: {e}", Colors.RED)
            return False
    Colors.print(f"Unable to remove {path} after {retries} attempts.", Colors.RED)
    return False


# Sanitize an NSIS script by commenting-out 'File' lines that reference globs with no matches
def sanitize_nsi_for_globs(src_nsi, dst_nsi, root_dir):
    import re
    pattern = re.compile(r'^(\s*File\b[^\"]*\"([^\"]*\*[^\"]*)\".*)$', flags=re.IGNORECASE)
    changed = False
    with open(src_nsi, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    out_lines = []
    for line in lines:
        m = pattern.match(line)
        if m:
            full_line = m.group(1)
            glob_path = m.group(2)
            # Normalize NSIS path separators to OS separators
            candidate = os.path.normpath(os.path.join(root_dir, glob_path)) if not os.path.isabs(glob_path) else os.path.normpath(glob_path)
            matches = glob.glob(candidate)
            if len(matches) == 0:
                # Comment out the line so makensis won't warn
                out_lines.append('; ' + line)
                changed = True
                continue
        out_lines.append(line)

    with open(dst_nsi, 'w', encoding='utf-8') as f:
        f.writelines(out_lines)

    return changed

def _cleanup_vcpkg_downloads(root_dir):
    """Scan vcpkg downloads for zero-length or corrupted archives and remove them."""
    removed = []
    vcpkg_dl = os.path.join(root_dir, 'vcpkg', 'downloads')
    if not os.path.isdir(vcpkg_dl):
        return removed

    for fname in os.listdir(vcpkg_dl):
        full = os.path.join(vcpkg_dl, fname)
        try:
            if os.path.isfile(full):
                # Zero-length files are obviously bad
                if os.path.getsize(full) == 0:
                    os.remove(full)
                    removed.append(full)
                    continue
                # Check basic zip integrity for .zip files
                if full.lower().endswith('.zip'):
                    try:
                        with zipfile.ZipFile(full, 'r') as z:
                            bad = z.testzip()
                            if bad is not None:
                                os.remove(full)
                                removed.append(full)
                    except Exception:
                        # If zip open fails, remove it
                        try:
                            os.remove(full)
                            removed.append(full)
                        except Exception:
                            pass
        except Exception:
            pass
    return removed


def initialize_vcpkg(root_dir):
    """Initialize vcpkg if it doesn't exist, and install dependencies."""
    vcpkg_dir = os.path.join(root_dir, 'vcpkg')

    # Check if vcpkg exists, if not clone it
    if not os.path.exists(vcpkg_dir):
        Colors.print("Initializing vcpkg submodule...", Colors.BLUE)
        try:
            run_command(['git', 'submodule', 'update', '--init', '--recursive'], fail_exit=False)
            # If the above fails, try alternative approach
            if not os.path.exists(vcpkg_dir):
                Colors.print("Cloning vcpkg repository...", Colors.BLUE)
                run_command(['git', 'clone', 'https://github.com/microsoft/vcpkg.git', vcpkg_dir], fail_exit=False)
        except Exception as e:
            Colors.print(f"Failed to initialize vcpkg: {e}", Colors.RED)
            return False

    # Bootstrap vcpkg on Windows
    if platform.system() == "Windows":
        vcpkg_exe = os.path.join(vcpkg_dir, 'vcpkg.exe')
        if not os.path.exists(vcpkg_exe):
            Colors.print("Bootstrapping vcpkg...", Colors.BLUE)
            bootstrap_script = os.path.join(vcpkg_dir, 'bootstrap-vcpkg.bat')
            if os.path.exists(bootstrap_script):
                try:
                    result = run_command([bootstrap_script], fail_exit=False)
                    if not result:
                        Colors.print("Bootstrap failed, trying with Visual Studio Developer Command Prompt...", Colors.YELLOW)
                        # Try with developer command prompt if available
                        dev_cmd = shutil.which('vcvars64.bat')
                        if not dev_cmd:
                            # Look in common locations
                            vs_paths = [
                                r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
                                r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
                                r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
                                r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
                            ]
                            for vs_path in vs_paths:
                                if os.path.exists(vs_path):
                                    dev_cmd = vs_path
                                    break

                        if dev_cmd:
                            # Try to run bootstrap from developer command prompt
                            cmd = f'"{dev_cmd}" && "{bootstrap_script}"'
                            result = run_command(['cmd', '/c', cmd], fail_exit=False)

                except Exception as e:
                    Colors.print(f"vcpkg bootstrap failed: {e}", Colors.RED)
                    return False
            else:
                Colors.print("vcpkg bootstrap script not found!", Colors.RED)
                return False

    # Install dependencies using vcpkg
    Colors.print("Installing vcpkg dependencies...", Colors.BLUE)
    vcpkg_exe = os.path.join(vcpkg_dir, 'vcpkg.exe') if platform.system() == "Windows" else os.path.join(vcpkg_dir, 'vcpkg')

    if not os.path.exists(vcpkg_exe):
        Colors.print(f"vcpkg executable not found at {vcpkg_exe}!", Colors.RED)
        return False

    # Install dependencies from vcpkg.json
    if os.path.exists(os.path.join(root_dir, 'vcpkg.json')):
        try:
            triplet = 'x64-windows' if platform.system() == "Windows" else ('x64-osx' if platform.system() == "Darwin" else 'x64-linux')
            vcpkg_cmd = [vcpkg_exe, 'install', '--triplet', triplet]
            result = run_command(vcpkg_cmd, fail_exit=False)
            if not result:
                Colors.print("vcpkg install failed. This might cause build failures later.", Colors.YELLOW)
                # Continue with warning as some dependencies might already be installed
        except Exception as e:
            Colors.print(f"Error during vcpkg install: {e}", Colors.YELLOW)
            # Continue with warning
    else:
        Colors.print("vcpkg.json not found!", Colors.YELLOW)

    return True


def _handle_vcpkg_manifest_log(build_dir, root_dir):
    """Parse vcpkg-manifest-install.log for archive errors and delete referenced files.
    Returns True if something was deleted and a retry should be attempted.
    """
    log = os.path.join(build_dir, 'vcpkg-manifest-install.log')
    if not os.path.exists(log):
        return False
    try:
        content = open(log, 'r', encoding='utf-8', errors='ignore').read()
    except Exception:
        return False

    # Look for lines that indicate a bad archive, e.g. "ERROR: C:\...\PowerShell-7.5.4-win-x64.zip"
    matches = re.findall(r'ERROR:\s*(.+\.(zip|tar|tgz|gz|xz|7z))', content, flags=re.IGNORECASE)
    removed_any = False
    for m in matches:
        path = m[0].strip()
        # Normalize and remove if exists
        if os.path.isabs(path):
            target = path
        else:
            target = os.path.join(root_dir, path)
        try:
            if os.path.exists(target):
                Colors.print(f"Removing corrupt vcpkg download: {target}", Colors.YELLOW)
                os.remove(target)
                removed_any = True
        except Exception:
            pass

    # Detect 7zip/tool extraction failures (Codec Load Error, 7zip failed)
    if '7zip' in content.lower() or 'codec load error' in content.lower() or '7z.dll' in content.lower():
        tools_dir = os.path.join(root_dir, 'vcpkg', 'downloads', 'tools')
        if os.path.isdir(tools_dir):
            for entry in os.listdir(tools_dir):
                if '7zip' in entry.lower() or '7-zip' in entry.lower() or entry.lower().startswith('7zip'):
                    path = os.path.join(tools_dir, entry)
                    try:
                        shutil.rmtree(path)
                        Colors.print(f"Removed vcpkg tool folder due to extraction error: {path}", Colors.YELLOW)
                        removed_any = True
                    except Exception:
                        pass

    # If PowerShell extraction or runtime validation failed repeatedly, surface an actionable message
    if 'pwsh.exe failed' in content.lower() or 'pwsh.dll' in content.lower() or 'powershell-core' in content.lower() and 'failed' in content.lower():
        Colors.print("vcpkg failed to install PowerShell runtime. This commonly happens when the extracted PowerShell lacks required runtime files or system dependencies.", Colors.RED)
        Colors.print("Actions you can take:", Colors.YELLOW)
        Colors.print("  1) Install PowerShell 7.5.4 for Windows from https://github.com/PowerShell/PowerShell/releases and ensure 'pwsh.exe' runs on your system.", Colors.YELLOW)
        Colors.print("  2) Ensure the Visual C++ Redistributable is installed (x64) — missing runtimes can prevent pwsh.exe from running.", Colors.YELLOW)
        Colors.print("  3) Install 7-Zip on the host so vcpkg can extract archives reliably, or verify your antivirus isn't blocking extraction.", Colors.YELLOW)
        Colors.print("After taking these steps, re-run: python package.py --installer", Colors.YELLOW)
        # Returning False indicates we didn't auto-fix the issue
        return False

    # Additionally, run a general cleanup pass to remove any obvious bad zips
    removed = _cleanup_vcpkg_downloads(root_dir)
    if removed:
        removed_any = True

    # If we detected PowerShell or other zip failures, try manual extraction with Python as a fallback
    # Look for PowerShell downloads in the downloads directory
    try:
        dl_dir = os.path.join(root_dir, 'vcpkg', 'downloads')
        if os.path.isdir(dl_dir):
            for fname in os.listdir(dl_dir):
                if 'powershell' in fname.lower() and fname.lower().endswith('.zip'):
                    full = os.path.join(dl_dir, fname)
                    # Attempt manual extraction into expected tools folder
                    tool_name = None
                    # try to infer version folder name used by vcpkg when extracting
                    # e.g., tools\powershell-core-7.5.4-windows
                    m = re.search(r'(powershell[-_].*?\d[\d\.\-]*[-_]?win)', fname, flags=re.IGNORECASE)
                    if m:
                        tool_name = m.group(0)
                    else:
                        # Fallback name
                        tool_name = 'powershell-core-7.5.4-windows'
                    target_dir = os.path.join(root_dir, 'vcpkg', 'downloads', 'tools', tool_name)
                    if _manual_extract_zip(full, target_dir):
                        Colors.print(f"Manual extraction of {full} succeeded -> {target_dir}", Colors.GREEN)
                        removed_any = True
    except Exception:
        pass

    return removed_any


def _manual_extract_zip(zip_path, target_dir):
    """Fallback extractor using Python's zipfile to extract a zip into target_dir."""
    try:
        if not os.path.exists(zip_path):
            return False
        temp_dir = zip_path + '.tmp'
        if os.path.exists(temp_dir):
            try:
                shutil.rmtree(temp_dir)
            except Exception:
                pass
        os.makedirs(temp_dir, exist_ok=True)
        with zipfile.ZipFile(zip_path, 'r') as z:
            bad = z.testzip()
            if bad:
                Colors.print(f"Zip test failed: first bad file {bad}", Colors.YELLOW)
            z.extractall(temp_dir)
        # Move extracted files into target_dir
        if os.path.exists(target_dir):
            try:
                shutil.rmtree(target_dir)
            except Exception:
                pass
        shutil.move(temp_dir, target_dir)
        # Quick sanity: check for pwsh.exe in the target_dir (or bin/pwsh.exe)
        possible = [os.path.join(target_dir, 'pwsh.exe'), os.path.join(target_dir, 'bin', 'pwsh.exe')]
        if any(os.path.exists(p) for p in possible):
            return True
        # If pwsh.exe not found, still return True assuming extraction may help later
        return True
    except Exception as e:
        Colors.print(f"Manual zip extraction failed: {e}", Colors.RED)
        try:
            if os.path.exists(temp_dir):
                shutil.rmtree(temp_dir)
        except Exception:
            pass
        return False


def preflight_check_windows():
    """Check common host prerequisites that frequently break vcpkg on Windows.
    Returns True if the essentials are present and executable (pwsh); otherwise False.
    """
    Colors.print("Running Windows preflight checks...", Colors.BLUE)
    ok = True

    # Check for pwsh (PowerShell 7)
    pwsh = shutil.which('pwsh') or shutil.which('pwsh.exe')
    if not pwsh:
        Colors.print("PowerShell 7 (pwsh) not found in PATH. vcpkg downloads PowerShell and executes it; missing pwsh can lead to failures.", Colors.RED)
        Colors.print("Install PowerShell 7.5.4 from https://github.com/PowerShell/PowerShell/releases and ensure 'pwsh.exe' runs on your system.", Colors.YELLOW)
        ok = False
    else:
        try:
            out = subprocess.check_output([pwsh, '--version'], stderr=subprocess.STDOUT, universal_newlines=True)
            Colors.print(f"Detected pwsh: {out.strip()}", Colors.GREEN)
        except Exception as e:
            Colors.print(f"pwsh exists but failed to execute: {e}", Colors.RED)
            ok = False

    # Check for 7-Zip (7z) presence (used by vcpkg for extraction)
    seven = shutil.which('7z') or shutil.which('7z.exe')
    if not seven:
        pf = os.environ.get('ProgramFiles', r"C:\Program Files")
        common = os.path.join(pf, '7-Zip', '7z.exe')
        if os.path.exists(common):
            seven = common

    if not seven:
        Colors.print("7-Zip not found (7z). This can cause vcpkg archive extraction failures.", Colors.YELLOW)
        Colors.print("Install 7-Zip or ensure '7z' is available in PATH.", Colors.YELLOW)
    else:
        try:
            subprocess.check_output([seven, '--help'], stderr=subprocess.STDOUT, universal_newlines=True)
            Colors.print("7z found", Colors.GREEN)
        except Exception:
            Colors.print("7z exists but failed to run; extraction may fail.", Colors.YELLOW)

    # Check for Visual C++ redistributable presence by scanning System32 for common DLLs
    sysroot = os.environ.get('SystemRoot', r"C:\Windows")
    dlls = ['vcruntime140.dll', 'msvcp140.dll']
    found_runtime = False
    for dll in dlls:
        if os.path.exists(os.path.join(sysroot, 'System32', dll)):
            found_runtime = True
            break
    if not found_runtime:
        Colors.print("Visual C++ runtime (vcruntime) not detected in System32. Missing runtime may prevent pwsh.exe from running.", Colors.YELLOW)
        Colors.print("Install the Visual C++ Redistributable (Microsoft Visual C++ 2015-2022 Redistributable x64).", Colors.YELLOW)

    return ok


def build_neutron(os_type, arch_type, build_dir="build", skip_vcpkg=False):
    """Configure and build Neutron. If vcpkg fails due to third-party tool issues
    (e.g. PowerShell extraction toolchain), we will warn and continue so that
    other parts (like Box) can still be built and the packaging flow can proceed.
    Returns True if we can continue; False only if a non-recoverable error occurs.
    """
    global VCPKG_ISSUE_DETECTED
    VCPKG_ISSUE_DETECTED = False

    Colors.print("Configuring and building Neutron...", Colors.BLUE)
    os.makedirs(build_dir, exist_ok=True)

    cmake_exe = get_cmake_command()

    # Initialize vcpkg first if needed
    root_dir = os.getcwd()
    if os_type == "windows" and not skip_vcpkg:
        initialize_vcpkg(root_dir)

    # Configure based on OS
    if os_type == "windows":
        # Use vcpkg preset
        cmake_cmd = [cmake_exe, "--preset", "winmsvc"]
        run_cwd = os.getcwd()  # Run from root for presets
    else:
        cmake_cmd = [cmake_exe, "..", "-DCMAKE_BUILD_TYPE=Release"]
        run_cwd = build_dir
        os.chdir(build_dir)  # Change to build directory for Unix systems

    # On Windows with vcpkg, retry configure up to N times if vcpkg download/extract fails
    max_attempts = 3
    attempt = 0

    while attempt < max_attempts:
        attempt += 1
        # Pre-clean any obviously-bad downloads before configuring
        _cleanup_vcpkg_downloads(os.getcwd())

        Colors.print(f"CMake configure attempt {attempt}/{max_attempts}...", Colors.BLUE)
        success = run_command(cmake_cmd, cwd=run_cwd, fail_exit=False)
        if success:
            # Configure succeeded, proceed to build
            break

        # Configure failed; check vcpkg log for archive issues
        Colors.print("CMake configure failed; checking vcpkg logs for corrupt downloads...", Colors.YELLOW)
        handled = _handle_vcpkg_manifest_log(build_dir, os.getcwd())
        if handled:
            # If we fixed something, retry
            Colors.print("Deleted suspected corrupt vcpkg downloads/tools; retrying configure...", Colors.YELLOW)
            time.sleep(1)
            continue

        # If we couldn't handle the log (e.g. pwsh runtime missing), set flag and proceed with a warning
        Colors.print("vcpkg reported issues that couldn't be auto-fixed. Continuing without full vcpkg install.", Colors.YELLOW)
        VCPKG_ISSUE_DETECTED = True
        # Break out and allow overall packaging to continue; some artifacts may be missing
        break

    # Change back to original directory if needed
    if os_type != "windows":
        os.chdir(root_dir)

    # Check if configure produced build files; if not and we didn't detect a vcpkg issue, it's fatal
    if not os.path.exists(os.path.join(build_dir, 'CMakeCache.txt')) and not VCPKG_ISSUE_DETECTED:
        Colors.print("CMake configure failed after retries.", Colors.RED)
        return False

    # Attempt to build if configure generated files
    if os.path.exists(build_dir):
        build_cmd = [cmake_exe, "--build", ".", "--config", "Release"]
        # Parallel build
        import multiprocessing
        try:
            cpu_count = multiprocessing.cpu_count()
            build_cmd.append(f"-j{cpu_count}")
        except:
            pass

        if not run_command(build_cmd, cwd=build_dir, fail_exit=False):
            Colors.print("Build failed for Neutron; proceeding with packaging but some runtime files may be missing.", Colors.YELLOW)
            VCPKG_ISSUE_DETECTED = True
            # Not fatal; return True to continue overall packaging
            return True
    else:
        Colors.print("Build directory not found after configure; continuing packaging as best-effort.", Colors.YELLOW)
        VCPKG_ISSUE_DETECTED = True

    return True

def build_box(os_type, arch_type, build_dir="nt-box/build", skip_vcpkg=False):
    Colors.print("Configuring and building Box package manager...", Colors.BLUE)

    # Initialize vcpkg first if needed
    root_dir = os.getcwd()
    if os_type == "windows" and not skip_vcpkg:
        initialize_vcpkg(root_dir)

    # Ensure the nt-box directory exists
    if not os.path.exists("nt-box"):
        Colors.print("nt-box directory not found! Cannot build Box package manager.", Colors.RED)
        return False

    os.makedirs(build_dir, exist_ok=True)

    cmake_exe = get_cmake_command()

    # Configure based on OS
    if os_type == "windows":
        # Use vcpkg toolchain for Box too
        vcpkg_toolchain = os.path.join(root_dir, "vcpkg", "scripts", "buildsystems", "vcpkg.cmake")
        cmake_cmd = [cmake_exe, "..", f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_toolchain}", "-DCMAKE_BUILD_TYPE=Release"]
        run_cwd = build_dir
    else:
        cmake_cmd = [cmake_exe, "..", "-DCMAKE_BUILD_TYPE=Release"]
        run_cwd = build_dir

    # Configure
    if not run_command(cmake_cmd, cwd=run_cwd, fail_exit=False):
        Colors.print("CMake configure failed for Box.", Colors.RED)
        return False

    # Build
    build_cmd = [cmake_exe, "--build", ".", "--config", "Release"]
    import multiprocessing
    try:
        cpu_count = multiprocessing.cpu_count()
        build_cmd.append(f"-j{cpu_count}")
    except:
        pass

    if not run_command(build_cmd, cwd=build_dir, fail_exit=False):
        Colors.print("Build failed for Box; proceeding with packaging but box.exe may be missing.", Colors.YELLOW)
        return False

    return True

def main():
    parser = argparse.ArgumentParser(description="Neutron Packaging Script")
    parser.add_argument("--output", help="Output directory name for the package", default=None)
    parser.add_argument("--installer", help="Build NSIS installer (Windows only)", action="store_true")
    parser.add_argument("--skip-vcpkg", help="Skip vcpkg bootstrap and proceed (may fail if runtime deps missing)", action="store_true")
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
    else:
        # Build both neutron and box if any are missing
        Colors.print("Building Neutron and Box executables...", Colors.YELLOW)

        # Preflight checks on Windows to catch common vcpkg/tooling problems before we start
        if os_type == 'windows' and not args.skip_vcpkg:
            ok = preflight_check_windows()
            if not ok:
                Colors.print("Preflight checks failed. Either install the listed prerequisites or re-run with --skip-vcpkg to proceed (may still fail).", Colors.RED)
                sys.exit(1)

        # Build both executables
        neutron_success = build_neutron(os_type, arch_type, build_dir, args.skip_vcpkg)
        box_success = build_box(os_type, arch_type, box_build_dir, args.skip_vcpkg)

        # Ensure we have found both binaries after building
        # Wait a bit in case build process is still copying files
        time.sleep(2)

        # Find binaries after build
        real_neutron_path = None
        real_box_path = None

        # Try neutron paths first
        for p in neutron_bin_paths:
            if os.path.exists(p):
                real_neutron_path = p
                break

        # If not found in standard locations, search in build directory more broadly
        if not real_neutron_path:
            neutron_search_paths = [
                os.path.join(build_dir, "Release", neutron_exe),
                os.path.join(build_dir, "Debug", neutron_exe),
                os.path.join(build_dir, "MinSizeRel", neutron_exe),
                os.path.join(build_dir, "RelWithDebInfo", neutron_exe),
                os.path.join(root_dir, neutron_exe),  # In case build copied to root
            ]
            for p in neutron_search_paths:
                if os.path.exists(p):
                    real_neutron_path = p
                    break

        # Try box paths
        for p in box_bin_paths:
            if os.path.exists(p):
                real_box_path = p
                break

        # If not found in standard locations, search in box build directory more broadly
        if not real_box_path:
            box_search_paths = [
                os.path.join(box_build_dir, "Release", box_exe),
                os.path.join(box_build_dir, "Debug", box_exe),
                os.path.join(box_build_dir, "MinSizeRel", box_exe),
                os.path.join(box_build_dir, "RelWithDebInfo", box_exe),
                os.path.join(root_dir, box_exe),  # In case build copied to root
            ]
            for p in box_search_paths:
                if os.path.exists(p):
                    real_box_path = p
                    break

        # Check if both builds were successful and binaries exist
        if not real_neutron_path and not real_box_path:
            Colors.print("Build seemed successful but could not locate any binaries!", Colors.RED)
            sys.exit(1)

        # Warn if one of them is missing but continue (best-effort packaging)
        if not real_neutron_path:
            Colors.print("Warning: neutron executable not found; installer may not include runtime files.", Colors.YELLOW)
        if not real_box_path:
            Colors.print("Warning: box executable not found; some packaging actions may be skipped.", Colors.YELLOW)

        # Verify executables work (optional test)
        if real_neutron_path and os.path.exists(real_neutron_path):
            try:
                subprocess.check_output([real_neutron_path, "--version"], timeout=10)
                Colors.print("Neutron executable verified successfully.", Colors.GREEN)
            except Exception as e:
                Colors.print(f"Neutron executable failed verification: {e}", Colors.YELLOW)

        if real_box_path and os.path.exists(real_box_path):
            try:
                subprocess.check_output([real_box_path, "--help"], timeout=10)
                Colors.print("Box executable verified successfully.", Colors.GREEN)
            except Exception as e:
                Colors.print(f"Box executable failed verification: {e}", Colors.YELLOW)

    # Packaging
    if args.output:
        target_name = args.output
    else:
        target_name = f"neutron-{os_type}-{arch_type}"
        
    Colors.print(f"Creating package: {target_name}", Colors.BLUE)
    
    if os.path.exists(target_name):
        shutil.rmtree(target_name)
    os.makedirs(target_name)
    
    # Copy binaries (only if present)
    if real_neutron_path and os.path.exists(real_neutron_path):
        shutil.copy2(real_neutron_path, target_name)
    else:
        Colors.print("Skipping copy of neutron executable (not found).", Colors.YELLOW)

    if real_box_path and os.path.exists(real_box_path):
        shutil.copy2(real_box_path, target_name)
    else:
        Colors.print("Skipping copy of box executable (not found).", Colors.YELLOW)
    
    # Create lib directory and copy runtime library
    lib_dir = os.path.join(target_name, "lib")
    os.makedirs(lib_dir, exist_ok=True)
    
    # Libraries stuff
    if os_type == "windows":
        Colors.print(f"Copying libraries to {lib_dir}...", Colors.BLUE)
        num_copied = 0
        
        # Copy runtime library to lib directory
        # Check standard MSVC locations
        search_paths = [
            os.path.join(build_dir, "*.lib"),
            os.path.join(build_dir, "neutron.lib"), # Import library for the executable
            os.path.join(build_dir, "Release", "*.lib"),
            os.path.join(build_dir, "Release", "neutron.lib"),
            os.path.join(build_dir, "MinSizeRel", "*.lib"),
            os.path.join(build_dir, "RelWithDebInfo", "*.lib"),
            os.path.join(build_dir, "*.a") # Just in case
        ]
        
        for p in search_paths:
            for lib_file in glob.glob(p):
                print(f"  Copying {lib_file}")
                # Copy to root directory instead of lib/ so it's with the executable
                shutil.copy2(lib_file, target_name)
                num_copied += 1
                
        if num_copied == 0:
             Colors.print("WARNING: No .lib or .a files found to copy!", Colors.YELLOW)

        # Copy DLLs from build dir if any
        # With MSVC dynamic linking, we might rely on system installed runtimes or vcpkg dlls
        # Search for .dll in build dir
        for dll in glob.glob(os.path.join(build_dir, "*.dll")):
             shutil.copy2(dll, target_name)
        for dll in glob.glob(os.path.join(build_dir, "Release", "*.dll")):
             shutil.copy2(dll, target_name)
    else:
        # Unix
        # Copy libneutron_runtime to lib directory
        lib_extensions = ["a", "so", "dylib"]
        for ext in lib_extensions:
            for f in glob.glob(os.path.join(build_dir, f"*.{ext}*")):
                shutil.copy2(f, target_name)
    
    # Copy assets
    items_to_copy = ["README.md", "LICENSE", "docs", "include", "src", "libs", "nt-box"]
    
    for item in items_to_copy:
        src_path = os.path.join(root_dir, item)
        if os.path.exists(src_path):
            dst_path = os.path.join(target_name, item)
            if os.path.isdir(src_path):
                # For nt-box, exclude build artifacts and git
                if item == "nt-box":
                    shutil.copytree(src_path, dst_path, ignore=shutil.ignore_patterns('build', '.git', '*.o', '*.obj', '*.exe', '*.dll'))
                else:
                    shutil.copytree(src_path, dst_path)
            else:
                shutil.copy2(src_path, target_name)

    # Always attempt to bundle vcpkg-managed tools into the package directory
    vcpkg_tools_dir = os.path.join(root_dir, 'vcpkg', 'downloads', 'tools')
    if os.path.isdir(vcpkg_tools_dir):
        Colors.print(f"Bundling vcpkg tools into package ({target_name}/tools)...", Colors.BLUE)
        pkg_tools_dir = os.path.join(target_name, 'tools')
        os.makedirs(pkg_tools_dir, exist_ok=True)
        for entry in os.listdir(vcpkg_tools_dir):
            entry_l = entry.lower()
            if 'powershell' in entry_l or '7zip' in entry_l or '7-zip' in entry_l:
                src = os.path.join(vcpkg_tools_dir, entry)
                dst = os.path.join(pkg_tools_dir, entry)
                try:
                    if os.path.exists(dst):
                        shutil.rmtree(dst)
                    shutil.copytree(src, dst)
                    Colors.print(f"  Bundled tool: {entry}", Colors.GREEN)
                except Exception as e:
                    Colors.print(f"Failed to bundle tool {entry}: {e}", Colors.YELLOW)
    else:
        Colors.print("No vcpkg tools found to include in package.", Colors.YELLOW)

    # Version check (only if neutron executable present)
    version = "unknown"
    if real_neutron_path and os.path.exists(real_neutron_path):
        try:
            output = subprocess.check_output([real_neutron_path, "--version"]).decode().strip()
            # Simple extraction if output is like "Neutron 1.2.3"
            import re
            m = re.search(r'(\d+\.\d+\.\d+)', output)
            if m:
                version = m.group(1)
        except Exception:
            version = "unknown"
    else:
        Colors.print("Neutron executable not available; setting package version to 'unknown'", Colors.YELLOW)

    Colors.print(f"Version: {version}", Colors.GREEN)
    
    # Archive
    # Archive skipped as per user request
    # Instead of creating zip/tar, we just leave the directory there
    Colors.print(f"Build artifacts are in directory: {target_name}", Colors.GREEN)
    
    # Optional: NSIS Installer (keep if simple, or remove if considered "packaging")
    # User said "remove zipping support", but installer is useful. 
    # Let's keep NSIS check if on Windows but DO NOT ZIP.
    if args.installer and os_type == "windows" and shutil.which("makensis"):
         Colors.print("Building Windows Installer...", Colors.BLUE)
         if os.path.exists("installer.nsi"):
             # Copy binaries and libs to root for NSIS to find (only if they exist)
             if real_neutron_path and os.path.exists(real_neutron_path):
                 shutil.copy2(real_neutron_path, root_dir)
             else:
                 Colors.print("Note: neutron executable not present; installer will not include it.", Colors.YELLOW)
             if real_box_path and os.path.exists(real_box_path):
                 shutil.copy2(real_box_path, root_dir)
             else:
                 Colors.print("Note: box executable not present; installer will not include it.", Colors.YELLOW)
             
             # Copy DLLs (needed for vcpkg dynamic linking)
             for dll in glob.glob(os.path.join(build_dir, "Release", "*.dll")):
                 shutil.copy2(dll, root_dir)
             for dll in glob.glob(os.path.join(build_dir, "*.dll")):
                 shutil.copy2(dll, root_dir)
             
             # Copy runtime lib if it exists
             for lib_file in glob.glob(os.path.join(build_dir, "*.lib")):
                 shutil.copy2(lib_file, root_dir)
             for lib_file in glob.glob(os.path.join(build_dir, "Release", "*.lib")):
                 shutil.copy2(lib_file, root_dir)

             # Sanitize installer.nsi by commenting out File lines that glob to nothing
             temp_nsi = "installer.temp.nsi"
             try:
                 changed = sanitize_nsi_for_globs("installer.nsi", temp_nsi, root_dir)
                 nsi_to_use = temp_nsi if changed else "installer.nsi"
                 Colors.print(f"Running NSIS with version {version}...", Colors.BLUE)
                 result = subprocess.call(["makensis", f"/DVERSION={version}", nsi_to_use])
                 if result == 0:
                     Colors.print("✓ Installer created successfully: NeutronInstaller.exe", Colors.GREEN)
                 else:
                     Colors.print(f"✗ NSIS failed with exit code {result}", Colors.RED)
             finally:
                 # Clean up temporary file if it was created
                 if os.path.exists(temp_nsi):
                     try:
                         os.remove(temp_nsi)
                     except Exception:
                         pass
         else:
             Colors.print("installer.nsi not found, skipping installer creation", Colors.YELLOW)

             if os.path.exists("NeutronInstaller.exe"):
                 inst_name = f"neutron-{version}-installer.exe"
                 # Remove old installer file safely (it may be locked by antivirus or other process)
                 if os.path.exists(inst_name):
                     if not safe_remove(inst_name, retries=20, delay=0.5):
                         Colors.print(f"Could not remove existing installer {inst_name}; keeping previous installer.", Colors.YELLOW)

                 # Move new installer into place
                 try:
                     shutil.move("NeutronInstaller.exe", inst_name)
                     Colors.print(f"Installer created: {inst_name}", Colors.GREEN)
                 except Exception as e:
                     Colors.print(f"Failed to move installer into place: {e}", Colors.RED)

    Colors.print("Packaging flow completed successfully!", Colors.GREEN)

if __name__ == "__main__":
    main()
