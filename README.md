# Ed

**Ed** is a small calculator-style educational language: lexer → parser → bytecode emitter → stack VM. Source files use the **`.ed`** extension; compiled tape bundles use **`.edb`** (magic `EDB1`).

## Requirements

- **g++** (or another C++17 compiler) with **GNU Make** for the optional `Makefile`
- On Windows, a common setup is **MSYS2** / **MinGW-w64** or **Git for Windows** (includes `g++` and `make` in some environments)

## Build

### Using Make

From the repository root:

```bash
make
```

This produces **`ed.exe`** on Windows and **`ed_run`** on Linux and macOS.

```bash
make clean    # remove the built executable
```

### Without Make

```bash
g++ -std=c++17 -O2 -Ied -o ed.exe ed/main.cpp ed/stack_emitter.cpp ed/syntax.cpp ed/source_scan.cpp ed/lexicon.cpp ed/phrase_book.cpp ed/tape_runner.cpp
```

## Run

Always run the executable from the **repository root** (so paths like `ed_examples/...` resolve), unless you pass absolute paths.

### Windows PowerShell

PowerShell does not run programs in the current directory unless you prefix with `.\`:

```powershell
.\ed.exe ed_examples\program.ed
```

### Windows CMD / Git Bash / Unix

```bash
ed.exe ed_examples/program.ed
# or
./ed_run ed_examples/program.ed
```

The sample **`ed_examples/program.ed`** ends with `input(...)`: type a line and press **Enter** when prompted.

### Compile and run bytecode

```powershell
.\ed.exe compile ed_examples\program.ed
.\ed.exe run ed_examples\program.edb
```

Optional output path:

```powershell
.\ed.exe compile ed_examples\program.ed path\to\out.edb
.\ed.exe run path\to\out.edb
```

### Modules (`use`)

At the start of a line (after optional spaces), include another file:

```ed
use "other.ed";
```

Paths are resolved relative to the file that contains the `use` line.

## Language sketch

| Idea | ED syntax |
|------|-----------|
| Line / block comments | `// …` and `/* … */` |
| Print | `emit(...)` |
| If / else | `choose (expr) { … }` and optional `otherwise { … }` |
| While | `during (expr) { … }` |
| For | `span (init; cond; step) { … }` |
| Void routine | `proc name(...) { … }` |
| Function (must end with `give`) | `fn name(...) { … give expr; }` |
| Return | `give expr;` |
| Local / global | `cell x = …`, `sheet g = …` |
| Booleans | `yes`, `no` |
| Empty value | `empty` |
| Constants | `pi`, `euler` |
| And / or / not | `&&`, `||`, `!` |
| Break / continue | `halt`, `skip` |
| Switch-style | `pick (expr) { on value: stmt… on a, b: stmt… fallback: stmt… }` |

Built-ins include math (`sin`, `cos`, `sqrt`, `pow`, …), `input`, `len`, `type`, `ord`, `chr`, and radix helpers (`bin`, `oct`, `hex`, `dec`).

## Layout

| Path | Role |
|------|------|
| `ed/` | C++ implementation (scan, lexicon, phrase book, syntax, emitter, VM, CLI) |
| `ed/sources.txt` | Translation units (for reference / IDEs) |
| `ed_examples/` | Sample `.ed` programs |

## `make run` note

The `Makefile` **`run`** target uses `./$(TARGET)`. That works in **Git Bash**, **MSYS2**, and typical Unix shells. In **PowerShell**, prefer running **`.\ed.exe ed_examples\program.ed`** manually after `make` or `mingw32-make`.
