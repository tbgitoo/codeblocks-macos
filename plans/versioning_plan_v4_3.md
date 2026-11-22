# Code::Blocks v4.3 — active GDB watches on Apple Silicon

**Status: M4 done. M5 next.** The companion GDB already evaluates the calls. No Code::Blocks source has yet been modified for v4.3. The remaining change is the Watches display of one GDB reference reply.

**Question this version answers:** why did active GDB watches such as `a[0]` and `a.size()` fail on Apple Silicon, and what minimal changes make those expressions both evaluate and display useful values in Code::Blocks?

Two repairs, kept separate:

**A. Companion GDB** (`binutils-gdb` `780135ca580`, `program counter fix`, 21 Nov 2026). A single-register program-counter store lets GDB make the inferior call. That repair is landed and tested. Do not reopen it.

**B. Code::Blocks.** When GDB returns a decorated reference value, the Watches window shows the payload. It does not show the `(type &) @address:` decoration. This is M5. It is a parser change only.

The November notes are `plans/thoughts/codeblocks-gdb_watches-thoughts.md`. They describe the failure before `780135ca580`.

```text
Original failure
    -> GDB PC-store repair (A, done)
    -> function watches evaluate (done)
    -> M4 identifies the GDB reference decoration (done)
    -> M5 strips only that decoration in ParseGDBWatchValue (next)
    -> M6 final acceptance
```

---

## 1. Original failure

Stopped on a `std::string a`, the Watches window showed:

```text
1+2      3
a        "6.044"
a[0]     Changing the program counter is not supported on AArch64 Darwin.
a.size() the same error
```

`1+2` is computed inside GDB. `a` could already be displayed as `"-6.044"`. `a[0]` and `a.size()` are compiled functions. GDB has to call them in the inferior.

The Watches window does not use `GdbCmd_TooltipEvaluation`. That command is the hover tip. A user watch goes through `DebuggerGDB::UpdateWatches` → `GDB_driver::UpdateWatches` → `GdbCmd_FindWatchType` → `GdbCmd_Watch`. The value command is `output <expression>`.

The call reached `proceed(function address)` in `gdb/infrun.c`, then `regcache_write_pc`, then `target_store_registers` for `AARCH64_PC_REGNUM` alone. `aarch64_darwin_nat_target::store_registers` rejected that register. A store of every register (`regno == -1`) skipped `__pc` as well. `gdb/testsuite/gdb.darwin/aarch64-regs.exp` required the refusal.

---

## 2. Companion GDB repair (done)

Commit `780135ca580` changes the PC-alone path in `gdb/aarch64-darwin-nat.c` and the expectation in `gdb/testsuite/gdb.darwin/aarch64-regs.exp`.

When `store_registers` is asked for `AARCH64_PC_REGNUM` and that register is valid, it reads `ARM_THREAD_STATE64`, passes the address GDB holds to `arm_thread_state64_set_pc_fptr`, and writes the state with `thread_set_state`. It then reads the thread state again. If `arm_thread_state64_get_pc` is not the address GDB wrote, it reports `AArch64 Darwin program counter store did not match the value GDB wrote.`

This translation is arm64 without `ptrauth_calls`, so the setter stores the integer address in `__pc`. The relevant Darwin regression run was 85 expected passes and 0 failures. Direct GDB then showed:

```text
print add_one(4)    5
print b.get()       3
print a.size()      6
print a[0]          45 '-'
print a[0] == '-'   true
```

for `std::string a = "-6.044"`. The inferior stayed healthy and the original PC, LR, and SP were restored. The repaired GDB is staged into Code::Blocks and function-call watches evaluate there. It is built `--without-python`. Python and libc++ pretty-printers are not part of either repair.

Left in that GDB function, and outside v4.3: a full-register store still skips the kernel PC; floating-point and SIMD stores still fail.

---

## 3. Why the value cell is still raw (M4, done)

GDB generates the decorated reference. Code::Blocks then stores that entire reply as the watch value.

For one user watch `a[0]`, the path is:

```text
DebuggerGDB::UpdateWatches
    -> GDB_driver::UpdateWatches
    -> GdbCmd_FindWatchType          whatis a[0]
    -> GdbCmd_Watch                  output a[0]
    -> ParseGDBWatchValue
    -> TryNativeWatchSummary
    -> WatchesDlg::UpdateWatch
```

`GdbCmd_FindWatchType` and `GdbCmd_Watch` are in `src/plugins/debuggergdb/gdb_commands.h`. No `print`, `ptype`, format flag, address command, or child command is added. `whatis` retries with `whatis &` only when the reply is exactly `Attempt to take contents of a non-pointer value.`

The successful `output` reply is:

```text
(const std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::value_type &) @0x16fdfd450: 45 '-'
```

The useful payload is `45 '-'`. The libc++ name is already in that GDB line. `print` is the same line with `$1 = ` in front.

`whatis a[0]` can reply `Attempt to take address of value not located in memory.` That sentence has no `=`, so `AfterFirst('=')` stores an empty type. The type field is not the visible libc++ text. Do not special-case this `whatis` error.

`ParseGDBWatchValue` (`src/plugins/debuggergdb/parsewatchvalue.cpp`) looks for a `{...}` aggregate. This reply is not one, so the non-aggregate path stores the complete line:

```text
watch->SetValue(value);
watch->RemoveChildren();
```

`TryNativeWatchSummary` leaves a watch alone when it already has a value. `WatchesDlg::UpdateWatch` (`src/src/watchesdlg.cpp`) displays that value as the cell.

---

## 4. M5 — strip the reference decoration

**Source.** `ParseGDBWatchValue` in `src/plugins/debuggergdb/parsewatchvalue.cpp`, on the trimmed reply, before the existing non-aggregate fallback stores the complete line.

**Rule.** Recognize one complete GDB reference prefix. The type text is arbitrary. Do not parse it. Do not split on the last colon. A type may contain `::`, and a payload may contain a colon. The libc++ `a[0]` line is the motivating example, not a pattern to hard-code. The structure is only:

```text
(<arbitrary type text>) @0x<hex-address>: <payload>
```

Apply this to the trimmed reply before the existing search for `{`. A reply whose payload is itself `{...}` already takes the aggregate branch (`parsewatchvalue.cpp`: first `{` and a final `}`), and the text before `{` is kept as the parent value. Stripping only inside the non-aggregate branch would leave that decoration in place.

1. Work with the trimmed reply.
2. It must begin with `(`.
3. Find one boundary of the form `) @0x<hex-address>:`.
4. `<hex-address>` is one or more digits from `0-9`, `a-f`, and `A-F` only.
5. The colon immediately after that address ends the decoration.
6. Everything after that colon is the payload.
7. Remove only the separator whitespace before the payload.
8. Parse that payload with the existing function. A scalar stays a scalar. A `{...}` payload uses the existing aggregate path.
9. If that whole structure is absent, leave the reply on the existing path, unchanged.

Do not strip because a string merely contains `@0x`, a colon, or parentheses. The complete structure must match. Nothing in the match depends on `std::__1`, `basic_string`, `value_type`, libc++, libstdc++, Darwin, or Apple Silicon.

The motivating macOS reply becomes `45 '-'`. Leave the `45`. A later UI choice could show only `'-'`. That is not M5.

The aggregate `{...}` path, once the decoration is gone, is unchanged. `TryNativeWatchSummary` and `WatchesDlg` stay unchanged unless a test shows this parser-only change cannot produce the cell text.

### Cross-platform constraint

`ParseGDBWatchValue` is shared debugger code. The reference-payload rule is validated GDB output syntax. It is not a Darwin, Apple Silicon, libc++, `std::__1`, or standard-library rule.

Do not add an `__APPLE__`, `_WIN32`, Darwin, MinGW, libc++, or libstdc++ conditional for this transformation.

The inferior-call failure was AArch64 Darwin-specific. This display rule is generic GDB UI handling. The same textual construct produces the same parse on every host. If MinGW GDB, or any other GDB, returns the same validated form, Code::Blocks normalizes it the same way. A different form stays unchanged until that exact reply is recorded and specified on its own.

### M5 must not

- Change GDB commands, or replace `output` with `print`.
- Modify `GdbCmd_FindWatchType`, or special-case the `whatis` error.
- Parse the libc++ type.
- Add an expression evaluator, Python, or a libc++ pretty-printer.
- Modify `binutils-gdb` or GDB.
- Change aggregate parsing of a payload that is already a `{...}` value.
- Modify `TryNativeWatchSummary` or `WatchesDlg` unless a test shows the parser-only change cannot work.
- Remove the numeric form of a character. `45 '-'` stays `45 '-'`.
- Add a platform preprocessor branch, or detect libc++ versus libstdc++, Darwin versus Windows, or the target architecture.
- Special-case `std::__1` or `std::__cxx11`.
- Alter a reply that does not match the complete validated reference syntax.

---

## 5. M5 parser checks

Add the cases to the existing `SUITE(GDBWatchParser)` in `src/plugins/debuggergdb/debuggergdb_test_parser.cpp`. That suite already calls `ParseGDBWatchValue` and checks `WatchToString` (`symbol=value`, plus children). Do not add a new test framework.

Each new test follows `TEST(Simple)`: one `GDBWatch`, `CHECK(ParseGDBWatchValue(...))`, then `CHECK_EQUAL` on `*w`.

| Case | Input | Expected value |
|---|---|---|
| 1. Generic reference (primary) | `(const int &) @0x1234: 10` | `10` |
| 2. Generic C++ reference, aggregate payload | `(Foo &) @0xABCDEF: {value = 7}` | existing aggregate of the payload, not the literal `{value = 7}` and not the `(Foo &)` decoration. Same `WatchToString` shape as `TEST(SimpleMembers)`: for a watch named `w`, `w= {value=7}` |
| 3. libc++ reference (motivating example) | `(const std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::value_type &) @0x16fdfd450: 45 '-'` | `45 '-'` |
| 4. Ordinary scalar | `10` | `10` |
| 5. Boolean scalar | `true` | `true` |
| 6. Text with a colon, not a reference | `error at main.cpp:42` | unchanged |
| 7. Bad address | `(const int &) @not-an-address: 10` | unchanged |
| 8. Bad hex | `(const int &) @0x12ZZ: 10` | unchanged |
| 9. Incomplete prefix | `(const int &) @0x1234` | unchanged |
| 10. Existing aggregate | `{\n  a = 5, \n  b = 7, \n  c = 1, \n  d = 2 \n}` | unchanged: `cmp= {a=5,b=7,c=1,d=2}` as in `TEST(SimpleMembers)` |

Case 1 is the primary generic case. Case 2 exists to show the match does not depend on `std::__1`. After the decoration is removed, `{value = 7}` is the same kind of aggregate `TEST(SimpleMembers)` already parses (`a = 5` becomes child `a=5`, parent value empty). Do not store the braces as a scalar, and do not invent a second aggregate grammar. Do not add a Windows case whose type is `std::__cxx11` unless a real MinGW reply has been recorded.

Case 10 is the current `TEST(SimpleMembers)` result. Re-run that test. Keep the other tests in `SUITE(GDBWatchParser)` passing, including the existing non-aggregate `0x60d088 "test_test_test2"`.

### Windows / MinGW non-regression

The parser change does not need Windows-specific behavior to be correct. The existing `SUITE(GDBWatchParser)` tests must still pass. The new reference tests are shared, so they cover the macOS and Windows builds of this parser.

A live Windows smoke test is not a blocker for the Apple Silicon repair. Do not create a Windows debugger fixture for v4.3. Do not pull in OGRE, OGRE-Next, the Windows wizard, or the installer. If a Code::Blocks + MinGW GDB setup is already available during M6, smoke-test ordinary watches that the fixture already has: an integer or other scalar, a boolean, a pointer if one is already there, and a string if one is already there.

### A different MinGW reply

If MinGW GDB later shows a different reference syntax, do not widen M5 by guessing another grammar. Record the actual reply first. A second syntax is a separate, evidence-based change. M5 stays:

```text
(<type>) @0x<hex-address>: <payload>
```

---

## 6. M5 runtime check

After the parser checks, use the bundled repaired GDB and the Watches path in section 3.

`a[0]` before M5:

```text
(const std::__1::basic_string<...>::value_type &) @0x...: 45 '-'
```

`a[0]` after M5:

```text
45 '-'
```

On the same stop, these cells stay useful. The parser change must not damage ordinary scalars, booleans, the `a` display, or function-call watches:

```text
1 + 2         3
x             10
x + 1         11
x == 10       true
add_one(x)    11
a             "-6.044"
a.size()      6
a[0]          45 '-'
a[0] == '-'   true
```

---

## 7. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Name the original PC-store failure. | `a[0]` and `a.size()` printed `Changing the program counter is not supported on AArch64 Darwin.` `1+2` and `a` did not. | **DONE** |
| M2 | Repair the PC store in the companion GDB. | `780135ca580` writes one PC through `arm_thread_state64_set_pc_fptr`. Direct calls return `5`, `3`, `6`, `45 '-'`, and `true`. Darwin run: 85 passes, 0 failures. | **DONE** |
| M3 | Evaluate an active call from Code::Blocks. | The staged GDB evaluates function watches in the IDE. | **DONE** |
| M4 | Localize the raw reference notation. | `output a[0]` itself returns `(type &) @0x...: 45 '-'`. `ParseGDBWatchValue` stores that non-aggregate reply unchanged. The libc++ text originates in GDB. Code::Blocks keeps the decoration. Python is not required for the scalar payload. | **DONE** |
| M5 | Strip a validated GDB reference decoration. | `ParseGDBWatchValue` stores `45 '-'` for the section 3 reply. Section 5 cases pass. The section 6 cells match. | **NEXT** |
| M6 | Final v4.3 acceptance. | Section 8. No further implementation. | pending |

---

## 8. M6 — final acceptance

Not an implementation milestone. After M5:

1. Every acceptance watch evaluates.
2. The visible results are:

```text
1 + 2         3
x             10
x + 1         11
x == 10       true
add_one(x)    11
a             "-6.044"
a.size()      6
a[0]          45 '-'
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

3. The program-counter error does not appear.
4. The `a[0]` value cell does not contain `std::__1::basic_string<...> @0x...:`.
5. Refresh the watches.
6. Step once.
7. Refresh the watches again.
8. Debugging still steps, and the watched values stay correct.
9. The bundled GDB still has its debugger signing and entitlement.

Those Apple Silicon checks are the required v4.3 acceptance. This is the defect being repaired.

### Cross-platform non-regression

Required on every host that runs the parser tests:

- All of `SUITE(GDBWatchParser)` passes.
- All of the M5 reference cases in section 5 pass.
- The Apple Silicon Watches acceptance above passes.

Only when a suitable Windows / MinGW GDB setup is already available:

- Run a short Watches smoke test on ordinary scalar and other watches that fixture already has.
- If that GDB emits the validated reference form, the cell shows the payload.
- If it emits a different form, record the reply. Do not change the parser during M6.

Not required: OGRE, OGRE-Next, the Windows wizard, the Windows installer, a new Windows project, or a look through MinGW internals.

---

## 9. Outside v4.3

Do not investigate these in M5 or M6:

- Floating-point or SIMD inferior calls, and V-register stores.
- arm64e and pointer authentication.
- A full-register program-counter store.
- Pretty-printer infrastructure.
- Unrelated debugger or watch bugs.
- OGRE, OGRE-Next, the Windows wizard, and the Windows installer.
- A guessed MinGW reference grammar. Record an actual reply before any second syntax.

No second expression evaluator. No general STL pretty-printing project. Python stays off.
