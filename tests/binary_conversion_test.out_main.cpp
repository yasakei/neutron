// Auto-generated executable for tests/binary_conversion_test.nt
#include <iostream>
#include <string>
#include <vector>
#include "scanner.h"
#include "parser.h"
#include "vm.h"
#include "compiler.h"

const char* embedded_source = R"neutron_source(
// Binary conversion test script
// This script demonstrates what works and what doesn't with the binary conversion feature

say("=== Neutron Binary Conversion Test ===");
say("");

// Working features
say("1. Variables and Data Types");
var number = 42;
var text = "Hello, Neutron!";
var boolean = true;
var nilValue = nil;
say("Number: " + number);
say("Text: " + text);
say("Boolean: " + boolean);
say("Nil: " + nilValue);
say("");

say("2. Arithmetic Operations");
var a = 10;
var b = 3;
say("Addition: " + a + " + " + b + " = " + (a + b));
say("Subtraction: " + a + " - " + b + " = " + (a - b));
say("Multiplication: " + a + " * " + b + " = " + (a * b));
say("Division: " + a + " / " + b + " = " + (a / b));
say("");

say("3. String Operations");
var str1 = "Hello";
var str2 = "World";
say("Concatenation: " + str1 + " " + str2);
say("Length of '" + str1 + "': " + string_length(str1));
say("Character at index 1: " + string_get_char_at(str1, 1));
say("");

say("4. Conditional Statements");
var x = 5;
if (x > 0) {
    say("x is positive");
} else if (x < 0) {
    say("x is negative");
} else {
    say("x is zero");
}

var y = 10;
if (x < y) {
    say("x is less than y");
} else {
    say("x is greater than or equal to y");
}
say("");

say("5. Loops");
say("While loop:");
var i = 0;
while (i < 3) {
    say("i = " + i);
    i = i + 1;
}
say("");

say("6. Built-in Functions");
say("String representation of number: " + str(number));
say("Integer representation of string: " + int("123"));
say("Binary representation of 10: " + int_to_bin(10));
say("Integer from binary '1010': " + bin_to_int("1010"));
say("Character code of 'A': " + char_to_int("A"));
say("Character from code 65: " + int_to_char(65));
say("");

// Features that are NOT working:
say("7. Function Definitions (NOT SUPPORTED)");
say("The following would cause runtime errors if uncommented:");
say("// fun greet(name) {");
say("//     return \"Hello, \" + name + \"!\";");
say("// }");
say("// say(greet(\"Neutron\"));");
say("");

say("8. Module Imports (LIMITED SUPPORT)");
say("The following may not work in binary conversion:");
say("// use sys;");
say("// say(sys.cwd());");
say("");

say("=== End of Neutron Binary Conversion Test ===");

)neutron_source";

int main() {
    // Initialize the VM
    neutron::VM vm;

    // Run the embedded source code
    neutron::Scanner scanner(embedded_source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();

    neutron::Parser parser(tokens);
    std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();

    neutron::Compiler compiler(vm);
    neutron::Function* function = compiler.compile(statements);

    vm.interpret(function);

    return 0;
}
