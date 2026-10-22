# Arrow Up step crash, 2026-10-21 17:25

Copied from `~/Library/Logs/DiagnosticReports/` on 2026-10-21. The reports are the originals:

- [gdb-2026-10-21-172539.ips](gdb-2026-10-21-172539.ips)
- [test4-2026-10-21-172539.000.ips](test4-2026-10-21-172539.000.ips)

The prompt that started this was: stepping in the event loop after focusing the SFML window and pressing Arrow Up crashed. The two reports are one event, two milliseconds apart.

## What the reports show

**CONFIRMED FROM LOCAL MACHINE.** GDB and `test4` had been running since 17:09:26. Both died at 17:25:35. GDB’s parent was Code::Blocks pid 83293. The GDB binary was `/Applications/CodeBlocks.app/Contents/Resources/debugger/bin/gdb`.

GDB pid 83426, `SIGABRT` (`abort()` from an internal error). Faulting thread is the main thread. The stack is:

1. `gdb_do_one_event` (`event-loop.cc:220`)
2. `fetch_inferior_event` (`infrun.c:4721`)
3. `do_target_wait` / `target_wait`
4. `darwin_nat_target::wait` (`darwin-nat.c:1420` in that binary)
5. `darwin_nat_target::wait_1` (`darwin-nat.c:1365`)
6. `darwin_nat_target::check_new_threads` (`darwin-nat.c:370`)
7. `delete_thread_1` (`thread.c:524`)
8. `internal_error_loc` → `dump_core` → `abort`

`thread.c:524` in this GDB is `gdb_assert (thr != nullptr)`.

`test4` pid 83428, `EXC_BREAKPOINT` / `SIGTRAP`, PC `0x100000980`. The only frames are `main` at `main.cpp:28` and `start`.

The process image in the report is the redacted path `/Users/USER/Documents/*/test4`. That project is `~/Documents/Mathis/Test Videogame/test4/`. A copy of the source, the project file, and the bitmap the source loads is in [test4/](test4/):

- [test4/main.cpp](test4/main.cpp) — line 28 is `while (const std::optional event = app.pollEvent())`
- [test4/test4.cbp](test4/test4.cbp) — Debug target, clang, C++17, Homebrew SFML include and lib
- [test4/cb.bmp](test4/cb.bmp) — `loadFromFile("cb.bmp")` returns before the event loop if this file is missing

## What that sequence is

**CONFIRMED FROM SOURCE** in the parent of `a8a35dc1a76` (`3c50925576a`, “avoid beachball stop”). The installed binary’s line numbers are not that commit’s line numbers. The symbols are the same functions. In that parent, the remove branch was:

```c
/* A thread was removed.  */
struct thread_info *thr
  = this->find_thread (ptid_t (inf->pid, 0, old_id));
delete_thread (thr);
```

`process_stratum_target::find_thread` uses `inferior::find_thread`, which looks only in `ptid_thread_map`. That map holds live threads. The lookup returned null, and `delete_thread` aborted.

`wait_1` does this after it has a Mach exception to report, not while decoding that exception. It suspends every inferior with `task_suspend`, then calls `check_new_threads`. The call inside `decode_exception_message` is a different one and is not the frame in the report. GDB had resumed the program for the step. The abort is the reconcile at the end of the wait that followed that resume.

The step breakpoint was still planted. With GDB gone, `test4` executed it and died with `SIGTRAP` at `pollEvent`.

## Why Arrow Up may have led there

The sample creates no threads. `pollEvent` drains Cocoa. A key delivered to a focused window presumably makes AppKit create and destroy short-lived Mach threads. This Darwin target records every port from `task_threads` as a GDB thread, with ptid `(pid, 0, port)`.

The fast path in `check_new_threads` returns when the port names still match. It never looks at `ptid_thread_map`. Upstream Darwin does not report `TARGET_WAITKIND_THREAD_EXITED`. `darwin_nat_target::thread_alive` always returns true, so infrun’s “thread vanished” path does not repair the Darwin list. The two lists meet only in `check_new_threads`, and only the remove branch calls `find_thread`.

Arrow Up is a suspected trigger, not the identified defect. It may have supplied workload during pollEvent that changed the Mach thread population, but this was not directly demonstrated. The observed failure occurred at the next stop, when the remove path encountered a port still present in darwin_inferior::threads that was absent from the current live-thread map and no longer returned by the kernel. find_thread consequently returned null. The defect was the remove path's failure to tolerate that stale-thread condition. The trigger is nondeterministic: this occurred during one of many stepping trials, while nearly all equivalent trials completed normally.

While the debugger is already stopped, `wait_1` has suspended the task, so AppKit cannot dequeue a key until the next resume. The press and the abort are separated by that resume and the following reconcile.

## What the reports do not show

The port number is not in the report. Neither is the earlier call that dropped the thread from `ptid_thread_map`, nor whether the `thread_info` still existed in the exited state. The selftest `darwin_delayed_thread` builds that shape directly. It does not replay AppKit.

The fix on `a8a35dc1a76` looks the removed port up in the inferior’s thread list, including exited threads. If none matches, the port is dropped and GDB does not abort.
