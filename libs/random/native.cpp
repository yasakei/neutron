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

#include "native.h"
#include "vm.h"
#include "types/array.h"
#include "types/obj_string.h"
#include "types/native_fn.h"
#include "modules/module.h"
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

// Cross-platform includes for secure seeding
#ifdef _WIN32
    #include <windows.h>
    #include <wincrypt.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

using namespace neutron;

// Global random number generator
static std::mt19937 rng;
static bool rng_seeded = false;

// Cross-platform secure seed generation
uint32_t get_secure_seed() {
    uint32_t seed = 0;
    
#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptGenRandom(hCryptProv, sizeof(seed), reinterpret_cast<BYTE*>(&seed))) {
            CryptReleaseContext(hCryptProv, 0);
            return seed;
        }
        CryptReleaseContext(hCryptProv, 0);
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd != -1) {
        if (read(fd, &seed, sizeof(seed)) == sizeof(seed)) {
            close(fd);
            return seed;
        }
        close(fd);
    }
#endif
    
    // Fallback to time-based seed
    auto now = std::chrono::high_resolution_clock::now();
    return static_cast<uint32_t>(now.time_since_epoch().count());
}

// Initialize RNG if not already seeded
void ensure_rng_seeded() {
    if (!rng_seeded) {
        rng.seed(get_secure_seed());
        rng_seeded = true;
    }
}

// Random module functions

// random() - Returns random float between 0.0 and 1.0
Value random_random(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for random.random().");
    }
    
    ensure_rng_seeded();
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double result = dist(rng);
    
    return Value(result);
}

// uniform(a, b) - Returns random float between a and b
Value random_uniform(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for random.uniform(a, b).");
    }
    
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Both arguments for random.uniform() must be numbers.");
    }
    
    double a = arguments[0].as.number;
    double b = arguments[1].as.number;
    
    if (a > b) {
        std::swap(a, b);
    }
    
    ensure_rng_seeded();
    std::uniform_real_distribution<double> dist(a, b);
    double result = dist(rng);
    
    return Value(result);
}

// randint(a, b) - Returns random integer between a and b (inclusive)
Value random_randint(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for random.randint(a, b).");
    }
    
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Both arguments for random.randint() must be numbers.");
    }
    
    int a = static_cast<int>(arguments[0].as.number);
    int b = static_cast<int>(arguments[1].as.number);
    
    if (a > b) {
        std::swap(a, b);
    }
    
    ensure_rng_seeded();
    std::uniform_int_distribution<int> dist(a, b);
    int result = dist(rng);
    
    return Value(static_cast<double>(result));
}

// choice(array) - Returns random element from array
Value random_choice(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for random.choice().");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Argument for random.choice() must be an array.");
    }
    
    Array* arr = arguments[0].as.array;
    if (arr->elements.empty()) {
        throw std::runtime_error("Cannot choose from empty array.");
    }
    
    ensure_rng_seeded();
    std::uniform_int_distribution<size_t> dist(0, arr->elements.size() - 1);
    size_t index = dist(rng);
    
    return arr->elements[index];
}

// shuffle(array) - Shuffles array in-place
Value random_shuffle(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for random.shuffle().");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Argument for random.shuffle() must be an array.");
    }
    
    Array* arr = arguments[0].as.array;
    if (arr->elements.size() <= 1) {
        return Value(); // nil - nothing to shuffle
    }
    
    ensure_rng_seeded();
    std::shuffle(arr->elements.begin(), arr->elements.end(), rng);
    
    return Value(); // nil
}

// seed(value) - Sets random seed
Value random_seed(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() > 1) {
        throw std::runtime_error("Expected 0-1 arguments for random.seed().");
    }
    
    uint32_t seed_value;
    
    if (arguments.size() == 0) {
        seed_value = get_secure_seed();
    } else {
        if (arguments[0].type != ValueType::NUMBER) {
            throw std::runtime_error("Argument for random.seed() must be a number.");
        }
        seed_value = static_cast<uint32_t>(arguments[0].as.number);
    }
    
    rng.seed(seed_value);
    rng_seeded = true;
    
    return Value(); // nil
}

// gauss(mu, sigma) - Returns random number from Gaussian distribution
Value random_gauss(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for random.gauss(mu, sigma).");
    }
    
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Both arguments for random.gauss() must be numbers.");
    }
    
    double mu = arguments[0].as.number;
    double sigma = arguments[1].as.number;
    
    if (sigma <= 0) {
        throw std::runtime_error("Sigma must be positive for random.gauss().");
    }
    
    ensure_rng_seeded();
    std::normal_distribution<double> dist(mu, sigma);
    double result = dist(rng);
    
    return Value(result);
}

// expovariate(lambd) - Returns random number from exponential distribution
Value random_expovariate(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for random.expovariate(lambd).");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for random.expovariate() must be a number.");
    }
    
    double lambd = arguments[0].as.number;
    
    if (lambd <= 0) {
        throw std::runtime_error("Lambda must be positive for random.expovariate().");
    }
    
    ensure_rng_seeded();
    std::exponential_distribution<double> dist(lambd);
    double result = dist(rng);
    
    return Value(result);
}

// triangular(low, high, mode) - Returns random number from triangular distribution
Value random_triangular(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 3) {
        throw std::runtime_error("Expected 3 arguments for random.triangular(low, high, mode).");
    }
    
    if (arguments[0].type != ValueType::NUMBER || 
        arguments[1].type != ValueType::NUMBER || 
        arguments[2].type != ValueType::NUMBER) {
        throw std::runtime_error("All arguments for random.triangular() must be numbers.");
    }
    
    double low = arguments[0].as.number;
    double high = arguments[1].as.number;
    double mode = arguments[2].as.number;
    
    if (low >= high) {
        throw std::runtime_error("Low must be less than high for random.triangular().");
    }
    
    if (mode < low || mode > high) {
        throw std::runtime_error("Mode must be between low and high for random.triangular().");
    }
    
    ensure_rng_seeded();
    
    // Implement triangular distribution using inverse transform sampling
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    double u = uniform(rng);
    
    double fc = (mode - low) / (high - low);
    
    double result;
    if (u < fc) {
        result = low + std::sqrt(u * (high - low) * (mode - low));
    } else {
        result = high - std::sqrt((1 - u) * (high - low) * (high - mode));
    }
    
    return Value(result);
}

// sample(array, k) - Returns k random elements without replacement
Value random_sample(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for random.sample(array, k).");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument for random.sample() must be an array.");
    }
    
    if (arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Second argument for random.sample() must be a number.");
    }
    
    Array* arr = arguments[0].as.array;
    int k = static_cast<int>(arguments[1].as.number);
    
    if (k < 0) {
        throw std::runtime_error("Sample size must be non-negative.");
    }
    
    if (k > static_cast<int>(arr->elements.size())) {
        throw std::runtime_error("Sample size cannot be larger than array size.");
    }
    
    ensure_rng_seeded();
    
    // Create indices and shuffle them
    std::vector<size_t> indices;
    for (size_t i = 0; i < arr->elements.size(); i++) {
        indices.push_back(i);
    }
    
    std::shuffle(indices.begin(), indices.end(), rng);
    
    // Create result array with first k elements
    Array* result = vm.allocate<Array>();
    for (int i = 0; i < k; i++) {
        result->elements.push_back(arr->elements[indices[i]]);
    }
    
    return Value(result);
}

// getrandbits(k) - Returns random integer with k random bits
Value random_getrandbits(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for random.getrandbits(k).");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for random.getrandbits() must be a number.");
    }
    
    int k = static_cast<int>(arguments[0].as.number);
    
    if (k <= 0 || k > 32) {
        throw std::runtime_error("Number of bits must be between 1 and 32.");
    }
    
    ensure_rng_seeded();
    
    // Generate random bits
    uint32_t mask = (1ULL << k) - 1;
    uint32_t result = rng() & mask;
    
    return Value(static_cast<double>(result));
}

namespace neutron {
    void register_random_functions(VM& vm, std::shared_ptr<Environment> env) {
        // Core random functions
        env->define("random", Value(vm.allocate<NativeFn>(random_random, 0, true)));
        env->define("uniform", Value(vm.allocate<NativeFn>(random_uniform, 2, true)));
        env->define("randint", Value(vm.allocate<NativeFn>(random_randint, 2, true)));
        
        // Sequence operations
        env->define("choice", Value(vm.allocate<NativeFn>(random_choice, 1, true)));
        env->define("shuffle", Value(vm.allocate<NativeFn>(random_shuffle, 1, true)));
        env->define("sample", Value(vm.allocate<NativeFn>(random_sample, 2, true)));
        
        // State management
        env->define("seed", Value(vm.allocate<NativeFn>(random_seed, -1, true)));
        
        // Distribution functions
        env->define("gauss", Value(vm.allocate<NativeFn>(random_gauss, 2, true)));
        env->define("expovariate", Value(vm.allocate<NativeFn>(random_expovariate, 1, true)));
        env->define("triangular", Value(vm.allocate<NativeFn>(random_triangular, 3, true)));
        
        // Utility functions
        env->define("getrandbits", Value(vm.allocate<NativeFn>(random_getrandbits, 1, true)));
    }
}

extern "C" void neutron_init_random_module(VM* vm) {
    auto random_env = std::make_shared<neutron::Environment>();
    neutron::register_random_functions(*vm, random_env);
    auto random_module = vm->allocate<neutron::Module>("random", random_env);
    vm->define_module("random", random_module);
}