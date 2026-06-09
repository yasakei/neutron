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
    std::string wrapper = std::string("/tmp/llvm_wrap_") + name + ".cpp";
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
    pclose(fp);
    while (!output.empty() && output.back() == '\n') output.pop_back();
    if (output == expected) {
        passed++;
        std::cout << "PASS: " << name << "\n";
    } else {
        std::cout << "FAIL: " << name << " (expected=\"" << expected << "\", got=\"" << output << "\")\n";
    }
}

Value makeNum(double n) {
    Value v; v.type = ValueType::NUMBER; v.as.number = n; return v;
}

int main() {
    // Test 1: push 42, say it
    {
        Chunk c;
        auto idx = c.addConstant(makeNum(42));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)idx, 1);
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("const_42", c, "42");
    }

    // Test 2: 10 + 20 = 30
    {
        Chunk c;
        auto i10 = c.addConstant(makeNum(10));
        auto i20 = c.addConstant(makeNum(20));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i10, 1);
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i20, 2);
        c.write((uint8_t)OpCode::OP_ADD, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("add", c, "30");
    }

    // Test 3: locals
    {
        Chunk c;
        auto i99 = c.addConstant(makeNum(99));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i99, 1);
        c.write((uint8_t)OpCode::OP_SET_LOCAL, 2); c.write((uint8_t)0, 2);
        c.write((uint8_t)OpCode::OP_GET_LOCAL, 3); c.write((uint8_t)0, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("locals", c, "99");
    }

    // Test 4: conditional (if-true)
    {
        // if (1) { say(10); } else { say(20); }
        Chunk c;
        auto i1 = c.addConstant(makeNum(1));
        auto i10 = c.addConstant(makeNum(10));
        auto i20 = c.addConstant(makeNum(20));

        // 0: push 1 (condition)
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1);
        // 2: JUMP_IF_FALSE offset=6 → else at 2+3+6=11
        uint16_t jfOffset = 6;
        c.write((uint8_t)OpCode::OP_JUMP_IF_FALSE, 2);
        c.write((uint8_t)(jfOffset >> 8), 2); c.write((uint8_t)(jfOffset & 0xFF), 2);
        // 5: push 10 (then)
        c.write((uint8_t)OpCode::OP_CONSTANT, 3); c.write((uint8_t)i10, 3);
        // 7: say 10
        c.write((uint8_t)OpCode::OP_SAY, 4);
        // 8: JUMP offset=3 → skip else, target at 8+3+3=14
        uint16_t jOffset = 3;
        c.write((uint8_t)OpCode::OP_JUMP, 5);
        c.write((uint8_t)(jOffset >> 8), 5); c.write((uint8_t)(jOffset & 0xFF), 5);
        // 11: else: push 20
        c.write((uint8_t)OpCode::OP_CONSTANT, 6); c.write((uint8_t)i20, 6);
        // 13: say 20
        c.write((uint8_t)OpCode::OP_SAY, 7);
        // 14: return
        c.write((uint8_t)OpCode::OP_RETURN, 8);
        test("cond_true", c, "10");
    }

    // Test 5: conditional (if-false)
    {
        Chunk c;
        auto i0 = c.addConstant(makeNum(0));
        auto i10 = c.addConstant(makeNum(10));
        auto i20 = c.addConstant(makeNum(20));

        // 0: push 0 (condition, false)
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i0, 1);
        // 2: JUMP_IF_FALSE offset=6 → else at 11
        uint16_t jfOffset = 6;
        c.write((uint8_t)OpCode::OP_JUMP_IF_FALSE, 2);
        c.write((uint8_t)(jfOffset >> 8), 2); c.write((uint8_t)(jfOffset & 0xFF), 2);
        // 5: push 10 (then)
        c.write((uint8_t)OpCode::OP_CONSTANT, 3); c.write((uint8_t)i10, 3);
        // 7: say
        c.write((uint8_t)OpCode::OP_SAY, 4);
        // 8: JUMP offset=3 → skip else
        uint16_t jOffset = 3;
        c.write((uint8_t)OpCode::OP_JUMP, 5);
        c.write((uint8_t)(jOffset >> 8), 5); c.write((uint8_t)(jOffset & 0xFF), 5);
        // 11: else: push 20
        c.write((uint8_t)OpCode::OP_CONSTANT, 6); c.write((uint8_t)i20, 6);
        // 13: say
        c.write((uint8_t)OpCode::OP_SAY, 7);
        // 14: return
        c.write((uint8_t)OpCode::OP_RETURN, 8);
        test("cond_false", c, "20");
    }

    // Test 6: loop (count to 3)
    {
        // var i = 0;
        // loop: say(i); i = i + 1; if (i < 3) goto loop;
        Chunk c;
        auto i0 = c.addConstant(makeNum(0));
        auto i1c = c.addConstant(makeNum(1));
        auto i3 = c.addConstant(makeNum(3));

        // var i = 0
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i0, 1);
        c.write((uint8_t)OpCode::OP_SET_LOCAL, 2); c.write((uint8_t)0, 2);

        // loop: say(i)
        size_t loopStart = c.code.size();
        c.write((uint8_t)OpCode::OP_GET_LOCAL, 3); c.write((uint8_t)0, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);

        // i = i + 1
        c.write((uint8_t)OpCode::OP_GET_LOCAL, 5); c.write((uint8_t)0, 5);
        c.write((uint8_t)OpCode::OP_CONSTANT, 6); c.write((uint8_t)i1c, 6);
        c.write((uint8_t)OpCode::OP_ADD, 7);
        c.write((uint8_t)OpCode::OP_SET_LOCAL, 8); c.write((uint8_t)0, 8);

        // if (i < 3) goto loop
        c.write((uint8_t)OpCode::OP_GET_LOCAL, 9); c.write((uint8_t)0, 9);
        c.write((uint8_t)OpCode::OP_CONSTANT, 10); c.write((uint8_t)i3, 10);
        c.write((uint8_t)OpCode::OP_LESS, 11);

        // At this point code.size() = 19
        // JUMP_IF_FALSE at byte 19 → ip after instr = 22, target = 25 (OP_RETURN), offset = 3
        uint16_t jfOff = 3;
        c.write((uint8_t)OpCode::OP_JUMP_IF_FALSE, 12);
        c.write((uint8_t)(jfOff >> 8), 12); c.write((uint8_t)(jfOff & 0xFF), 12);

        // OP_LOOP at byte 22 → ip after = 22+1+2=25, target = loopStart=4, offset = 25-4=21
        uint16_t loopOff = (c.code.size() + 3) - loopStart;
        c.write((uint8_t)OpCode::OP_LOOP, 13);
        c.write((uint8_t)(loopOff >> 8), 13); c.write((uint8_t)(loopOff & 0xFF), 13);

        c.write((uint8_t)OpCode::OP_RETURN, 14);
        test("loop", c, "0\n1\n2");
    }

    std::cout << "\n" << passed << "/" << tests << " passed\n";
    return passed == tests ? 0 : 1;
}
