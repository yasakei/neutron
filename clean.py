
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
print("Done cleaning.")
