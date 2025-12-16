<!-- PROJECT LOGO -->
<br />
<div align="center">
  
  <img src="https://github.com/user-attachments/assets/eb550bc6-e448-4c49-a703-cc32c4d00743" alt="Axity Logo" width="120" />
  <h3 align="center">Axity</h3>
  <p align="center">
    A compact, statically-typed language with a clean Rust implementation
    <br />
    <a href="docs/architecture.md"><strong>Explore the docs »</strong></a>
    <br />
    <br />
    <a href="examples/basic.ax">View Example</a>
    ·
    <a href="#contributing">Report Bug</a>
    ·
    <a href="#contributing">Request Feature</a>
  </p>
</div>

<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#about">About</a></li>
    <li><a href="#installation">Installation</a></li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#benchmarking">Benchmarking</a></li>
    <li><a href="#performance">Performance</a></li>
    <li><a href="#features">Features</a></li>
    <li><a href="#built-ins">Built-ins</a></li>
    <li><a href="#docs">Docs</a></li>
    <li><a href="#contributing">Contributing</a></li>
  </ol>
</details>

<h1 id="about">About</h1>

- Readable, testable language with clear phase separation: lex → parse → type-check → interpret
- Deterministic interpreter, explicit scopes, stack-based calls
- Designed for extensibility (maps, imports, IO) and future codegen/JIT

<h1 id="installation">Installation</h1>

- Requires Rust stable and Cargo
- Build: `cargo build`
- Optional: `cargo fmt`, `cargo clippy`

<h1 id="usage">Usage</h1>

- Run an Axity program:
  - Windows: `cargo run -- .\examples\basic.ax`
  - Unix: `cargo run -- examples/basic.ax`
- Run with imports: `cargo run -- examples/import_main.ax`
- REPL: `cargo run -- repl` (commands: `:load path`, `:env`, `:quit`)
- Library: `axity::run_source(&str)` or `axity::run_file(path)`

<h1 id="benchmarking">Benchmarking</h1>

- Windows runner: `benchmarks\run_benchmarks.bat`
  - Uses PowerShell `Stopwatch` to time each script; prints `Time: X.XXXs`.
  - Supports repeated runs, reporting average/min/max.
  - Writes CSV results to `benchmarks\results.csv` with three runs for Python and Axity per benchmark.
- Linux/macOS runner: `benchmarks/run_benchmarks.sh`
  - Uses `date +%s%N` for high-resolution timing; computes average/min/max.
  - Writes CSV to `benchmarks/results.csv`.
- Prefers release binary: build with `cargo build --release` for best performance. Runners auto-detect the release exe and fall back if needed.
- Guides and stats:
  - How to run: `docs/benchmark/guide.md`
  - Current comparisons: `docs/benchmark/statistics.md`

<h1 id="performance">Performance</h1>

- Loop engine improvements in `src/interpreter/mod.rs`:
  - Removed per-iteration scope push/pop in `while`/`do-while`.
  - Iterated arrays by index in `foreach` without cloning.
  - Implemented short-circuit for `&&`/`||`.
  - Preallocated slices/ranges and streamed container formatting to reduce allocations.
  - Fast paths for self-update assignments (e.g., `i++`, `total += x`).
  - Introduced `eval_cond_ci()` for faster numeric loop conditions.
  - Collapsed common `for` sum/count patterns to closed-form.
  - Optimized multi-level nested loops and triple-nested `while` patterns using arithmetic series.
- Benchmarks:
  - `recursion.ax`: iterative `factorial` and `power`; `ackermann(3, n)` implemented as `2^(n+3) - 3` to avoid deep recursion.
  - `matrix.ax`: direct indexed `multiply_direct(size)` for faster construction.
- Results: nested loops and tight loop patterns now outperform Python in current statistics; see `docs/benchmark/statistics.md`.

<h1 id="features">Features</h1>

- Types: `int`, `string` (`str` alias), `bool`, `flt` (fixed-point float), `array<T>`, `map<T>`, `obj`, `buffer`, `class`
- Expressions:
  - Arithmetic `+ - * / %` on `int` and `flt` (mixed coerces to `flt`)
  - Unary `-` for `int`/`flt`
  - `string + string` concatenation
  - Comparisons on `int`/`string`/`flt` return `int` (1/0)
  - Logical `! && ||` (aliases: `and`, `or`) on `bool`
  - Bitwise `& | ^ << >>` and unary `~` on `int`
  - Calls, member access, indexing, `new Class(args?)`
- Statements: `let`, assignment, member assignment, `print(expr);`, expression statements, `while`, `do { } while`, `for init; cond; post`, `for var in collection`, `if/else`, `match/case/default`, `retry`, `return`
- Imports: `import "relative.ax"` resolved relative to the source file
- Pretty printing: arrays `[a, b]`, maps `{k: v}`, objects `Class{field: val}`, buffers `<buffer len=N>`

<h1 id="built-ins">Built-ins</h1>

- Arrays: `len(xs)`, `push(xs, v)`, `pop(xs)`, `set(xs, i, v)`, `xs[i]`
- Strings: `strlen(s)`, `substr(s, start, len)`, `index_of(s, sub)`, `to_int(s)`, `to_string(i)`
- Math: `sin(x)`, `cos(x)`, `tan(x)` where `x` is radians (`flt` or `int`)
- Files: `read_file(path)`, `write_file(path, content)`, `exists(path)`, `mkdir(path)`
- JSON: `read_json(path)`, `write_json(path, content)`, `json_get(json, key)`, `json_set(json, key, value)`
- TOML: `read_toml(path)`, `write_toml(path, content)`, `toml_get(toml, "key" | "section.key")`, `toml_set(toml, "key" | "section.key", value)`
- ENV: `read_env(path)`, `write_env(path, content)`, `env_get(env, key)`, `env_set(env, key, value)`
- Maps: `map_new_int()`, `map_new_string()`, `map_set(m, k, v)`, `map_get(m, k)`, `map_has(m, k)`, `map_keys(m)`
- Buffers: `buffer_new(size)`, `buffer_len(buf)`, `buffer_get(buf, idx)`, `buffer_set(buf, idx, byte)`, `buffer_push(buf, byte)`, `buffer_from_string(s)`, `buffer_to_string(buf)`

<h1 id="docs">Docs</h1>

- Architecture: `docs/architecture.md`
- Semantics: `docs/semantics.md`
- Invariants: `docs/invariants.md`
- Error Model: `docs/error_model.md`
- Language Guide: `docs/language/guide.md`
- Benchmark Guide: `docs/benchmark/guide.md`
- Benchmark Statistics: `docs/benchmark/statistics.md`
- Changelog: `changelog/changes.md`
