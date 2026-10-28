# Thoughts for GDB Watch Expressions on Apple Silicon

## Context

A beginner-facing debugging test in the native Apple Silicon Code::Blocks build exposed a clear limitation in the current AArch64 Darwin GDB port.

The original observation was simple and reasonable:

> Watches are useful because they show variables, but can the debugger also show individual expressions such as `a[0]` or `a[0] == '-'`?

Code::Blocks already supports manually entered watch expressions. However, expressions involving `std::string` member functions or operators fail in the current debugger, while simple expressions and direct display of the object work.

---

## Minimal observed case

Given a stopped program containing a `std::string` such as:

```cpp
std::string a = "6.044";
```

The Code::Blocks Watches window produces the following results:

```text
Expression    Observed result
----------    ---------------------------------------------------------------
1+2           3
a             "6.044"
a[0]          Changing the program counter is not supported on AArch64 Darwin.
a.size()      Changing the program counter is not supported on AArch64 Darwin.
```

The distinction is clean:

- Pure debugger-side expression evaluation works: `1+2` evaluates to `3`.
- Direct inspection and pretty-printing of the `std::string` object works: `a` displays as `"6.044"`.
- Expressions that appear to require executing a C++ member function or overloaded operator in the inferior fail: `a[0]` and `a.size()`.

This strongly suggests that the Watches UI and general expression parser are not the primary problem.

---

## Working hypothesis

The likely boundary is **inferior function calling**.

For a `std::string`, the following expressions may require GDB to call compiled code inside the debugged process:

```cpp
a[0]       // std::string::operator[]
a.size()   // std::string::size()
```

Even if these functions are small or normally inlined, the debugger may resolve the requested expression as a function call in the inferior.

By contrast:

```cpp
1 + 2
```

can be evaluated internally by GDB without running the inferior, and displaying:

```cpp
a
```

can be handled through object inspection and the `std::string` pretty-printer.

Therefore the message about changing the program counter is probably the **first visible failure in the inferior-call path**, not necessarily a complete diagnosis of the missing functionality.

---

## Preliminary analysis

An inferior function call is likely to require more than allowing one write to the program counter. Depending on what is already implemented in this GDB port, the complete operation may involve:

1. Saving the current stopped thread state.
2. Preparing arguments according to the AArch64 calling convention.
3. Setting up registers, stack state, and a return mechanism.
4. Changing the program counter to the called function.
5. Resuming the inferior temporarily.
6. Detecting completion of the temporary call.
7. Retrieving the return value.
8. Restoring the original registers, stack, program counter, thread state, and stop context.
9. Handling signals, exceptions, breakpoints, and multiple threads safely during the temporary execution.

The existing `Changing the program counter is not supported on AArch64 Darwin.` error may merely identify the first explicit guard encountered by machinery that is otherwise partly or mostly present.

The correct approach is to trace the actual call path before deciding the scope of the repair.

---

## First investigation steps

### 1. Reproduce outside Code::Blocks

Use the same GDB binary directly and stop at a line where `a` is in scope. Test:

```gdb
print 1+2
print a
print a[0]
print a.size()
```

Expected current behaviour, based on the Code::Blocks results:

```text
print 1+2       succeeds
print a         succeeds
print a[0]      fails with the AArch64 Darwin PC error
print a.size()  fails with the AArch64 Darwin PC error
```

If reproduced in the GDB console, Code::Blocks is only displaying the underlying GDB result and should not be modified as the first response.

### 2. Locate the exact error source

Search the GDB source tree for the literal text:

```text
Changing the program counter is not supported on AArch64 Darwin.
```

Identify:

- the function emitting the error;
- the caller immediately above it;
- whether the operation is a generic `write_pc`, Darwin thread-state update, dummy-call setup, or another target-specific method;
- whether the restriction was deliberately added during the earlier Apple Silicon porting work;
- whether adjacent AArch64 targets implement the missing operation;
- whether x86_64 Darwin has an equivalent implementation that reveals the intended Darwin control flow.

### 3. Trace the inferior-call path

Run GDB under a debugger or add temporary diagnostics around the error site. Determine whether `print a.size()` reaches machinery associated with concepts such as:

```text
call_function_by_hand
infcall
push_dummy_call
write_pc
resume
restore_registers
```

The exact names vary by GDB version, so source-tree search and the observed call stack should lead the investigation rather than assumptions based on a different release.

### 4. Determine whether the blocked PC write is the only missing operation

A cautious temporary experiment may permit the relevant program-counter update, but only as a diagnostic branch.

The experiment should answer:

- Does the inferior call then complete?
- Does it crash immediately?
- Is the return value correct?
- Is the original stop location restored?
- Does stepping still work afterwards?
- Are registers and local variables unchanged apart from intentional side effects of the called expression?
- Does the correct thread remain selected?

A successful `a.size()` result after enabling the PC update would be encouraging, but it would not by itself prove that arbitrary inferior calls are safe.

---

## Suggested test ladder

Proceed from low-risk expressions to more demanding calls.

### Tier 1: No inferior call expected

```gdb
print 1+2
print x
print x+1
print x==10
```

### Tier 2: Simple user-defined function

```cpp
int add_one(int x)
{
    return x + 1;
}
```

```gdb
print add_one(4)
```

This provides a simpler inferior-call case than the C++ standard library.

### Tier 3: Trivial const member function

```cpp
struct Box
{
    int value;
    int get() const { return value; }
};
```

```gdb
print box.get()
```

### Tier 4: `std::string` accessors

```gdb
print a.size()
print a.empty()
print a[0]
```

### Tier 5: Boolean expressions useful to beginners

```gdb
print a[0] == '-'
print !a.empty() && a[0] == '-'
```

### Tier 6: Side effects and failure handling

Only after read-like expressions work reliably, test calls that modify state or fail:

```gdb
print ++x
print a.clear()
```

Also test a function that throws, hits a breakpoint, receives a signal, or does not return normally. These are not necessary for the first beginner use case, but they reveal whether inferior-call cleanup is robust.

---

## Regression case for Code::Blocks

Create a small standalone test program:

```cpp
#include <string>

int add_one(int x)
{
    return x + 1;
}

int main()
{
    int x = 10;
    std::string a = "-6.044";

    // Break here.
    return 0;
}
```

At the breakpoint, add these watches:

```cpp
1 + 2
x
x + 1
x == 10
add_one(x)
a
a.size()
a[0]
a[0] == '-'
```

The beginner-facing success criterion is straightforward:

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

Exact formatting may differ, but the expressions must evaluate without displaying an unrelated target-backend error.

---

## Product significance

This is not merely an obscure GDB completeness issue.

For the PARENTS/beginner use case, automatic locals answer:

> What values do my variables contain?

Manual expression watches answer:

> Is this exact condition true, and why?

For code such as:

```cpp
if (!a.empty() && a[0] == '-')
```

watching the components separately provides a visible decomposition of the program's reasoning:

```cpp
a.empty()
!a.empty()
a[0]
a[0] == '-'
!a.empty() && a[0] == '-'
```

That is particularly valuable for children learning indexing, conditions, Boolean operators, and control flow. The current backend error breaks precisely the transition from “see a variable” to “understand an expression.”

The user expectation is correct: entering `a[0]` in a Watches window should show the first character, not expose an internal limitation of the Darwin target backend.

---

## Scope boundary for November

The immediate goal should be narrowly defined:

> Determine why inferior function calls fail in the AArch64 Darwin GDB port, and make common read-only watch expressions such as `a.size()`, `a[0]`, and `a[0] == '-'` work reliably in the native Apple Silicon Code::Blocks debugger.

This investigation should **not** automatically expand into:

- redesigning the Code::Blocks Watches UI;
- implementing a new expression evaluator in Code::Blocks;
- manually decoding every C++ standard-library type in the IDE;
- broad debugger refactoring unrelated to inferior calls;
- claiming support for arbitrary side-effecting function calls before the restoration path is tested.

First establish the GDB-side failure and repair the narrow backend capability needed by the observed beginner workflow.

---

## Decision summary

1. The Watches feature already accepts expressions.
2. General expression evaluation works, as demonstrated by `1+2 -> 3`.
3. Direct `std::string` inspection works, as demonstrated by `a -> "6.044"`.
4. `a[0]` and `a.size()` both fail with the same AArch64 Darwin program-counter error.
5. The evidence points towards broken or incomplete inferior function-call support.
6. The PC error is probably an entry point for investigation, not proof of a one-line fix.
7. Reproduce directly in GDB before touching Code::Blocks.
8. Trace the exact error and inferior-call stack.
9. Implement and test the smallest defensible AArch64 Darwin repair.
10. Preserve the concrete PARENTS regression case as the acceptance test.


