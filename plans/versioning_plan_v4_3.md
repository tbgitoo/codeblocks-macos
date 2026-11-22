# Code::Blocks v4.3 — active GDB watches on Apple Silicon

**Status: GDB repair landed.** Sister repo `binutils-gdb` commit `780135ca580` (`program counter fix`, 21 Nov 2026) stores a request for the program counter alone. This repository was not modified. In Code::Blocks, watches that call a function now evaluate. The text in the watch is still GDB's raw `output` reply.

**Question this version answers:** why a watch such as `a[0]` or `a.size()` printed `Changing the program counter is not supported on AArch64 Darwin`, and what change in the companion GDB makes those expressions evaluate.

The November notes are `plans/thoughts/codeblocks-gdb_watches-thoughts.md`. They describe the failure before `780135ca580`.

---

## 1. What failed before the fix

Stopped on a `std::string a`, the Watches window showed:

```text
1+2      3
a        "6.044"
a[0]     Changing the program counter is not supported on AArch64 Darwin.
a.size() the same error
```

`1+2` is computed inside GDB. `a` is read from memory and pretty-printed. `a[0]` and `a.size()` are compiled functions. GDB has to call them in the inferior.

Code::Blocks asks for that with `output <expression>` (`GdbCmd_TooltipEvaluation` in `src/plugins/debuggergdb/gdb_commands.h`) and copies the reply into the watch.

The call reached `proceed(function address)` in `gdb/infrun.c`, then `regcache_write_pc`, then `target_store_registers` for `AARCH64_PC_REGNUM` alone. `aarch64_darwin_nat_target::store_registers` rejected that register. A store of every register (`regno == -1`) skipped `__pc` as well. `gdb/testsuite/gdb.darwin/aarch64-regs.exp` required the refusal.

---

## 2. What landed in the sister repo

Commit `780135ca580` changes the PC-alone path in `gdb/aarch64-darwin-nat.c` and the expectation in `gdb/testsuite/gdb.darwin/aarch64-regs.exp`.

When `store_registers` is asked for `AARCH64_PC_REGNUM` and that register is valid, it reads `ARM_THREAD_STATE64`, passes the address GDB holds to `arm_thread_state64_set_pc_fptr`, and writes the state with `thread_set_state`. It then reads the thread state again. If `arm_thread_state64_get_pc` is not the address GDB wrote, it reports `AArch64 Darwin program counter store did not match the value GDB wrote.` A failed `thread_set_state` reports `Failed to store the AArch64 Darwin program counter.`

The comment on that path records the fact that mattered: this translation is arm64 without `ptrauth_calls`, so `arm_thread_state64_set_pc_fptr` stores the integer address in `__pc`. The earlier plan treated a pointer-authentication signature as the required repair. This build did not need one.

`aarch64-regs.exp` now records `$pc`, sets it to `finish_here`, flushes the register cache, checks the fetched PC, restores the original PC, and still expects main to exit with code `011`. The existing `x1` store check is unchanged.

These parts of the same function were left as they were:

- A store of every register (`regno == -1`) still does not write the kernel PC.
- LR, SP, FP, CPSR, and the integer registers are still written as raw words.
- A register at or above `AARCH64_V0_REGNUM` still errors with `Writing floating-point or SIMD registers is not supported on AArch64 Darwin.`

`gdb/inferior-stdio-unbuffered.c` still describes the old refusal and still updates the `FILE` flags in memory. That workaround was not part of this commit.

---

## 3. What Code::Blocks shows now

After this GDB, the debugger in Code::Blocks evaluates the expressions that used to stop on the program-counter error. The watch shows GDB's raw `output` text. It does not yet show the beginner formatting in the acceptance list below: a quoted character, a bare size, and `true` or `false`.

Arithmetic watches and the pretty-printed `std::string` already worked and were not part of this change. `gdb_commands.h` was not edited.

---

## 4. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Name the failure. | `a[0]` and `a.size()` printed the program-counter error. `1+2` and pretty-printed `a` did not. | **DONE** |
| M2 | Store and restore one PC in the sister repo. | `780135ca580` writes `AARCH64_PC_REGNUM` through `arm_thread_state64_set_pc_fptr`, reads it back, and `aarch64-regs.exp` sets `$pc`, checks it, and restores it. | **DONE** |
| M3 | Evaluate a call from Code::Blocks. | Watches that call a function evaluate. The window shows the raw GDB reply. | **DONE** (raw) |
| M4 | Show the beginner string results. | `a.size()`, `a[0]`, and `a[0] == '-'` display as `6`, `'-'`, and `true` for `a == "-6.044"`. | **OPEN** |

---

## 5. Acceptance

At a breakpoint in this program, the Watches window evaluates every row. The open gap is the wording of the last three rows.

```text
1 + 2         3
x             10
x + 1         11
x == 10       true
add_one(x)    11
a             "-6.044"
a.size()      6
a[0]          '-'
a[0] == '-'   true
```

```cpp
#include <string>
int add_one(int x) { return x + 1; }
int main()
{
    int x = 10;
    std::string a = "-6.044";
    return 0; /* break here */
}
```

The program-counter error is gone. After the watches update, one step leaves the selected line and the value of `a` unchanged.

---

## 6. Left as it is

A full-register store still leaves the kernel PC untouched. Floating-point and SIMD stores still fail. An expression that needs a V register is outside this version.

This repository does not grow its own expression evaluator. The remaining display work is how the raw GDB reply is shown.
