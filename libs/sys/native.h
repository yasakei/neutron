/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 *
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * For full license text, see LICENSE file in the root directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Code Documentation: Sys Module (libs/sys/native.h)
 * ==================================================
 * 
 * This header defines the Sys module - Neutron's system operations interface.
 * It provides access to file I/O, environment variables, process control,
 * and other system-level functionality.
 * 
 * What This File Includes:
 * ------------------------
 * - register_sys_functions(): Register all sys module functions
 * - neutron_init_sys_module(): C API for module initialization
 * 
 * Available Functions (implemented in native.cpp):
 * -----------------------------------------------
 * - sys.readFile(path) → string: Read entire file contents
 * - sys.writeFile(path, content) → void: Write string to file
 * - sys.appendFile(path, content) → void: Append to file
 * - sys.deleteFile(path) → void: Delete a file
 * - sys.fileExists(path) → bool: Check if file exists
 * - sys.createDir(path) → void: Create directory
 * - sys.deleteDir(path) → void: Delete directory
 * - sys.listDir(path) → array: List directory contents
 * - sys.getenv(name) → string?: Get environment variable
 * - sys.setenv(name, value) → void: Set environment variable
 * - sys.exec(command) → string: Execute shell command
 * - sys.exit(code) → noreturn: Terminate process
 * - sys.sleep(ms) → void: Sleep for milliseconds
 * - sys.time() → number: Get Unix timestamp
 * - sys.cwd() → string: Get current working directory
 * - sys.chdir(path) → void: Change directory
 * 
 * Adding Features:
 * ----------------
 * - New functions: Implement in native.cpp, register in register_sys_functions()
 * - Platform support: Use platform.h for cross-platform compatibility
 * - Security: Consider sandbox implications for dangerous operations
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT expose raw file descriptors (use high-level APIs)
 * - Do NOT allow arbitrary command execution without sanitization
 * - Do NOT bypass file permission checks
 * - Do NOT leak system resources (close handles, free memory)
 * 
 * Security Notes:
 * ---------------
 * - sys.exec() should be restricted in safe mode (.ntsc files)
 * - File operations respect OS permissions
 * - Environment variable access may be sandboxed
 */

#pragma once

#include "vm.h"
#include "runtime/environment.h"

namespace neutron {

/**
 * @brief Register all sys module functions with the VM.
 * @param vm The VM instance.
 * @param env The environment to register functions in.
 * 
 * Creates a "sys" module and registers all system functions.
 * Called automatically when "use sys" is encountered.
 * 
 * Example Neutron code:
 * @code
 * use sys
 * 
 * var content = sys.readFile("data.txt")
 * sys.writeFile("output.txt", content)
 * sys.print("Current dir: " + sys.cwd())
 * @endcode
 */
void register_sys_functions(VM& vm, std::shared_ptr<Environment> env);

} // namespace neutron

/**
 * @brief C API entry point for sys module initialization.
 * @param vm The VM instance (as opaque pointer).
 * 
 * Used for dynamic module loading via dlopen/LoadLibrary.
 * The function name follows the neutron_init_<module>_module convention.
 */
extern "C" void neutron_init_sys_module(neutron::VM* vm);

