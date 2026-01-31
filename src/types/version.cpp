/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "types/version.h"
#include <sstream>
#include <ctime>

namespace neutron {

std::string Version::getVersion() {
    std::ostringstream oss;
    oss << MAJOR << "." << MINOR << "." << PATCH << "-" << STAGE;
    return oss.str();
}

std::string Version::getFullVersion() {
    std::ostringstream oss;
    oss << "Neutron " << getVersion() << " (" << getPlatform() << ")";
    return oss.str();
}

std::string Version::getMajorMinorPatch() {
    std::ostringstream oss;
    oss << MAJOR << "." << MINOR << "." << PATCH;
    return oss.str();
}

std::string Version::getBuildDate() {
    return __DATE__ " " __TIME__;
}

std::string Version::getPlatform() {
#if defined(_WIN32)
    #if defined(__MINGW64__)
        return "Windows MINGW64";
    #elif defined(__MINGW32__)
        return "Windows MINGW32";
    #else
        return "Windows MSVC";
    #endif
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

} // namespace neutron
