# Axity Language Guide

## Table of Contents

1. [Overview](#overview)
2. [Types](#types)

   * Primitive Types
   * Composite Types
   * Special Types (`any`, `buffer`, `class`)
3. [Variables](#variables)
4. [Expressions and Operators](#expressions-and-operators)

   * Arithmetic
   * Logical and Comparison
   * Bitwise
   * Strings
   * Postfix Increment/Decrement
   * Numerics and Trigonometry
5. [Control Flow](#control-flow)

   * Conditional Statements
   * Loops
   * Retry Statement
   * Match / Case
   * Exceptions
6. [Functions](#functions)

   * Declaration and Return Types
   * Examples by Type
7. [Lambdas and IIFE](#lambdas-and-iife)
8. [Classes and Objects](#classes-and-objects)

   * Class Initialization
9. [Dynamic Objects (`obj`)](#dynamic-objects-obj)
10. [Arrays and Maps](#arrays-and-maps)

    * Map Key Iteration

11. [Strings](#strings)
12. [Buffers](#buffers)
13. [Math Functions](#math-functions)
14. [IO: Files, JSON, TOML, ENV](#io-files-json-toml-env)
15. [Imports](#imports)
16. [REPL and Debug](#repl-and-debug)
17. [Printing](#printing)
18. [Comments](#comments)
19. [Examples](#examples)

---

## Overview

Axity is a statically typed, flexible programming language designed for simplicity and productivity. It supports primitive and dynamic types, functional programming features, object-oriented constructs, and robust IO utilities.

**Key Features:**

* Static types: `int`, `str`, `flt`, `bool`, `array<T>`, `map<T>`, `obj`, `buffer`, `class`
* Dynamic type: `any`
* Control flow: `if/else`, `while`, `do/while`, `for`, `foreach`, `match/case`, `try/catch`, `throw`, `retry`
* Operators: arithmetic, logical (`and/or`), bitwise, string concatenation, postfix increment/decrement
* Strings: interpolation with `!{}`
* Lambdas and IIFE for functional programming
* Buffers: mutable byte arrays with conversion utilities

---

## Types

### Primitive Types

```axity
let n: int = 42;
n = n + 8;
print(n);

let x: flt = 1.5;
let y: flt = 1.5e2;
let z: flt = -3.25;
print(x);
print(y);
print(z);

let s: str = "hello";
let flag: bool = true;
print(!flag);
```

---

### Composite Types

```axity
let xs: array<int> = [
    1,
    2,
    3
];
push(xs, 4);
set(xs, 0, 9);
print(xs[0]);

let m: map<str> = map_new_string();
map_set(m, "name", "Alice");
print(map_get(m, "name"));

let user: obj = {
    "name": "Alice",
    "meta": {
        "age": 30,
        "city": "NY"
    }
};
print(user.name);
print(user.meta.city);
```

---

### Special Types

```axity
let v: any = 1;
v = "hi";
v = {
    "k": "v"
};
print(v.k);

let buf: buffer = buffer_from_string("hello");
buffer_push(buf, 33);
print(buffer_to_string(buf));

class Box {
    let x: int;
}
let b: Box = new Box();
print(b.x);
```

---

## Variables

```axity
let a: int = 1;
let s: str = "hello";

a = a + 2;
print(a);
print(s);

let flag: bool = true;
print(flag);
```

---

## Expressions and Operators

### Arithmetic

```axity
print(5 + 3);
print(10 - 4);
print(6 * 7);
print(15 / 3);
print(10 % 3);
```

### Logical and Comparison

```axity
print(true && false);
print(true or false);

print(3 < 5);
print(5 <= 5);
print(7 > 3);
print(5 >= 4);
print(5 == 5);
print(5 != 3);
```

### Bitwise

```axity
print(5 & 1);
print(5 | 1);
print(5 ^ 1);
print(5 << 1);
print(5 >> 1);
print(~5);
```

### Strings

```axity
let name: str = "George";

print("Hello " + name);
print("Hello !{name}");
```

### Postfix Increment / Decrement

```axity
let x: int = 1;
x++;
print(x);

let y: int = 2;
y--;
print(y);
```

### Numerics and Trigonometry

```axity
let a: flt = 1.5e1;
print(a);

print(sin(0));
print(cos(0));
print(tan(0));

let neg: flt = -3.25;
print(neg);
```

---

## Control Flow

### Conditional Statements

```axity
if x == 5 {
    print("Five");
} else {
    print("Other");
}
```

### Loops

**While Loop**

```axity
let x: int = 0;

while x < 3 {
    print(x);
    x++;
}
```

**Do-While Loop**

```axity
do {
    print(x);
    x++;
} while x < 5;
```

**For Loop (C-style)**

```axity
for let i: int = 0; i < 3; i++ {
    print(i);
}
```

**Foreach Array with Retry**

```axity
for n in xs {
    if n == 2 {
        retry;
    }
    print(n);
}
```

**Foreach Map**

```axity
for k in m {
    print(k);
}
```

### Retry Statement

```axity
for n in xs {
    if n < 0 {
        retry; // Skips current iteration and restarts loop
    }
    print(n);
}
```

### Match / Case

```axity
match x {
    case 5: {
        print("five");
    }
    default: {
        print("other");
    }
}
```

### Exceptions

```axity
try {
    throw "boom";
} catch err {
    print(err);
}
```

---

## Functions

```axity
fn add(a: int, b: int) -> int {
    return a + b;
}

fn greet(name: str) -> str {
    return "Hello " + name;
}

fn pi() -> flt {
    return 3.141593;
}

fn make_array() -> array<int> {
    return [
        1,
        2,
        3
    ];
}

fn make_map() -> map<str> {
    let m: map<str> = map_new_string();
    map_set(m, "key", "value");
    return m;
}

fn make_obj() -> obj {
    return {
        "name": "Alice",
        "age": 30
    };
}

fn make_buf() -> buffer {
    return buffer_from_string("hello");
}

fn make_box() -> Box {
    let b: Box = new Box();
    return b;
}

fn id(x: any) -> any {
    return x;
}
```

---

## Lambdas and IIFE

```axity
let inc: obj = fn (a: int) -> int {
    return a + 1;
};
print(inc(2));

let iife: int = fn (a: int, b: int) -> int {
    return a + b;
}(2, 3);
print(iife);
```

---

## Classes and Objects

```axity
class Box {
    let x: int;

    fn init(self: Box) -> int {
        self.x = 0;
        return 0;
    }

    fn inc(self: Box) -> int {
        self.x = self.x + 1;
        return 0;
    }
}

let b: Box = new Box();
b.init();
b.inc();
print(b.x);
```

---

## Dynamic Objects (`obj`)

```axity
let user: obj = {
    "name": "Alice",
    "meta": {
        "age": 30,
        "city": "NY"
    }
};
print(user.name);
print(user.meta.age);
```

---

## Arrays and Maps

**Arrays**

```axity
let arr: array<int> = [
    1,
    2,
    3
];
push(arr, 4);
set(arr, 1, 5);
let last: int = pop(arr);
print(arr);
print(last);
```

**Maps**

```axity
let m: map<str> = map_new_string();
map_set(m, "name", "Alice");
print(map_get(m, "name"));
print(map_has(m, "name"));

map_remove(m, "name");
map_clear(m);
```

**Map Key Iteration**

```axity
let keys: array<str> = map_keys(m);
for k in keys {
    print(k);
}
```

---

## Strings

```axity
print(strlen("George"));
print(substr("George", 0, 3));
print(index_of("George", "or"));

let parts: array<str> = string_split("a,b,c", ",");
print(parts);

print(to_int("42"));
print(to_string(7));
```

---

## Buffers

```axity
let buf: buffer = buffer_new(3);
buffer_set(buf, 0, 65);
buffer_set(buf, 1, 66);
buffer_set(buf, 2, 67);

print(buffer_len(buf));
print(buffer_to_string(buf));

let buf2: buffer = buffer_from_string("hi");
buffer_push(buf2, 33);
print(buffer_to_string(buf2));
print(buffer_get(buf2, 0));
```

---

## Math Functions

```axity
print(sin(0));
print(cos(0));
print(tan(0));
print(exp(1));
print(log(10));
print(sqrt(16));
```

---

## IO: Files, JSON, TOML, ENV

```axity
write_file("out.txt", "hello");
print(read_file("out.txt"));

let j = read_json("data.json");
json_set(j, "key", "new");
write_json("data.json", j);

let t = read_toml("conf.toml");
toml_set(t, "db.port", 5433);
write_toml("conf.toml", t);

let e = read_env(".env");
env_set(e, "TOKEN", "abc123");
write_env(".env", e);
```

---

## Imports

```axity
import "import_functions.ax";
print(add(2, 3));

run_file("file.ax");
```

---

## REPL and Debug

* Start REPL: `cargo run -- repl`
* Commands: `:load`, `:env`, `:quit`
* Debug: `--dump-tokens`, `--dump-ast`

---

## Printing

```axity
print([1, 2]);

print({
    "k": "v"
});

print({
    "user": {
        "name": "Alice"
    },
    "nums": [
        1,
        2
    ]
});
```

---

## Comments

* **Single-line:** `// comment`
* **Multi-line:**

```axity
/// 
This is a multi-line comment block
///
```

---

## Examples

* `functions_returns.ax`
* `objects.ax`
* `operators.ax`
* `comparisons.ax`
* `bitwise.ax`
* `logical.ax`
* `loops_do_for.ax`
* `iife_and_buffers.ax`
* `floats_trig.ax`
