#include "aot/llvm_codegen.h"
#include "compiler/bytecode.h"
#include "types/value.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

using namespace neutron;
using namespace neutron::aot;

static int tests = 0, passed = 0;

void test(const char* name, Chunk& chunk, const char* expected) {
    tests++;
    LlvmCodegen codegen(&chunk);
    std::string path = std::string("/tmp/llvm_") + name + ".o";
    if (!codegen.generateModule("test_main", path)) {
        std::cout << "FAIL: " << name << " (codegen failed)\n";
        return;
    }
    // Compile wrapper + link + run
    std::string wrapper = "/tmp/llvm_wrap_" + std::string(name) + ".cpp";
    std::string binary = "/tmp/llvm_bin_" + std::string(name);
    {
        FILE* f = fopen(wrapper.c_str(), "w");
        fprintf(f, "extern \"C\" int test_main(); int main() { return test_main(); }\n");
        fclose(f);
    }
    std::string cmd = "g++ -no-pie -o " + binary + " " + path + " " + wrapper + " 2>&1";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cout << "FAIL: " << name << " (link failed)\n";
        return;
    }
    std::string runCmd = binary + " 2>&1";
    FILE* fp = popen(runCmd.c_str(), "r");
    if (!fp) {
        std::cout << "FAIL: " << name << " (run failed)\n";
        return;
    }
    std::string output;
    char buf[256] = {};
    while (fgets(buf, sizeof(buf), fp)) {
        output += buf;
    }
    int exitCode = pclose(fp);
    // Trim trailing newline
    while (!output.empty() && output.back() == '\n') output.pop_back();
    if (output == expected) {
        passed++;
        std::cout << "PASS: " << name << " (\"" << buf << "\")\n";
    } else {
        std::cout << "FAIL: " << name << " (expected=\"" << expected << "\", got=\"" << buf << "\")\n";
    }
}

int main() {
    // Test 1: push 42, say it
    {
        Chunk c;
        Value n; n.type = ValueType::NUMBER; n.as.number = 42;
        auto idx = c.addConstant(n);
        c.write((uint8_t)OpCode::OP_CONSTANT, 1);
        c.write((uint8_t)idx, 1);
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("const_42", c, "42");
    }
    // Test 2: 10 + 20 = 30
    {
        Chunk c;
        Value n10; n10.type = ValueType::NUMBER; n10.as.number = 10;
        Value n20; n20.type = ValueType::NUMBER; n20.as.number = 20;
        auto i10 = c.addConstant(n10);
        auto i20 = c.addConstant(n20);
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i10, 1);
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i20, 2);
        c.write((uint8_t)OpCode::OP_ADD, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("add_10_20", c, "30");
    }
    // Test 3: 50 - 15 = 35
    {
        Chunk c;
        Value n50; n50.type = ValueType::NUMBER; n50.as.number = 50;
        Value n15; n15.type = ValueType::NUMBER; n15.as.number = 15;
        auto i50 = c.addConstant(n50);
        auto i15 = c.addConstant(n15);
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i50, 1);
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i15, 2);
        c.write((uint8_t)OpCode::OP_SUBTRACT, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("sub_50_15", c, "35");
    }
    // Test 4: 6 * 7 = 42
    {
        Chunk c;
        Value n6; n6.type = ValueType::NUMBER; n6.as.number = 6;
        Value n7; n7.type = ValueType::NUMBER; n7.as.number = 7;
        auto i6 = c.addConstant(n6);
        auto i7 = c.addConstant(n7);
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i6, 1);
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i7, 2);
        c.write((uint8_t)OpCode::OP_MULTIPLY, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("mul_6_7", c, "42");
    }
    // Test 5: 100 / 3 = 33 (integer division, truncated)
    {
        Chunk c;
        Value n100; n100.type = ValueType::NUMBER; n100.as.number = 100;
        Value n3; n3.type = ValueType::NUMBER; n3.as.number = 3;
        auto i100 = c.addConstant(n100);
        auto i3 = c.addConstant(n3);
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i100, 1);
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i3, 2);
        c.write((uint8_t)OpCode::OP_DIVIDE, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("div_100_3", c, "33.3333");
    }
    // Test 6: nil
    {
        Chunk c;
        c.write((uint8_t)OpCode::OP_NIL, 1);
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("nil", c, "nil");
    }
    // Test 7: true
    {
        Chunk c;
        c.write((uint8_t)OpCode::OP_TRUE, 1);
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("true", c, "true");
    }
    // Test 8: false
    {
        Chunk c;
        c.write((uint8_t)OpCode::OP_FALSE, 1);
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("false", c, "false");
    }
    // Test 9: locals
    {
        Chunk c;
        Value n99; n99.type = ValueType::NUMBER; n99.as.number = 99;
        auto i99 = c.addConstant(n99);
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i99, 1);
        c.write((uint8_t)OpCode::OP_SET_LOCAL, 2); c.write((uint8_t)0, 2);
        c.write((uint8_t)OpCode::OP_GET_LOCAL, 3); c.write((uint8_t)0, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("locals", c, "99");
    }

    std::cout << "\n" << passed << "/" << tests << " passed\n";
    return passed == tests ? 0 : 1;
}
