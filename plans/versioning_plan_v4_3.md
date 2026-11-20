# Code::Blocks v4.3 — active GDB watches on Apple Silicon

**Status: Planning.** v4.2 and v4.2.1 stay the Windows OGRE work and are not continued here. The repair is in the sister repo `binutils-gdb`, file `gdb/aarch64-darwin-nat.c`. Code::Blocks already sends the watch text to GDB and displays the reply. Do not change the Watches window until a direct GDB session evaluates the same expressions.

**Question this version answers:** why does a watch such as `a[0]` or `a.size()` print `Changing the program counter is not supported on AArch64 Darwin`, and what is the smallest change in the companion GDB that makes those expressions evaluate?

The November notes are `plans/thoughts/codeblocks-gdb_watches-thoughts.md`. This plan records what the source shows and where the fix belongs.

---

## 1. What already works

Stopped on a `std::string a`, the Watches window shows:

```text
1+2      3
a        "6.044"
a[0]     Changing the program counter is not supported on AArch64 Darwin.
a.size() the same error
```

`1+2` is computed inside GDB. `a` is read from memory and pretty-printed. `a[0]` and `a.size()` are compiled functions (`operator[]` and `size()`). GDB has to call them in the inferior, then restore the stop.

Code::Blocks asks for that with the GDB command `output <expression>` (`GdbCmd_TooltipEvaluation` in `src/plugins/debuggergdb/gdb_commands.h`). The error text is GDB's message, copied into the watch. The IDE is not evaluating the expression itself.

---

## 2. Where the call stops

An inferior call does this:

```text
call_function_by_hand
  aarch64_push_dummy_call     writes LR, SP, and argument registers
  proceed(function address)
    regcache_write_pc
      regcache::raw_write
        target_store_registers(regcache, AARCH64_PC_REGNUM)
```

`proceed` is in `gdb/infrun.c`. When the address is not “resume where we stopped”, it calls `regcache_write_pc`. That writes one register through `target_store_registers`.

`aarch64_darwin_nat_target::store_registers` then refuses that register:

```text
if (regno == AARCH64_PC_REGNUM)
  error ("Changing the program counter is not supported on AArch64 Darwin.");
```

The comment above the check says a `thread_set_state` of a new PC faults the inferior, so the kernel PC is left unchanged. A store of every register (`regno == -1`) also skips `__pc`. General registers, FP, LR, SP, and CPSR are written. `gdb/testsuite/gdb.darwin/aarch64-regs.exp` requires this refusal: `set $pc = finish_here` must print the error, and the inferior must still exit normally.

x86_64 Darwin does not have this refusal. `i386_darwin_nat_target::store_registers` copies the whole thread state, including `__rip`, and calls `thread_set_state`.

The same port already avoids inferior calls for one other feature. `gdb/inferior-stdio-unbuffered.c` says `call setvbuf(...)` fails because AArch64 Darwin rejects PC stores, and it writes the `FILE` flags in memory instead.

---

## 3. Why a raw PC store is the wrong experiment

On this SDK, `arm_thread_state64_t` does not treat `__pc` as a plain address. `mach/arm/_structs.h` keeps it as `__opaque_pc` and signs it. The supported write is `arm_thread_state64_set_pc_fptr` in `mach/arm/thread_status.h`. It resigns a function pointer with the process-independent code key and the `"pc"` discriminator. A second macro, `arm_thread_state64_set_pc_presigned_fptr`, stores a pointer that is already signed. `arm_thread_state64_get_pc` is the matching read.

The current fetch copies `&regs.__pc` straight into the register cache. The current store never writes that field back. Putting an unsigned GDB address into the opaque field and calling `thread_set_state` is the operation the comment says faults the inferior.

LR has the same shape (`arm_thread_state64_set_lr_fptr`). `aarch64_push_dummy_call` writes LR to the dummy return address before `proceed` writes PC, and that LR store is accepted today as a raw word. Do not change LR until a signed PC store gets past the current error. If the call then faults on return, sign LR the same way. SP and the integer argument registers are already stored; `aarch64-regs.exp` shows a write of `x1` survives a register flush.

Floating-point and SIMD stores are a separate error in the same function: any register at or above `AARCH64_V0_REGNUM` is rejected. `a.size()` and `a[0]` do not need those registers. An expression that passes or returns a floating-point value does. That stays out of this version.

---

## 4. Where to continue

Continue in the sister repo. The first edit is `gdb/aarch64-darwin-nat.c`. A direct `gdb` session must evaluate the expressions before this repository changes.

This repository's part is the acceptance run inside Code::Blocks, then rebuilding the bundled companion GDB the way v1.2 already packages it. Do not decode `std::string` in the IDE, and do not add a second expression evaluator.

---

## 5. Repair

In `store_registers`, a request for `AARCH64_PC_REGNUM` reads `ARM_THREAD_STATE64`, sets the PC with `arm_thread_state64_set_pc_fptr` when the address is a function pointer, and writes the state back with `thread_set_state`. The value GDB holds is the unsigned address it already shows in `print $pc`. The setter signs it for the kernel.

If that setter returns no usable pointer, try `arm_thread_state64_set_pc_presigned_fptr` only when the address is already the value `arm_thread_state64_get_pc` returned. Do not write the raw `__opaque_pc` field.

After the store, fetch the thread state again and check that `arm_thread_state64_get_pc` is the address GDB wrote. Then restore the original PC by the same path. `aarch64-regs.exp` must stop requiring the error. It should set `$pc`, read it back, restore it, and still reach the original exit.

Only then:

```text
print add_one(4)
print box.get()
print a.size()
print a[0]
print a[0] == '-'
```

`add_one` and `box.get()` are the small calls from the November notes. They tell a signing or restore bug from a C++ ABI bug. The `std::string` watches are the beginner case.

---

## 6. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Confirm the failure outside Code::Blocks. | With the sister-repo `gdb`, stopped where `a` is in scope: `print 1+2` and `print a` succeed; `print a[0]` and `print a.size()` print the PC error. | **NEXT** |
| M2 | Store and restore one PC. | `set $pc` uses the signed setter, the fetched PC matches, the original PC is restored, and the inferior still exits. `aarch64-regs.exp` matches that, and `x1` still stores. | |
| M3 | Call a plain function and a const method. | `print add_one(4)` is `5`. `print box.get()` returns the member. The next `print a` is unchanged, and `next` still steps. | |
| M4 | Evaluate the beginner string watches. | `a.size()`, `a[0]`, and `a[0] == '-'` return `6`, `'-'`, and `true` for `a == "-6.044"`. A thrown call or a breakpoint inside the call restores the original stop. | |
| M5 | Show the same watches in Code::Blocks. | The Watches window shows the M4 results. No change to `gdb_commands.h` unless the reply is right in GDB and wrong in the window. | |

---

## 7. Acceptance

At a breakpoint in this program, both a direct GDB session and the Code::Blocks Watches window show:

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

Formatting may differ. The PC error must not appear. After the watches update, step once and the selected line and the value of `a` are unchanged.

---

## 8. Stop

Stop if a signed `thread_set_state` of the PC still faults the inferior. The next fact to record is the kernel return and the thread state, not a second unsigned store.

Stop if M3's `add_one(4)` returns and then `next` or `print a` is wrong. That is a restore bug, not a reason to special-case `std::string` in Code::Blocks.

Stop if the first expression that fails needs a V register. Floating-point stores are a later change, not this version.

Do not start the edit in this repository.
