
import shutil
import os
import time
import stat

def on_rm_error(func, path, exc_info):
    # chmod to write and try again
    os.chmod(path, stat.S_IWRITE)
    func(path)

def force_remove(path):
    if os.path.exists(path):
        print(f"Removing {path}...")
        try:
            shutil.rmtree(path, onerror=on_rm_error)
        except Exception as e:
            print(f"Python remove failed: {e}")
            # Try shell
            os.system(f"rmdir /s /q {path}")

force_remove("build")
force_remove("nt-box/build")

import glob
import time
import fnmatch

# Try to remove files robustly (handles locked files on Windows)
def remove_with_retries(path, retries=10, delay=0.5):
    for i in range(retries):
        try:
            if os.path.exists(path):
                print(f"Removing file: {path}")
                os.chmod(path, stat.S_IWRITE)
                os.remove(path)
            return True
        except PermissionError:
            print(f"PermissionError removing {path}; retrying ({i+1}/{retries})...")
            time.sleep(delay)
        except Exception as e:
            print(f"Error removing {path}: {e}")
            return False
    print(f"Failed to remove {path} after {retries} attempts.")
    return False

# Remove top-level binaries and libs
patterns = []
if os.name == 'nt':
    patterns = ["neutron.exe", "box.exe", "*.lib", "*.dll", "neutron-*-installer.exe"]
else:
    patterns = ["neutron", "box", "libneutron_runtime.*", "*.a", "*.so", "*.dylib"]

for pat in patterns:
    for f in glob.glob(pat):
        remove_with_retries(f)

# Remove any copied lib/dll in project root (leftovers from packaging)
for f in glob.glob("*.lib") + glob.glob("*.dll") + glob.glob("*.a") + glob.glob("*.so") + glob.glob("*.dylib") + glob.glob("*.exe"):
    remove_with_retries(f)

# Remove build artifacts recursively but avoid touching third-party folders
exclude_dirs = {'.git', 'vcpkg', 'neutron-linux-x64', 'neutron-windows-x64', 'build_test', 'node_modules', '.box'}
# Patterns to remove (add .obj and .exe as requested)
patterns = ['*.dll', '*.lib', '*.a', '*.so', '*.dylib', '*.obj', '*.exe']

print("Scanning project tree for build artifacts (excluding common third-party dirs)...")
for root, dirs, files in os.walk('.'):
    # Normalize and skip excluded folders
    # Modify dirs in-place to prevent os.walk from recursing into them
    dirs[:] = [d for d in dirs if d not in exclude_dirs]

    for pat in patterns:
        for filename in fnmatch.filter(files, pat):
            path = os.path.join(root, filename)
            remove_with_retries(path)

print("Done cleaning.")
