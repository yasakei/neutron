# AOT Module Interface Audit

## Calling Convention (all native modules)

```cpp
Value function_name(VM& vm, std::vector<Value> arguments);
```

Module init:
```cpp
extern "C" void neutron_init_<module>_module(neutron::VM* vm);
```

## AOT Stub Convention (aot_module.h)

```cpp
extern "C" Value aot_<name>(Value* args, int argCount);
```

## Module Summary

| Module | Source | AOT Stub | Functions |
|--------|--------|----------|-----------|
| math | `libs/math/native.cpp` | YES (11) | 14 |
| random | `libs/random/native.cpp` | YES (3) | 11 |
| fmt | `libs/fmt/native.cpp` | YES (2) | 17 |
| path | `libs/path/native.cpp` | YES (3) | 13 |
| strings | `libs/strings/native.cpp` | NO | 25 |
| arrays | `libs/arrays/native.cpp` | NO | 27 |
| json | `libs/json/native.cpp` | NO | 12 |
| http | `libs/http/native.cpp` | NO | 15 |
| sys | `libs/sys/native.cpp` | NO | 24 |
| time | `libs/time/native.cpp` | NO | 3 |
| crypto | `libs/crypto/native.cpp` | NO | 9 |
| process | `libs/process/native.cpp` | NO | 8 |
| regex | `libs/regex/native.cpp` | NO | 8 |
| async | `libs/async/native.cpp` | NO | 4 |
| log | `libs/log/native.cpp` | NO | 14 |
| collections | `libs/collections/native.cpp` | NO | 21 |

---

## math (14 exports, 11 AOT-stubbed)

`libs/math/native.cpp`, `libs/math/native.h`

| Function | Params | Return | Notes |
|----------|--------|--------|-------|
| `add` | (a: num, b: num) | num | a+b |
| `subtract` | (a: num, b: num) | num | a-b |
| `multiply` | (a: num, b: num) | num | a*b |
| `divide` | (a: num, b: num) | num | checks div by 0 |
| `sqrt` | (x: num) | num | ✓ AOT-stubbed |
| `pow` | (base: num, exp: num) | num | ✓ AOT-stubbed |
| `abs` | (x: num) | num | ✓ AOT-stubbed |
| `ceil` | (x: num) | num | ✓ AOT-stubbed |
| `floor` | (x: num) | num | ✓ AOT-stubbed |
| `round` | (x: num) | num | ✓ AOT-stubbed |
| `sin` | (x: num) | num | ✓ AOT-stubbed |
| `cos` | (x: num) | num | ✓ AOT-stubbed |
| `tan` | (x: num) | num | ✓ AOT-stubbed |
| `random` | () | num | rand()/RAND_MAX |

## random (11 exports, 3 AOT-stubbed)

`libs/random/native.cpp`, `libs/random/native.h`

| Function | Params | Return | Notes |
|----------|--------|--------|-------|
| `random` | () | num | 0.0..1.0 |
| `uniform` | (a: num, b: num) | num | uniform a..b |
| `randint` | (a: num, b: num) | num | integer a..b |
| `choice` | (arr) | Value | random element |
| `shuffle` | (arr) | nil | in-place |
| `sample` | (arr, k: num) | array | k without replacement |
| `seed` | (val?: num) | nil | set RNG seed |
| `gauss` | (mu: num, sigma: num) | num | Gaussian |
| `expovariate` | (lambd: num) | num | exponential |
| `triangular` | (low, high, mode: num) | num | triangular |
| `getrandbits` | (k: num) | num | k random bits |

## fmt (17 exports, 2 AOT-stubbed)

`libs/fmt/native.cpp`, `libs/fmt/native.h`

| Function | Params | Return |
|----------|--------|--------|
| `to_int` | (val) | num |
| `to_str` | (val) | str |
| `to_float` | (val) | num |
| `to_bin` | (val) | str |
| `to_hex` | (x: num) | str |
| `to_oct` | (x: num) | str |
| `type` | (val) | str |
| `is_int` | (val) | bool |
| `is_float` | (val) | bool |
| `is_string` | (val) | bool |
| `is_bool` | (val) | bool |
| `is_nil` | (val) | bool |
| `is_array` | (val) | bool |
| `is_object` | (val) | bool |
| `is_callable` | (val) | bool |
| `pad_left` | (str, width, pad?) | str |
| `pad_right` | (str, width, pad?) | str |

## strings (25 exports, no AOT stub)

`libs/strings/native.cpp`, `libs/strings/native.h`

split, join, trim, trim_left, trim_right, upper, lower, replace, replace_first,
contains, starts_with, ends_with, index_of, last_index_of, substring, repeat,
reverse, length, count, char_at, char_code, from_char_code, is_empty, is_alpha,
is_digit, is_alnum

## arrays (27 exports, no AOT stub)

`libs/arrays/native.cpp`, `libs/arrays/native.h`

new, length, push, pop, at, set, slice, join, reverse, sort, index_of, contains,
remove, remove_at, clear, clone, to_string, flat, fill, range, shuffle, map,
filter, find, reduce, every, some, flat_map

## json (12 exports, no AOT stub)

`libs/json/native.cpp`, `libs/json/native.h`

stringify, parse, readFile, writeFile, get, set, has, delete, keys, values, merge, clone

## http (15 exports, no AOT stub)

`libs/http/native.cpp`, `libs/http/native.h`

get, post, put, delete, head, patch, request, createServer, listen, startServer,
stopServer, serveHTML, urlEncode, urlDecode, parseQuery

## path (13 exports, 3 AOT-stubbed)

`libs/path/native.cpp`, `libs/path/native.h`

join, split, dirname, basename, extname, isabs, normalize, resolve, relative,
toUnix, toWindows, sep (const), delimiter (const)

## sys (24 exports, no AOT stub)

`libs/sys/native.cpp`, `libs/sys/native.h`

checkpoint, alloc, read, write, append, cp, mv, rm, exists, mkdir, rmdir,
listdir, stat, chmod, tmpfile, cwd, chdir, env, args, info, input, sleep,
exit, exec

## time (3 exports, no AOT stub)

`libs/time/native.cpp`, `libs/time/native.h`

now, format, sleep

## crypto (9 exports, no AOT stub)

`libs/crypto/native.cpp`, `libs/crypto/native.h`

base64_encode, base64_decode, sha256, md5, random_bytes, random_string,
hex_encode, hex_decode, xor_cipher

## process (8 exports, no AOT stub)

`libs/process/native.cpp`, `libs/process/native.h`

spawn, send, receive, self, is_alive, kill, process_count, process_sleep

## regex (8 exports, no AOT stub)

`libs/regex/native.cpp`, `libs/regex/native.h`

test, search, find, findAll, replace, split, isValid, escape

## async (4 exports, no AOT stub)

`libs/async/native.cpp`, `libs/async/native.h`

run, await, sleep, timer

## log (14 exports, no AOT stub)

`libs/log/native.cpp`, `libs/log/native.h`

debug, info, warn, error, set_level, get_level, set_file, set_color, set_timestamp, DEBUG, INFO, WARN, ERROR

## collections (21 exports, no AOT stub)

`libs/collections/native.cpp`, `libs/collections/native.h`

set_new, set_add, set_has, set_remove, set_size, set_to_array, set_union,
set_intersection, set_difference, stack_new, stack_push, stack_pop, stack_peek,
stack_size, stack_is_empty, queue_new, queue_enqueue, queue_dequeue, queue_peek,
queue_size, queue_is_empty
