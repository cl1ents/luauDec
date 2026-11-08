# luauDec

![luauDec logo](luauDec.png)

Decompiler for Luau (https://luau-lang.org/) written by xgladius, updated to Luau 0.745 by [cl1ents](https://github.com/cl1ents) with Claude Opus 5.5

luauDec is a decompiler for the Luau language created by Roblox. It rebuilds a Luau AST from compiled bytecode and prints it as readable, editable source.

Built against Luau **0.745**; reads bytecode versions 3 through 14, including version 13 (the compiler's default target, which adds double-precision vector constants).

# Introduction

The decompiler was designed to have a few things in mind,

***No dependencies*** - Other than the Luau language, luauDec has 0 dependencies other than the C++ STL.

***Easy to understand and use*** - The code is written in a way that anyone can understand it, given they understand C++ of course.

***Speed*** - The decompiler does not invoke the Luau VM, (like some other decompilers), allowing for it to be incredibly fast.

***Multi-platform*** - Works on all platforms.

# Building

1. `git clone --recursive https://github.com/xgladius/luauDec.git`
2. `cd luauDec`
3. `cmake -B build && cmake --build build`
4. Optionally `cmake --install build` (or `--component library` / `--component cli` for just one half)

Set `-DLUAUDEC_BUILD_CLI=OFF` to build only the library, and `-DLUAUDEC_LUAU_SOURCE_DIR=<path>` to build against a Luau checkout other than the submodule.

## Nix

The flake builds against the same Luau release as the submodule, so no `--recursive` checkout is needed.

- `nix run github:cl1ents/luauDec -- script.luau` runs the CLI
- `nix build .#cli` builds `bin/luauDec`
- `nix build .#lib` builds the library: `lib/libluaudec.a`, `include/luauDec/Decompile.h`, plus CMake and pkg-config files
- `nix build` builds both
- `nix develop` opens a shell with CMake, Ninja and a compiler

# Usage

```
luauDec [-O<n>] [-g<n>] <file>
```

`<file>` is either Luau bytecode or Luau source. Source is compiled first (with the given optimization and debug levels), which is handy for seeing what the decompiler makes of a script. Use `-` to read from stdin.

With debug information (`-g2`), local and parameter names come from the bytecode. Without it, names are guessed from how values are produced: `game:GetService("Players")` becomes `Players`, `require(x.Remotes)` becomes `Remotes`, loop variables become `i`/`j`/`k` or `k, v`, and table methods get a `self` parameter.

## As a library

The whole API is one function:

```cpp
#include <luauDec/Decompile.h>

std::string source = Luau::Decompiler::decompile(bytecode); // throws Luau::Decompiler::DecompileError
```

`libluaudec.a` is self-contained: Luau's AST code is built into it, and the header exposes no Luau types. If your program also links its own copy of Luau, use the same Luau version (0.745).

```cmake
# installed (or from the Nix `lib` package):
find_package(luauDec 1 REQUIRED)
target_link_libraries(myTool PRIVATE luauDec::luaudec)

# or vendored:
add_subdirectory(luauDec)
target_link_libraries(myTool PRIVATE luauDec::luaudec)
```

Without CMake: `pkg-config --cflags --libs luaudec`.

## How it works

1. `BytecodeReader` parses the bytecode blob into protos, constants, and optional debug info.
2. `FlowAnalysis` decodes each function's instructions and builds a flow graph. Short-circuit expressions compiled with jumps (`a and f()`, `if c then x else y`, comparisons stored as booleans) are collapsed into single value regions. Reaching definitions then group register writes and reads into webs, one per source-level value or variable.
3. `FunctionBuilder` walks the instructions in order, keeping a symbolic register file:
   - A value read once becomes part of the expression that reads it.
   - A value read several times, captured by a closure, or written on several paths becomes a local.
   - Evaluation order is preserved; when inlining a value would reorder side effects, the value is named first.
   - Jump patterns are recognized as `if`/`elseif`/`else`, `while`, `repeat`, numeric and generic `for`, `break` and `continue`. Chains of conditional jumps are folded back into `and`/`or`/`not` conditions.
4. `Emitter` prints the resulting Luau AST, adding indentation and the parentheses that operator precedence requires.

Anything the decompiler cannot express is left as a `-- luauDec: ...` comment, so the rest of the output stays usable.

## Testing

`ctest --test-dir build` runs `tests/RoundTrip.cpp` over every program in `tests/corpus`. Each program is compiled at -O0/-O1/-O2 × -g0/-g1/-g2 (plus double-precision vectors). The test decompiles the bytecode, recompiles the output, runs both versions, and checks that they print the same thing.

### Known limitations

- At -O2 the compiler inlines small local functions. An inlined `return` from inside a loop becomes a jump past the end of the loop, which has no structured equivalent. luauDec marks it with a comment; see `tests/corpus/25_inlined_early_returns.luau`.
- `continue` inside `repeat ... until` loops is not reconstructed.
- `if ... then return end` chains may come out as consecutive `if` statements instead of `elseif`, because the compiler drops the jumps to the shared end. The behavior is the same.

### Feature list

- [x] StatBlock AST generation
- [x] Basic arithmetic AST generation (+, -, *, /, //, %, ^, .., unary -, not, #)
- [x] Reading and writing from constants table AST generation (numbers, strings, booleans, vectors, imports)
- [x] Call AST generation (print())
  - [x] Arguments (print(1,2,3,4,5,6)
  - [x] Vararg argument (print(...))
  - [x] Method calls (obj:method())
- [x] Function AST generation (Proto->p nested subprotos) (function x() print'a' end x())
  - [x] Newclosure / dupclosure AST generation
  - [x] Upvalues, recursive local functions, `function t.f()` and `function t:m()`
- [x] Return AST generation
  - [x] Normal returns (return 1)
  - [x] Vararg return (return 1,2,3,4)
- [x] Table AST generation
  - [x] Setlist support (x = {1, 2, 3, 4})
  - [x] Newtable support (x = {})
  - [x] Settable support (x[1] = 1)
  - [x] Constructors with fields (x = {a = 1, [k] = 2})
- [x] Local variable AST generation
  - [x] "Smart" local variable names
- [x] Numeric for loops (for i=1, 10 do end)
  - [x] Nested numeric for loops (for i=1, 10 do for x=1, 100 do end end)
- [x] Range based for loops (for i,v in pairs() do end)
  - [x] Nested range based for loops
- [x] if statements
  - [x] if then (if true then end)
  - [x] Nested if statements
  - [x] else
  - [x] elseif
  - [x] Compound conditions (and, or, not)
- [x] while statements
  - [x] Nested while statements
  - [x] break / continue
- [x] repeat ... until statements
- [x] Value expressions (a and b, a or b, if-expressions, comparisons stored in locals)
