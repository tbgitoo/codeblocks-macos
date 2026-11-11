## Current ogre-next-deps status

1. SDL2 DLL name
   Status: SUBMITTED (#18)

2. FreeImage / LibJXR / Imath MinGW-w64 compatibility
   Status: SUBMITTED (#19)

   The original local patch deleted LibJXR's bundled guiddef.h. During
   upstream preparation this was replaced by a less invasive solution:
   retain the private header but scope its include directory only to the
   LibJXR source files that require it.

3. Remotery MinGW-w64 compatibility
   Status: SUBMITTED (#20)

   Includes two commits:
   - avoid redefining rsize_t with MinGW-w64
   - link Remotery explicitly with ws2_32 on Windows

4. zziplib MinGW read/write compatibility
   Status: TO INVESTIGATE

   The local compatibility patch adds MinGW-specific wrappers around
   _read/_write because zziplib's plugin callback uses zzip_size_t while
   the MinGW CRT functions take an unsigned int byte count.

   Do not upstream the existing wrapper implementation without first
   reproducing the pristine failure and investigating the vendored
   zziplib code.

   In particular, the local patch clamps requests larger than UINT_MAX:

       if (len > 0xffffffffu)
           len = 0xffffffffu;

   This changes I/O behavior and therefore deserves more scrutiny than
   the previous build-only fixes.

   Investigation sequence:

   1. Branch from current upstream/master.
   2. Configure with the same x86_64 MSYS2 MinGW-w64 toolchain.
   3. Build the zziplib target from pristine upstream.
   4. Record the exact compiler diagnostic.
   5. Identify the expected zzip_plugin_io read/write callback signatures.
   6. Identify the exact MinGW-w64 _read/_write declarations.
   7. Check whether the vendored zziplib already contains portability
      machinery intended to bridge these signatures.
   8. Only then derive the smallest upstreamable fix.

   The objective is to fix the type incompatibility, not necessarily to
   reproduce the existing local wrapper patch verbatim.