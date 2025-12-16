# Axity Language Cheatsheet

---

## 1. Primitive Types

| Type   | Description                            | Default/Notes |
| ------ | -------------------------------------- | ------------- |
| `int`  | 64-bit integer                         | `0`           |
| `flt`  | Fixed-point float, 6 fractional digits | `0.0`         |
| `str`  | UTF-8 string                           | `""`          |
| `bool` | Boolean value                          | `false`       |

---

## 2. Composite Types

| Type       | Description        | Notes                             |
| ---------- | ------------------ | --------------------------------- |
| `array<T>` | Array of type `T`  | Dynamic size, indexed by integers |
| `map<T>`   | String-key map     | Dynamic, keys are strings         |
| `obj`      | Dynamic object     | String keys, any values           |
| `buffer`   | Mutable byte array | Supports `get/set/push`           |

---

## 3. Special Types

| Type    | Description       | Notes                                  |
| ------- | ----------------- | -------------------------------------- |
| `any`   | Dynamic value     | Can store any type                     |
| `class` | User-defined type | Supports fields, methods, constructors |

---

## 4. Variables

| Operation   | Syntax/Notes          |
| ----------- | --------------------- |
| Declaration | `let name: T = expr;` |
| Assignment  | `name = expr;`        |
| Scope       | Lexical, block-based  |

---

## 5. Arithmetic Operators

| Operator  | Description         | Notes                        |
| --------- | ------------------- | ---------------------------- |
| `+`       | Addition            | Works on `int`, `flt`, `str` |
| `-`       | Subtraction         | `int` / `flt` only           |
| `*`       | Multiplication      | `int` / `flt` only           |
| `/`       | Division            | `int` / `flt` only           |
| `%`       | Modulo              | `int` only                   |
| `++`      | Increment (postfix) | `int` only                   |
| `--`      | Decrement (postfix) | `int` only                   |
| Unary `-` | Negation            | `int` / `flt`                |

---

## 6. Logical Operators

| Operator     | Description | Notes              |
| ------------ | ----------- | ------------------ |
| `&&` / `and` | Logical AND | `bool` only        |
| `            | Logical OR | `bool` only         |
| `!`          | Logical NOT | Unary, `bool` only |

---

## 7. Comparison Operators

| Operator | Description           |
| -------- | --------------------- |
| `<`      | Less than             |
| `<=`     | Less than or equal    |
| `>`      | Greater than          |
| `>=`     | Greater than or equal |
| `==`     | Equal to              |
| `!=`     | Not equal to          |

---

## 8. Bitwise Operators

| Operator | Description | Notes       |
| -------- | ----------- | ----------- |
| `&`      | AND         | `int` only  |
| `        | OR          | `int` only  |
| `^`      | XOR         | `int` only  |
| `~`      | NOT         | Unary `int` |
| `<<`     | Left shift  | `int` only  |
| `>>`     | Right shift | `int` only  |

---

## 9. Control Flow Statements

| Statement / Loop       | Description                     |
| ---------------------- | ------------------------------- |
| `if/else`              | Conditional execution           |
| `while`                | Pre-condition loop              |
| `do { } while`         | Post-condition loop             |
| `for init; cond; post` | C-style loop                    |
| `for var in array`     | Array iteration                 |
| `for key in map`       | Map iteration                   |
| `match/case/default`   | Pattern matching                |
| `retry`                | Skip current iteration in loops |
| `try/catch/throw`      | Exception handling              |
| `return`               | Function return                 |

---

## 10. Functions

| Feature      | Notes                                                                              |
| ------------ | ---------------------------------------------------------------------------------- |
| Declaration  | `fn name(params) -> Ret { ... }`                                                   |
| Return Types | `int`, `flt`, `str`, `bool`, `array<T>`, `map<T>`, `obj`, `buffer`, `class`, `any` |
| Lambdas      | `fn(params) -> Ret { ... }`                                                        |
| IIFE         | `fn(params) -> Ret { ... }(args)`                                                  |

---

## 11. Classes & Objects

| Feature       | Notes                                   |
| ------------- | --------------------------------------- |
| Fields        | `let x: T;` inside class                |
| Methods       | `fn name(self: ClassType) -> Ret {}`    |
| Constructors  | Use `init()` method                     |
| Instantiation | `let obj: ClassType = new ClassType();` |

---

## 12. Arrays & Maps

| Operation              | Notes                          |
| ---------------------- | ------------------------------ |
| `push(arr, val)`       | Add element to array           |
| `pop(arr)`             | Remove and return last element |
| `set(arr, idx, val)`   | Set value at index             |
| `len(arr)`             | Get array length               |
| `map_set(m, key, val)` | Set key-value in map           |
| `map_get(m, key)`      | Get value from map             |
| `map_has(m, key)`      | Check if key exists            |
| `map_keys(m)`          | Returns array of keys          |
| `map_remove(m, key)`   | Remove key                     |
| `map_clear(m)`         | Clear all entries              |
| `map_size(m)`          | Get number of entries          |

---

## 13. Strings

| Function/Operator               | Description                 |
| ------------------------------- | --------------------------- |
| `+`                             | Concatenation               |
| `!{var}`                        | Interpolation               |
| `strlen(str)`                   | String length               |
| `substr(str, start, len)`       | Substring                   |
| `index_of(str, sub)`            | Index of substring          |
| `string_split(str, delim)`      | Split into array of strings |
| `string_replace(str, old, new)` | Replace substring           |
| `to_int(str)`                   | Convert to integer          |
| `to_string(val)`                | Convert to string           |

---

## 14. Buffers

| Operation                   | Notes                    |
| --------------------------- | ------------------------ |
| `buffer_new(size)`          | Create buffer            |
| `buffer_len(buf)`           | Get length               |
| `buffer_get(buf, idx)`      | Get byte at index        |
| `buffer_set(buf, idx, val)` | Set byte at index        |
| `buffer_push(buf, val)`     | Append byte              |
| `buffer_from_string(str)`   | Convert string to buffer |
| `buffer_to_string(buf)`     | Convert buffer to string |

---

## 15. Math / Trigonometry

| Function  | Notes        |
| --------- | ------------ |
| `sin(x)`  | Sine of x    |
| `cos(x)`  | Cosine of x  |
| `tan(x)`  | Tangent of x |
| `exp(x)`  | e^x          |
| `log(x)`  | Natural log  |
| `sqrt(x)` | Square root  |

---

## 16. IO Operations

| Domain | Functions                                         |
| ------ | ------------------------------------------------- |
| Files  | `read_file`, `write_file`, `exists`, `mkdir`      |
| JSON   | `read_json`, `write_json`, `json_get`, `json_set` |
| TOML   | `read_toml`, `write_toml`, `toml_get`, `toml_set` |
| ENV    | `read_env`, `write_env`, `env_get`, `env_set`     |

---

## 17. Imports / Execution

| Feature  | Notes                  |
| -------- | ---------------------- |
| Import   | `import "file.ax";`    |
| Run file | `run_file("file.ax");` |

---

## 18. REPL & Debug

| Feature    | Command                       |
| ---------- | ----------------------------- |
| Start REPL | `cargo run -- repl`           |
| Commands   | `:load`, `:env`, `:quit`      |
| Debug      | `--dump-tokens`, `--dump-ast` |

---

## 19. Printing

| Type    | Notes                            |
| ------- | -------------------------------- |
| Arrays  | `[1, 2, 3]`                      |
| Maps    | `{key: value}`                   |
| Objects | `Class{field: value}`            |
| Buffers | `<buffer len=N>`                 |
| Floats  | Printed with 6 fractional digits |

---

## 20. Comments

| Type        | Syntax                       |
| ----------- | ---------------------------- |
| Single-line | `// Comment`                 |
| Multi-line  | `/// Multi-line comment ///` |
