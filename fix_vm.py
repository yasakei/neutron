#!/usr/bin/env python3

import re

# Read the file
with open('/Users/yas/Projects/neutron/src/vm.cpp', 'r') as f:
    content = f.read()

# Replace the problematic section by finding the area with escaped newlines
def fix_escaped_newlines(text):
    # Replace escaped newlines with actual newlines
    text = re.sub(r'\\n', '\n', text)
    
    # Fix the indentation for the array opcodes
    # This is a more targeted fix for the specific area
    pattern = r'case \(uint8_t\)OpCode::OP_LOOP: \{\s*\n\s*uint16_t offset = READ_SHORT\(\);\s*\n\s*frame->ip -= offset;\s*\n\s*break;\s*\n\s*\}[\s\S]*?break;\s*\n\s*\}[\s\S]*?case \(uint8_t\)OpCode::OP_CALL: \{'
    
    # We'll replace the entire problematic section
    fixed_content = text.replace(
        'case (uint8_t)OpCode::OP_LOOP: {\n                uint16_t offset = READ_SHORT();\n                frame->ip -= offset;\n                break;\n            }\n            case (uint8_t)OpCode::OP_CALL: {\\n                uint8_t argCount = READ_BYTE();\\n                if (!callValue(stack[stack.size() - argCount - 1], argCount)) {\\n                    return;\\n                }\\n                frame = &frames.back();\\n                break;\\n            }\\n            case (uint8_t)OpCode::OP_ARRAY: {\\n                uint8_t count = READ_BYTE();\\n                std::vector<Value> elements;\\n                elements.reserve(count);\\n                \\n                // Pop \'count\' elements from the stack in reverse order\\n                for (int i = 0; i < count; i++) {\\n                    elements.insert(elements.begin(), pop());\\n                }\\n                \\n                push(Value(new Array(std::move(elements))));\\n                break;\\n            }\\n            case (uint8_t)OpCode::OP_INDEX_GET: {\\n                Value index = pop();\\n                Value object = pop();\\n                \\n                if (object.type == ValueType::ARRAY) {\\n                    if (index.type != ValueType::NUMBER) {\\n                        runtimeError(\\\"Array index must be a number.\\\");\\n                        return;\\n                    }\\n                    \\n                    int idx = static_cast<int>(std::get<double>(index.as));\\n                    Array* array = std::get<Array*>(object.as);\\n                    \\n                    if (idx < 0 || idx >= static_cast<int>(array->size())) {\\n                        runtimeError(\\\"Array index out of bounds.\\\");\\n                        return;\\n                    }\\n                    \\n                    push(array->at(idx));\\n                } else {\\n                    runtimeError(\\\"Only arrays support index access.\\\");\\n                    return;\\n                }\\n                break;\\n            }\\n            case (uint8_t)OpCode::OP_INDEX_SET: {\\n                Value value = pop();\\n                Value index = pop();\\n                Value object = pop();\\n                \\n                if (object.type == ValueType::ARRAY) {\\n                    if (index.type != ValueType::NUMBER) {\\n                        runtimeError(\\\"Array index must be a number.\\\");\\n                        return;\\n                    }\\n                    \\n                    int idx = static_cast<int>(std::get<double>(index.as));\\n                    Array* array = std::get<Array*>(object.as);\\n                    \\n                    if (idx < 0 || idx >= static_cast<int>(array->size())) {\\n                        runtimeError(\\\"Array index out of bounds.\\\");\\n                        return;\\n                    }\\n                    \\n                    array->set(idx, value);\\n                    push(value); // Return the assigned value\\n                } else {\\n                    runtimeError(\\\"Only arrays support index assignment.\\\");\\n                    return;\\n                }\\n                break;\\n            }',
        '''            case (uint8_t)OpCode::OP_CALL: {
                uint8_t argCount = READ_BYTE();
                if (!callValue(stack[stack.size() - argCount - 1], argCount)) {
                    return;
                }
                frame = &frames.back();
                break;
            }
            case (uint8_t)OpCode::OP_ARRAY: {
                uint8_t count = READ_BYTE();
                std::vector<Value> elements;
                elements.reserve(count);
                
                // Pop 'count' elements from the stack in reverse order
                for (int i = 0; i < count; i++) {
                    elements.insert(elements.begin(), pop());
                }
                
                push(Value(new Array(std::move(elements))));
                break;
            }
            case (uint8_t)OpCode::OP_INDEX_GET: {
                Value index = pop();
                Value object = pop();
                
                if (object.type == ValueType::ARRAY) {
                    if (index.type != ValueType::NUMBER) {
                        runtimeError("Array index must be a number.");
                        return;
                    }
                    
                    int idx = static_cast<int>(std::get<double>(index.as));
                    Array* array = std::get<Array*>(object.as);
                    
                    if (idx < 0 || idx >= static_cast<int>(array->size())) {
                        runtimeError("Array index out of bounds.");
                        return;
                    }
                    
                    push(array->at(idx));
                } else {
                    runtimeError("Only arrays support index access.");
                    return;
                }
                break;
            }
            case (uint8_t)OpCode::OP_INDEX_SET: {
                Value value = pop();
                Value index = pop();
                Value object = pop();
                
                if (object.type == ValueType::ARRAY) {
                    if (index.type != ValueType::NUMBER) {
                        runtimeError("Array index must be a number.");
                        return;
                    }
                    
                    int idx = static_cast<int>(std::get<double>(index.as));
                    Array* array = std::get<Array*>(object.as);
                    
                    if (idx < 0 || idx >= static_cast<int>(array->size())) {
                        runtimeError("Array index out of bounds.");
                        return;
                    }
                    
                    array->set(idx, value);
                    push(value); // Return the assigned value
                } else {
                    runtimeError("Only arrays support index assignment.");
                    return;
                }
                break;
            }'''
    )
    
    return fixed_content

# Apply the fix
fixed_content = fix_escaped_newlines(content)

# Write the fixed content back to the file
with open('/Users/yas/Projects/neutron/src/vm.cpp', 'w') as f:
    f.write(fixed_content)

print("Fixed vm.cpp file")