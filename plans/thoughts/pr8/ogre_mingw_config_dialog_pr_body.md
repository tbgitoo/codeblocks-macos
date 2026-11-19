\# Summary



This PR proposes to remove a MinGW-specific hard-coded DLL lookup and to use the address-based implementation which is already in place for other Windows compilers. The change is in  `Components/Bites/src/OgreWIN32ConfigDialog.cpp` and remove a specific runtime failure on MinGW shared (DLL-based) builds.



\# Context



This issue was found while bringing up an OGRE application built with MinGW on Windows.

After the required runtime DLLs were made available beside the executable, the application progressed far enough to invoke the native OGRE configuration dialog. Startup then stopped with:

```text

The specified image file did not contain a resource section.

```

Tracing this failure led to `Components/Bites/src/OgreWIN32ConfigDialog.cpp`.

The Windows configuration dialog is defined as `IDD\\\_DLG\\\_CONFIG` in:

```text

Components/Bites/misc/OgreWin32Resources.rc

```

`Components/Bites/CMakeLists.txt` compiles this resource into `OgreBites`.



\# Root cause



In a MinGW shared build `OgreWIN32ConfigDialog` obtains an `HMODULE` for `OgreMain.dll`, or `OgreMain\\\_d.dll` in a debug build. `GetModuleHandle` itself succeeds because `OgreMain` is loaded. The problem occurs later, when that handle is passed to `DialogBox` while requesting `IDD\\\_DLG\\\_CONFIG`. The handle tells `DialogBox` which dll contains the requested resource, but `IDD\\\_DLG\\\_CONFIG` is embedded in `OgreBites.dll`, not `OgreMain.dll`.

Changing the hard-coded name from `OgreMain.dll` to `OgreBites.dll` confirmed the diagnosis and allowed the dialog to open. However, it seems unnecessary to hardcode a DLL filename here. The existing non-MinGW implementation uses `GetModuleHandleEx` with `GET\\\_MODULE\\\_HANDLE\\\_EX\\\_FLAG\\\_FROM\\\_ADDRESS` and an address defined in `OgreWIN32ConfigDialog.cpp`. This asks Windows for the module that contains the implementation itself, which resolves to `OgreBites` without depending on its filename.



Current MinGW-w64 supports the same address-based lookup. This PR therefore removes the MinGW-specific hard-coded DLL lookup and uses the address-based implementation for MinGW as well.

The issue is relatively narrow because it affects Windows MinGW builds using shared OGRE libraries and invoking the native Bites configuration dialog. Other configurations (i.e. MSVC, and also static linking on MinGW) do not exercise this path. It is still worth fixing, especially because in certain circumstances the failure can be silent ending of the app without error message.



\# Proposed change

In `Components/Bites/src/OgreWIN32ConfigDialog.cpp`, remove the MinGW-specific module lookup and use the existing address-based lookup for all Windows compilers:

```diff

\&#x20;    ConfigDialog::ConfigDialog() : mImpl(new ConfigDialog::PrivateData())

\&#x20;    {

\\-		#ifdef \\\_\\\_MINGW32\\\_\\\_

\\-			#ifdef OGRE\\\_STATIC\\\_LIB

\\-                mImpl->mHInstance = GetModuleHandle( NULL );

\\-			#else

\\-				#if OGRE\\\_DEBUG\\\_MODE == 1

\\-                    mImpl->mHInstance = GetModuleHandle("OgreMain\\\_d.dll");

\\-				#else

\\-					mImpl->mHInstance = GetModuleHandle("OgreMain.dll");

\\-				#endif

\\-			#endif

\\-		#else

\\-			static const TCHAR staticVar;

\\-			GetModuleHandleEx(GET\\\_MODULE\\\_HANDLE\\\_EX\\\_FLAG\\\_FROM\\\_ADDRESS | GET\\\_MODULE\\\_HANDLE\\\_EX\\\_FLAG\\\_UNCHANGED\\\_REFCOUNT, \\\\\\\&staticVar, \\\\\\\&mImpl->mHInstance);

\\-		#endif

\\+			static const TCHAR staticVar = 0;

\\+			GetModuleHandleEx(GET\\\_MODULE\\\_HANDLE\\\_EX\\\_FLAG\\\_FROM\\\_ADDRESS | GET\\\_MODULE\\\_HANDLE\\\_EX\\\_FLAG\\\_UNCHANGED\\\_REFCOUNT, \\\\\\\&staticVar, \\\\\\\&mImpl->mHInstance);



\&#x20;        mImpl->mSelectedRenderSystem = 0;

\&#x20;    }

```

No resource or CMake code is changed. `staticVar` is initialized because GCC rejects an uninitialized `static const TCHAR` when the previously excluded code is compiled under MinGW, at least on my machine.



\# Test environment

OGRE: current `master` at test time

Environment: MSYS2 MinGW64

Compiler: GCC 15.2.0, target `x86\\\_64-w64-mingw32`

Generator: `MinGW Makefiles`

Build type: `Release`

Platform: Windows

The MSYS2 MinGW toolchain was selected explicitly. In this test environment, MSYS2 and CMake were installed under `D:\\\\msys64` and `D:\\\\Program Files\\\\CMake`, respectively:

```bat

set "MSYS\\\_MINGW=D:\\\\msys64\\\\mingw64"

set "PATH=%MSYS\\\_MINGW%\\\\bin;D:\\\\Program Files\\\\CMake\\\\bin;%SystemRoot%\\\\system32;%SystemRoot%"



where gcc

where g++

where mingw32-make

where windres



gcc --version

gcc -dumpmachine

```

Configuration command, run from the repository root:

```bat

cmake -S . -B build-test ^

\&#x20; -G "MinGW Makefiles" ^

\&#x20; -DCMAKE\\\_BUILD\\\_TYPE=Release ^

\&#x20; -DCMAKE\\\_MAKE\\\_PROGRAM=mingw32-make.exe ^

\&#x20; -DCMAKE\\\_C\\\_COMPILER=gcc.exe ^

\&#x20; -DCMAKE\\\_CXX\\\_COMPILER=g++.exe ^

\&#x20; -DCMAKE\\\_RC\\\_COMPILER=windres.exe

```

Targeted build command:

```bat

cmake --build build-test --target OgreBites --parallel 4

```

\# Reproduction

No existing target directly invokes both `OgreBites::getNativeConfigDialog()` and `Ogre::Root::showConfigDialog()`. The temporary program below does so. It is not part of the repository and was placed in `build-test\\\\dialog-ab\\\\`.

`Ogre::Root` is constructed with empty plugin and configuration filenames. `showConfigDialog()` still calls `ConfigDialog::display()`, which calls `DialogBox` for `IDD\\\_DLG\\\_CONFIG`. No render-system plugin is required to test whether the dialog resource can be loaded.

`build-test\\\\dialog-ab\\\\config\\\_dialog\\\_repro.cpp`:

```cpp

\\#include "OgreRoot.h"

\\#include "OgreException.h"

\\#include "OgreBitesConfigDialog.h"

\\#include <iostream>



int main()

{

\&#x20;   try

\&#x20;   {

\&#x20;       Ogre::Root root("", "", "dialog-ab-ogre.log");

\&#x20;       std::cout << "calling getNativeConfigDialog\\\\n" << std::flush;

\&#x20;       Ogre::ConfigDialog\\\* dialog = OgreBites::getNativeConfigDialog();

\&#x20;       std::cout << "calling showConfigDialog\\\\n" << std::flush;

\&#x20;       bool accepted = root.showConfigDialog(dialog);

\&#x20;       std::cout << "showConfigDialog returned " << (accepted ? 1 : 0) << "\\\\n";

\&#x20;       return accepted ? 0 : 2;

\&#x20;   }

\&#x20;   catch (const Ogre::Exception\\\\\\\& e)

\&#x20;   {

\&#x20;       std::cerr << "Ogre::Exception: " << e.getFullDescription() << "\\\\n";

\&#x20;       return 1;

\&#x20;   }

}

```

Compile the reproducer once after `OgreBites` and `OgreMain` have been built. The executable is written beside the OGRE DLLs:

```bat

g++ -std=c++17 -DNDEBUG -O2 ^

\&#x20; -o build-test\\\\bin\\\\config\\\_dialog\\\_repro.exe ^

\&#x20; build-test\\\\dialog-ab\\\\config\\\_dialog\\\_repro.cpp ^

\&#x20; -Ibuild-test\\\\include ^

\&#x20; -IOgreMain\\\\include ^

\&#x20; -IComponents\\\\Bites\\\\include ^

\&#x20; -Lbuild-test\\\\lib ^

\&#x20; -lOgreBites ^

\&#x20; -lOgreMain

```

Run:

```bat

build-test\\\\bin\\\\config\\\_dialog\\\_repro.exe

```

\# Behavior before the change

The reproducer reaches the native configuration-dialog path and fails in `ConfigDialog::display()`:

```text

...

\\\*-\\\*-\\\* Version 14.6.4 (Tsathoggua)

calling getNativeConfigDialog

calling showConfigDialog

\\\*-\\\*-\\\* OGRE Shutdown

Unregistering ResourceManager for type Compositor

Unregistering ResourceManager for type Material

Unregistering ResourceManager for type GpuProgram

Unregistering ResourceManager for type Skeleton

Unregistering ResourceManager for type Mesh

Ogre::Exception: : The specified image file did not contain a resource section.

\&#x20;in ConfigDialog::display

```

\# Behavior after the change

After applying the change and rebuilding only `OgreBites`:

```bat

cmake --build build-test --target OgreBites --parallel 4

```

running the same, unchanged `config\\\_dialog\\\_repro.exe` displays the native Windows configuration dialog.

The rendering-system list is empty because the reproducer deliberately loads no render-system plugins. Loading and displaying the dialog resource is the behavior under test.

