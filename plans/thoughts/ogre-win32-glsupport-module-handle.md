OGRE - MinGW module handle in OgreWin32GLSupport.cpp

STATUS

Reproducer identified, not yet run.

This is a separate follow-up to OGRE PR #3823. Start with OgreWin32GLSupport.cpp only. OgreWin32Window.cpp can be tested separately afterward.

TEST ENVIRONMENT

Use the same MSYS2 MinGW64 environment used for the previous OGRE config-dialog test.

Run from Command Prompt:

set "MSYS_MINGW=D:\msys64\mingw64" 
set "PATH=%MSYS_MINGW%\bin;D:\Program Files\CMake\bin;%SystemRoot%\system32;%SystemRoot%"

Verify the toolchain:

where gcc 
where g++ 
where mingw32-make 
where windres

gcc --version gcc -dumpmachine

Expected compiler:

GCC 15.2.0 target x86_64-w64-mingw32

PROBLEM

OgreWin32GLSupport.cpp contains a MinGW-specific lookup:

GetModuleHandle("RenderSystem_GL3Plus.dll")

This lookup is used while creating the temporary Windows window class OgreWglDummy during OpenGL/WGL initialization.

OgreWin32GLSupport.cpp is compiled into the static helper library OgreGLSupport.

That helper library is linked into several render-system DLLs:

RenderSystem_GL.dll RenderSystem_GL3Plus.dll RenderSystem_GLES2.dll

Therefore this code is not necessarily running from RenderSystem_GL3Plus.dll.

For example, when RenderSystem_GL.dll is loaded, this copy of OgreWin32GLSupport.cpp is inside RenderSystem_GL.dll, but the MinGW code still asks Windows for:

GetModuleHandle("RenderSystem_GL3Plus.dll")

The hard-coded DLL name is therefore not generally correct.

The mismatch may be latent rather than producing a visible application failure because the returned handle is used only for the temporary OgreWglDummy window.

REPRODUCER

The reproducer deliberately builds the legacy OpenGL renderer but not GL3Plus or GLES2.

This creates a particularly clean test:

RenderSystem_GL.dll exists. RenderSystem_GL3Plus.dll does not exist.

Configure from the OGRE repository root:

cmake -S . -B build-glsupport ^
 -G "MinGW Makefiles" ^
 -DCMAKE_BUILD_TYPE=Release ^
 -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
 -DCMAKE_C_COMPILER=gcc.exe ^
 -DCMAKE_CXX_COMPILER=g++.exe ^
 -DCMAKE_RC_COMPILER=windres.exe ^
 -DOGRE_BUILD_RENDERSYSTEM_GL3PLUS=OFF ^
 -DOGRE_BUILD_RENDERSYSTEM_GLES2=OFF ^
 -DOGRE_BUILD_SAMPLES=OFF ^
 -DOGRE_BUILD_TESTS=OFF ^
 -DOGRE_BUILD_TOOLS=OFF

Build the legacy OpenGL renderer:

cmake --build build-glsupport --target RenderSystem_GL --parallel 4

Verify that the result contains:

build-glsupport\bin\RenderSystem_GL.dll

and does not contain:

build-glsupport\bin\RenderSystem_GL3Plus.dll

TEMPORARY TEST HOST

Create a small temporary host outside the tracked source tree, for example:

build-glsupport\glsupport-ab\

The host should construct Ogre::Root and load only:

build-glsupport\bin\RenderSystem_GL.dll

Do not load RenderSystem_GL3Plus.dll.

Loading RenderSystem_GL.dll reaches:

Win32GLSupport::initialiseWGL()

A/B CHECK

Temporarily add diagnostics inside Win32GLSupport::initialiseWGL().

The diagnostic code is only for the experiment. It must not remain in the proposed patch.

A. CURRENT HARD-CODED LOOKUP

Current MinGW code:

GetModuleHandle("RenderSystem_GL3Plus.dll")

Use GetModuleFileName() or an equivalent temporary diagnostic to inspect the returned handle.

Because RenderSystem_GL3Plus.dll was deliberately not built or loaded, the expected result is:

GetModuleHandle("RenderSystem_GL3Plus.dll") -> NULL

At the same time, the code being executed is inside:

RenderSystem_GL.dll

B. ADDRESS-BASED LOOKUP

Test:

static const TCHAR staticVar = 0;

GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &staticVar, &hinst);

Use GetModuleFileName() temporarily to inspect hinst.

Expected result:

GetModuleHandleEx(address in OgreWin32GLSupport.cpp) -> RenderSystem_GL.dll

EXPECTED RESULT

The experiment should establish:

Current hard-coded lookup:

RenderSystem_GL3Plus.dll -> NULL

Address-based lookup:

address in OgreWin32GLSupport.cpp -> RenderSystem_GL.dll

This directly demonstrates the problem.

The code is executing from RenderSystem_GL.dll, but the MinGW-specific lookup assumes that the code belongs to RenderSystem_GL3Plus.dll.

A visible crash or rendering failure is not required. The purpose of the test is to determine whether the hard-coded DLL name identifies the module actually containing the code.

PROPOSED PATCH

Only after the reproducer confirms the expected result, modify:

RenderSystems/GLSupport/src/win32/OgreWin32GLSupport.cpp

Remove the MinGW-specific hard-coded module-name lookup.

Use the address-based GetModuleHandleEx implementation for MinGW as well.

Use:

static const TCHAR staticVar = 0;

The initializer is required because the tested MinGW GCC rejects an uninitialized static const TCHAR.

OUT OF SCOPE

Do not change:

OgreWin32Window.cpp OgreD3D9Plugin.cpp OgreWIN32ConfigDialog.cpp CMake files

OgreWin32Window.cpp gets its own reproducer afterward.

Only consider combining OgreWin32Window.cpp and OgreWin32GLSupport.cpp in one pull request after both have been independently tested and shown to have the same underlying module-lookup problem.