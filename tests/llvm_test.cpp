#include "aot/llvm_codegen.h"
#include "compiler/bytecode.h"
#include "types/object.h"
#include "types/obj_string.h"
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

Value makeStr(const std::string& s) {
    Value v; v.type = ValueType::OBJ_STRING; v.as.obj_string = new ObjString(s); return v;
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

    // Test 7: global define and get
    {
        // define x = 42; say(x)
        Chunk c;
        auto gname = c.addConstant(makeStr("x"));
        auto gval = c.addConstant(makeNum(42));
        // 0: push 42
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)gval, 1);
        // 2: define global x (pops 42, stores in global x)
        c.write((uint8_t)OpCode::OP_DEFINE_GLOBAL, 2); c.write((uint8_t)gname, 2);
        // 4: get global x, push it
        c.write((uint8_t)OpCode::OP_GET_GLOBAL, 3); c.write((uint8_t)gname, 3);
        // 6: say(x)
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("global_define_get", c, "42");
    }

    // Test 8: global set and increment
    {
        // x = 10; say(x); x = x + 5; say(x)
        Chunk c;
        auto gname = c.addConstant(makeStr("y"));
        auto g10 = c.addConstant(makeNum(10));
        auto g5 = c.addConstant(makeNum(5));
        // 0: push 10
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)g10, 1);
        // 2: define global y
        c.write((uint8_t)OpCode::OP_DEFINE_GLOBAL, 2); c.write((uint8_t)gname, 2);
        // 4: get y
        c.write((uint8_t)OpCode::OP_GET_GLOBAL, 3); c.write((uint8_t)gname, 3);
        // 6: say(10)
        c.write((uint8_t)OpCode::OP_SAY, 4);
        // 7: push 5
        c.write((uint8_t)OpCode::OP_CONSTANT, 5); c.write((uint8_t)g5, 5);
        // 9: set global y (=5)
        c.write((uint8_t)OpCode::OP_SET_GLOBAL, 6); c.write((uint8_t)gname, 6);
        // 11: get y
        c.write((uint8_t)OpCode::OP_GET_GLOBAL, 7); c.write((uint8_t)gname, 7);
        // 13: say(5)
        c.write((uint8_t)OpCode::OP_SAY, 8);
        c.write((uint8_t)OpCode::OP_RETURN, 9);
        test("global_set", c, "10\n5");
    }

    // Test 9: global increment
    {
        // x = 0; x++; say(x);
        Chunk c;
        auto gname = c.addConstant(makeStr("z"));
        auto g0 = c.addConstant(makeNum(0));
        // 0: push 0
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)g0, 1);
        // 2: define global z
        c.write((uint8_t)OpCode::OP_DEFINE_GLOBAL, 2); c.write((uint8_t)gname, 2);
        // 4: increment global z
        c.write((uint8_t)OpCode::OP_INCREMENT_GLOBAL, 3); c.write((uint8_t)gname, 3);
        // 6: get z
        c.write((uint8_t)OpCode::OP_GET_GLOBAL, 4); c.write((uint8_t)gname, 4);
        // 8: say(z)
        c.write((uint8_t)OpCode::OP_SAY, 5);
        c.write((uint8_t)OpCode::OP_RETURN, 6);
        test("global_inc", c, "1");
    }

    // Test 10: OP_CALL fallback (pushes nil result)
    {
        Chunk c;
        auto i42 = c.addConstant(makeNum(42));
        // 0: push 42 (pretend it's a "callee")
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i42, 1);
        // 2: call with 0 args (pops callee, pushes nil)
        c.write((uint8_t)OpCode::OP_CALL, 2); c.write((uint8_t)0, 2);
        // 4: say nil result
        c.write((uint8_t)OpCode::OP_SAY, 3);
        c.write((uint8_t)OpCode::OP_RETURN, 4);
        test("call_fallback", c, "nil");
    }

    // Test 11: OP_CLOSURE (pushes function constant)
    {
        Chunk c;
        // In the real compiler, this would be a Function object in the constant pool.
        // For the test, we just need any value at constant index 0.
        auto dummy = c.addConstant(makeNum(99));
        // 0: closure (pushes constant[0] onto stack)
        c.write((uint8_t)OpCode::OP_CLOSURE, 1); c.write((uint8_t)dummy, 1);
        // 2: say (will print "99")
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("closure", c, "99");
    }

    // Test 12: OP_GET_UPVALUE / OP_SET_UPVALUE (fallback)
    {
        Chunk c;
        // get_upvalue 0 → pushes nil
        c.write((uint8_t)OpCode::OP_GET_UPVALUE, 1); c.write((uint8_t)0, 1);
        // say(nil)
        c.write((uint8_t)OpCode::OP_SAY, 2);
        // push 42, set_upvalue 0 → pops 42 (discarded)
        auto i42 = c.addConstant(makeNum(42));
        c.write((uint8_t)OpCode::OP_CONSTANT, 3); c.write((uint8_t)i42, 3);
        c.write((uint8_t)OpCode::OP_SET_UPVALUE, 4); c.write((uint8_t)0, 4);
        // push nil for return
        c.write((uint8_t)OpCode::OP_NIL, 5);
        c.write((uint8_t)OpCode::OP_RETURN, 6);
        test("upvalue_fallback", c, "nil");
    }

    // Test 13: OP_CLOSE_UPVALUE (fallback — pops and discards top)
    {
        Chunk c;
        auto i7 = c.addConstant(makeNum(7));
        // push 7
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i7, 1);
        // close_upvalue → pops and discards 7
        c.write((uint8_t)OpCode::OP_CLOSE_UPVALUE, 2);
        // push nil for return
        c.write((uint8_t)OpCode::OP_NIL, 3);
        // say(nil)
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("close_upvalue", c, "nil");
    }

    // Test 14: OP_THIS (pushes nil fallback)
    {
        Chunk c;
        c.write((uint8_t)OpCode::OP_THIS, 1);
        c.write((uint8_t)OpCode::OP_SAY, 2);
        c.write((uint8_t)OpCode::OP_RETURN, 3);
        test("this", c, "nil");
    }

    // Test 15: OP_SPREAD (pops array, pushes nil)
    {
        Chunk c;
        auto i7 = c.addConstant(makeNum(7));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i7, 1);
        c.write((uint8_t)OpCode::OP_SPREAD, 2);
        c.write((uint8_t)OpCode::OP_SAY, 3);
        c.write((uint8_t)OpCode::OP_RETURN, 4);
        test("spread", c, "nil");
    }

    // Test 16: OP_ARRAY (pops count, pushes nil)
    {
        Chunk c;
        auto i1 = c.addConstant(makeNum(1));
        auto i2 = c.addConstant(makeNum(2));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1);
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i2, 2);
        c.write((uint8_t)OpCode::OP_ARRAY, 3); c.write((uint8_t)2, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("array", c, "nil");
    }

    // Test 17: OP_INDEX_GET (pops obj+idx, pushes nil)
    {
        Chunk c;
        auto i0 = c.addConstant(makeNum(0));
        auto i1 = c.addConstant(makeNum(1));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1); // array
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i0, 2); // index
        c.write((uint8_t)OpCode::OP_INDEX_GET, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("index_get", c, "nil");
    }

    // Test 18: OP_INDEX_SET (pops obj+idx+val, pushes val back)
    {
        Chunk c;
        auto i0 = c.addConstant(makeNum(0));
        auto i1 = c.addConstant(makeNum(1));
        auto i99 = c.addConstant(makeNum(99));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1); // array
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i0, 2); // index
        c.write((uint8_t)OpCode::OP_CONSTANT, 3); c.write((uint8_t)i99, 3); // val
        c.write((uint8_t)OpCode::OP_INDEX_SET, 4);
        c.write((uint8_t)OpCode::OP_SAY, 5);
        c.write((uint8_t)OpCode::OP_RETURN, 6);
        test("index_set", c, "99");
    }

    // Test 19: OP_GET_PROPERTY (pops obj, pushes nil)
    {
        Chunk c;
        auto prop = c.addConstant(makeStr("x"));
        auto i1 = c.addConstant(makeNum(1));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1);
        c.write((uint8_t)OpCode::OP_GET_PROPERTY, 2); c.write((uint8_t)prop, 2);
        c.write((uint8_t)OpCode::OP_SAY, 3);
        c.write((uint8_t)OpCode::OP_RETURN, 4);
        test("get_prop", c, "nil");
    }

    // Test 20: OP_SET_PROPERTY (pops obj+val, pushes val back)
    {
        Chunk c;
        auto prop = c.addConstant(makeStr("x"));
        auto i1 = c.addConstant(makeNum(1));
        auto i99 = c.addConstant(makeNum(99));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1); // obj
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i99, 2); // val
        c.write((uint8_t)OpCode::OP_SET_PROPERTY, 3); c.write((uint8_t)prop, 3);
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("set_prop", c, "99");
    }

    // Test 21: OP_OBJECT (pops 2*count, pushes nil)
    {
        Chunk c;
        auto key = c.addConstant(makeStr("k"));
        auto val = c.addConstant(makeNum(42));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)key, 1); // key
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)val, 2); // val
        c.write((uint8_t)OpCode::OP_OBJECT, 3); c.write((uint8_t)1, 3); // 1 pair
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("object", c, "nil");
    }

    // Test 22: OP_INVOKE (pops args+receiver, pushes nil)
    {
        Chunk c;
        auto meth = c.addConstant(makeStr("foo"));
        auto i1 = c.addConstant(makeNum(1));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1); // receiver
        c.write((uint8_t)OpCode::OP_CONSTANT, 2); c.write((uint8_t)i1, 2); // arg
        c.write((uint8_t)OpCode::OP_INVOKE, 3); c.write((uint8_t)meth, 3); c.write((uint8_t)1, 3); // 1 arg
        c.write((uint8_t)OpCode::OP_SAY, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("invoke", c, "nil");
    }

    // Test 23: OP_OPTIONAL_CHAIN (pops obj, pushes nil)
    {
        Chunk c;
        auto prop = c.addConstant(makeStr("y"));
        auto i1 = c.addConstant(makeNum(1));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1);
        c.write((uint8_t)OpCode::OP_OPTIONAL_CHAIN, 2); c.write((uint8_t)prop, 2);
        c.write((uint8_t)OpCode::OP_SAY, 3);
        c.write((uint8_t)OpCode::OP_RETURN, 4);
        test("opt_chain", c, "nil");
    }

    // Test 24: OP_FOR_IN_INIT + OP_FOR_IN_NEXT (offset = 0 → fall through)
    {
        Chunk c;
        auto i1 = c.addConstant(makeNum(1));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i1, 1);
        c.write((uint8_t)OpCode::OP_FOR_IN_INIT, 2);
        c.write((uint8_t)OpCode::OP_FOR_IN_NEXT, 3);
        c.write((uint8_t)0, 3); // baseSlot
        c.write((uint8_t)1, 3); // varSlot
        c.write((uint8_t)0, 3); c.write((uint8_t)0, 3); // offset = 0 → fall through
        c.write((uint8_t)OpCode::OP_NIL, 4);
        c.write((uint8_t)OpCode::OP_SAY, 5);
        c.write((uint8_t)OpCode::OP_RETURN, 6);
        test("for_in", c, "nil");
    }

    // Test 25: OP_THROW (pops exception, pushes nil fallback)
    {
        Chunk c;
        auto i7 = c.addConstant(makeNum(7));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i7, 1);
        c.write((uint8_t)OpCode::OP_THROW, 2);
        c.write((uint8_t)OpCode::OP_SAY, 3);
        c.write((uint8_t)OpCode::OP_NIL, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("throw_fallback", c, "nil");
    }

    // Test 26: OP_TAIL_CALL (same fallback as OP_CALL)
    {
        Chunk c;
        auto i42 = c.addConstant(makeNum(42));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i42, 1);
        // Tail call with 0 args: pops callee, pushes nil
        c.write((uint8_t)OpCode::OP_TAIL_CALL, 2); c.write((uint8_t)0, 2);
        c.write((uint8_t)OpCode::OP_SAY, 3);
        c.write((uint8_t)OpCode::OP_NIL, 4);
        c.write((uint8_t)OpCode::OP_RETURN, 5);
        test("tail_call", c, "nil");
    }

    // Test 27: OP_BREAK / OP_CONTINUE / OP_END_TRY (no-ops)
    {
        Chunk c;
        auto i42 = c.addConstant(makeNum(42));
        c.write((uint8_t)OpCode::OP_CONSTANT, 1); c.write((uint8_t)i42, 1);
        c.write((uint8_t)OpCode::OP_BREAK, 2);
        c.write((uint8_t)OpCode::OP_CONTINUE, 3);
        c.write((uint8_t)OpCode::OP_END_TRY, 4);
        c.write((uint8_t)OpCode::OP_SAY, 5);
        c.write((uint8_t)OpCode::OP_NIL, 6);
        c.write((uint8_t)OpCode::OP_RETURN, 7);
        test("break_cont_endtry", c, "42");
    }

    std::cout << "\n" << passed << "/" << tests << " passed\n";
    return passed == tests ? 0 : 1;
}
