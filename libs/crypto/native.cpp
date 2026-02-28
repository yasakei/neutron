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
#include "native.h"
#include "vm.h"
#include "types/array.h"
#include "types/obj_string.h"
#include "types/native_fn.h"
#include "modules/module.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdint>

// Cross-platform includes
#ifdef _WIN32
    #include <windows.h>
    #include <wincrypt.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

using namespace neutron;

// Base64 encoding table
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Helper function to check if character is base64
static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

// Base64 encode function
std::string base64_encode(const std::vector<uint8_t>& bytes) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (size_t idx = 0; idx < bytes.size(); idx++) {
        char_array_3[i++] = bytes[idx];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
            
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
            
        while((i++ < 3))
            ret += '=';
    }
    
    return ret;
}

// Base64 decode function
std::vector<uint8_t> base64_decode(const std::string& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;
    
    // First, validate that all characters are valid base64
    for (size_t idx = 0; idx < encoded_string.length(); idx++) {
        char c = encoded_string[idx];
        if (c != '=' && !is_base64(c)) {
            throw std::runtime_error("Invalid base64 character: " + std::string(1, c));
        }
    }
    
    while (in_len-- && ( encoded_string[in] != '=') && is_base64(encoded_string[in])) {
        char_array_4[i++] = encoded_string[in]; in++;
        if (i ==4) {
            for (i = 0; i <4; i++) {
                size_t pos = base64_chars.find(char_array_4[i]);
                if (pos == std::string::npos) {
                    throw std::runtime_error("Invalid base64 character");
                }
                char_array_4[i] = static_cast<unsigned char>(pos);
            }
                
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;
            
        for (j = 0; j <4; j++) {
            size_t pos = base64_chars.find(char_array_4[j]);
            if (pos == std::string::npos) {
                char_array_4[j] = 0;
            } else {
                char_array_4[j] = static_cast<unsigned char>(pos);
            }
        }
            
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
    }
    
    return ret;
}

// Simple SHA-256 implementation (for educational purposes - in production use a proper crypto library)
class SHA256 {
private:
    typedef unsigned char uint8;
    typedef uint32_t uint32;
    typedef unsigned long long uint64;
    
    const static uint32_t sha256_k[];
    static const unsigned int SHA224_256_BLOCK_SIZE = (512/8);
    
public:
    void init();
    void update(const unsigned char *message, unsigned int len);
    void final(unsigned char *digest);
    static const unsigned int DIGEST_SIZE = ( 256 / 8);
    
protected:
    void transform(const unsigned char *message, unsigned int block_nb);
    unsigned int m_tot_len;
    unsigned int m_len;
    unsigned char m_block[2*SHA224_256_BLOCK_SIZE];
    uint32_t m_h[8];
};

const uint32_t SHA256::sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define SHA2_SHFR(x, n)    (x >> n)
#define SHA2_ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define SHA2_ROTL(x, n)   ((x << n) | (x >> ((sizeof(x) << 3) - n)))
#define SHA2_CH(x, y, z)  ((x & y) ^ (~x & z))
#define SHA2_MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SHA256_F1(x) (SHA2_ROTR(x,  2) ^ SHA2_ROTR(x, 13) ^ SHA2_ROTR(x, 22))
#define SHA256_F2(x) (SHA2_ROTR(x,  6) ^ SHA2_ROTR(x, 11) ^ SHA2_ROTR(x, 25))
#define SHA256_F3(x) (SHA2_ROTR(x,  7) ^ SHA2_ROTR(x, 18) ^ SHA2_SHFR(x,  3))
#define SHA256_F4(x) (SHA2_ROTR(x, 17) ^ SHA2_ROTR(x, 19) ^ SHA2_SHFR(x, 10))
#define SHA2_UNPACK32(x, str)                 \
{                                             \
    *((str) + 3) = (uint8) ((x)      );       \
    *((str) + 2) = (uint8) ((x) >>  8);       \
    *((str) + 1) = (uint8) ((x) >> 16);       \
    *((str) + 0) = (uint8) ((x) >> 24);       \
}
#define SHA2_PACK32(str, x)                   \
{                                             \
    *(x) =   ((uint32_t) *((str) + 3)      )    \
           | ((uint32_t) *((str) + 2) <<  8)    \
           | ((uint32_t) *((str) + 1) << 16)    \
           | ((uint32_t) *((str) + 0) << 24);   \
}

void SHA256::transform(const unsigned char *message, unsigned int block_nb) {
    uint32_t w[64];
    uint32_t wv[8];
    uint32_t t1, t2;
    const unsigned char *sub_block;
    int i;
    int j;
    for (i = 0; i < (int) block_nb; i++) {
        sub_block = message + (i << 6);
        for (j = 0; j < 16; j++) {
            SHA2_PACK32(&sub_block[j << 2], &w[j]);
        }
        for (j = 16; j < 64; j++) {
            w[j] =  SHA256_F4(w[j -  2]) + w[j -  7] + SHA256_F3(w[j - 15]) + w[j - 16];
        }
        for (j = 0; j < 8; j++) {
            wv[j] = m_h[j];
        }
        for (j = 0; j < 64; j++) {
            t1 = wv[7] + SHA256_F2(wv[4]) + SHA2_CH(wv[4], wv[5], wv[6])
                + sha256_k[j] + w[j];
            t2 = SHA256_F1(wv[0]) + SHA2_MAJ(wv[0], wv[1], wv[2]);
            wv[7] = wv[6];
            wv[6] = wv[5];
            wv[5] = wv[4];
            wv[4] = wv[3] + t1;
            wv[3] = wv[2];
            wv[2] = wv[1];
            wv[1] = wv[0];
            wv[0] = t1 + t2;
        }
        for (j = 0; j < 8; j++) {
            m_h[j] += wv[j];
        }
    }
}

void SHA256::init() {
    m_h[0] = 0x6a09e667;
    m_h[1] = 0xbb67ae85;
    m_h[2] = 0x3c6ef372;
    m_h[3] = 0xa54ff53a;
    m_h[4] = 0x510e527f;
    m_h[5] = 0x9b05688c;
    m_h[6] = 0x1f83d9ab;
    m_h[7] = 0x5be0cd19;
    m_len = 0;
    m_tot_len = 0;
}

void SHA256::update(const unsigned char *message, unsigned int len) {
    unsigned int block_nb;
    unsigned int new_len, rem_len, tmp_len;
    const unsigned char *shifted_message;
    tmp_len = SHA224_256_BLOCK_SIZE - m_len;
    rem_len = len < tmp_len ? len : tmp_len;
    memcpy(&m_block[m_len], message, rem_len);
    if (m_len + len < SHA224_256_BLOCK_SIZE) {
        m_len += len;
        return;
    }
    new_len = len - rem_len;
    block_nb = new_len / SHA224_256_BLOCK_SIZE;
    shifted_message = message + rem_len;
    transform(m_block, 1);
    transform(shifted_message, block_nb);
    rem_len = new_len % SHA224_256_BLOCK_SIZE;
    memcpy(m_block, &shifted_message[block_nb << 6], rem_len);
    m_len = rem_len;
    m_tot_len += (block_nb + 1) << 6;
}

void SHA256::final(unsigned char *digest) {
    unsigned int block_nb;
    unsigned int pm_len;
    unsigned int len_b;
    int i;
    block_nb = (1 + ((SHA224_256_BLOCK_SIZE - 9)
                     < (m_len % SHA224_256_BLOCK_SIZE)));
    len_b = (m_tot_len + m_len) << 3;
    pm_len = block_nb << 6;
    memset(m_block + m_len, 0, pm_len - m_len);
    m_block[m_len] = 0x80;
    SHA2_UNPACK32(len_b, m_block + pm_len - 4);
    transform(m_block, block_nb);
    for (i = 0 ; i < 8; i++) {
        SHA2_UNPACK32(m_h[i], &digest[i << 2]);
    }
}

// Cross-platform secure random number generation
std::vector<uint8_t> generate_secure_random(size_t length) {
    std::vector<uint8_t> buffer(length);
    
#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("Failed to acquire cryptographic context");
    }
    
    if (!CryptGenRandom(hCryptProv, static_cast<DWORD>(length), buffer.data())) {
        CryptReleaseContext(hCryptProv, 0);
        throw std::runtime_error("Failed to generate random bytes");
    }
    
    CryptReleaseContext(hCryptProv, 0);
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open /dev/urandom");
    }
    
    ssize_t result = read(fd, buffer.data(), length);
    close(fd);
    
    if (result != static_cast<ssize_t>(length)) {
        throw std::runtime_error("Failed to read random bytes");
    }
#endif
    
    return buffer;
}

// Convert bytes to hex string
std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// Convert hex string to bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Crypto module functions

// Base64 encode
Value crypto_base64_encode(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.base64_encode().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for crypto.base64_encode() must be a string.");
    }
    
    std::string input = arguments[0].asString()->chars;
    std::vector<uint8_t> bytes(input.begin(), input.end());
    std::string encoded = base64_encode(bytes);
    
    return Value(vm.internString(encoded));
}

// Base64 decode
Value crypto_base64_decode(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.base64_decode().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for crypto.base64_decode() must be a string.");
    }
    
    std::string input = arguments[0].asString()->chars;
    try {
        std::vector<uint8_t> decoded = base64_decode(input);
        std::string result(decoded.begin(), decoded.end());
        return Value(vm.internString(result));
    } catch (...) {
        throw std::runtime_error("Invalid base64 string");
    }
}

// SHA-256 hash
Value crypto_sha256(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.sha256().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for crypto.sha256() must be a string.");
    }
    
    std::string input = arguments[0].asString()->chars;
    SHA256 sha256;
    sha256.init();
    sha256.update(reinterpret_cast<const unsigned char*>(input.c_str()), input.length());
    
    unsigned char digest[SHA256::DIGEST_SIZE];
    sha256.final(digest);
    
    std::vector<uint8_t> hash_bytes(digest, digest + SHA256::DIGEST_SIZE);
    std::string hex_hash = bytes_to_hex(hash_bytes);
    
    return Value(vm.internString(hex_hash));
}

// Generate random bytes
Value crypto_random_bytes(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.random_bytes().");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for crypto.random_bytes() must be a number.");
    }
    
    int length = static_cast<int>(arguments[0].as.number);
    if (length <= 0 || length > 1024 * 1024) { // Max 1MB
        throw std::runtime_error("Length must be between 1 and 1048576 bytes.");
    }
    
    try {
        std::vector<uint8_t> random_data = generate_secure_random(length);
        std::string hex_data = bytes_to_hex(random_data);
        return Value(vm.internString(hex_data));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to generate random bytes: " + std::string(e.what()));
    }
}

// Generate random string
Value crypto_random_string(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for crypto.random_string().");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("First argument for crypto.random_string() must be a number.");
    }
    
    int length = static_cast<int>(arguments[0].as.number);
    if (length <= 0 || length > 1024) {
        throw std::runtime_error("Length must be between 1 and 1024 characters.");
    }
    
    std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    if (arguments.size() == 2) {
        if (arguments[1].type != ValueType::OBJ_STRING) {
            throw std::runtime_error("Second argument for crypto.random_string() must be a string.");
        }
        charset = arguments[1].asString()->chars;
        if (charset.empty()) {
            throw std::runtime_error("Charset cannot be empty.");
        }
    }
    
    try {
        std::vector<uint8_t> random_data = generate_secure_random(length);
        std::string result;
        result.reserve(length);
        
        for (int i = 0; i < length; i++) {
            result += charset[random_data[i] % charset.length()];
        }
        
        return Value(vm.internString(result));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to generate random string: " + std::string(e.what()));
    }
}

// Simple MD5 hash (for compatibility - not recommended for security)
Value crypto_md5(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.md5().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for crypto.md5() must be a string.");
    }
    
    // Note: This is a placeholder - in a real implementation, you'd use a proper MD5 library
    // For now, we'll return a warning that MD5 is not secure
    throw std::runtime_error("MD5 is deprecated due to security vulnerabilities. Use sha256() instead.");
}

// Hex encode
Value crypto_hex_encode(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.hex_encode().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for crypto.hex_encode() must be a string.");
    }
    
    std::string input = arguments[0].asString()->chars;
    std::vector<uint8_t> bytes(input.begin(), input.end());
    std::string hex = bytes_to_hex(bytes);
    
    return Value(vm.internString(hex));
}

// Hex decode
Value crypto_hex_decode(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for crypto.hex_decode().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for crypto.hex_decode() must be a string.");
    }
    
    std::string input = arguments[0].asString()->chars;
    
    // Validate hex string
    if (input.length() % 2 != 0) {
        throw std::runtime_error("Hex string must have even length.");
    }
    
    for (char c : input) {
        if (!std::isxdigit(c)) {
            throw std::runtime_error("Invalid hex character: " + std::string(1, c));
        }
    }
    
    try {
        std::vector<uint8_t> bytes = hex_to_bytes(input);
        std::string result(bytes.begin(), bytes.end());
        return Value(vm.internString(result));
    } catch (...) {
        throw std::runtime_error("Failed to decode hex string");
    }
}

// Simple XOR cipher (for educational purposes)
Value crypto_xor_cipher(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for crypto.xor_cipher() (data, key).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING || arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Both arguments for crypto.xor_cipher() must be strings.");
    }
    
    std::string data = arguments[0].asString()->chars;
    std::string key = arguments[1].asString()->chars;
    
    if (key.empty()) {
        throw std::runtime_error("Key cannot be empty.");
    }
    
    std::string result;
    result.reserve(data.length());
    
    for (size_t i = 0; i < data.length(); i++) {
        result += static_cast<char>(data[i] ^ key[i % key.length()]);
    }
    
    return Value(vm.internString(result));
}

namespace neutron {
    void register_crypto_functions(VM& vm, std::shared_ptr<Environment> env) {
        // Base64 encoding/decoding
        env->define("base64_encode", Value(vm.allocate<NativeFn>(crypto_base64_encode, 1, true)));
        env->define("base64_decode", Value(vm.allocate<NativeFn>(crypto_base64_decode, 1, true)));
        
        // Hash functions
        env->define("sha256", Value(vm.allocate<NativeFn>(crypto_sha256, 1, true)));
        env->define("md5", Value(vm.allocate<NativeFn>(crypto_md5, 1, true)));
        
        // Random generation
        env->define("random_bytes", Value(vm.allocate<NativeFn>(crypto_random_bytes, 1, true)));
        env->define("random_string", Value(vm.allocate<NativeFn>(crypto_random_string, -1, true)));
        
        // Hex encoding/decoding
        env->define("hex_encode", Value(vm.allocate<NativeFn>(crypto_hex_encode, 1, true)));
        env->define("hex_decode", Value(vm.allocate<NativeFn>(crypto_hex_decode, 1, true)));
        
        // Simple ciphers (educational)
        env->define("xor_cipher", Value(vm.allocate<NativeFn>(crypto_xor_cipher, 2, true)));
    }
}

extern "C" void neutron_init_crypto_module(VM* vm) {
    auto crypto_env = std::make_shared<neutron::Environment>();
    neutron::register_crypto_functions(*vm, crypto_env);
    auto crypto_module = vm->allocate<neutron::Module>("crypto", crypto_env);
    vm->define_module("crypto", crypto_module);
}