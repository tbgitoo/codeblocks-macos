SUMMARY

This PR proposes to remove a MinGW-specific hard-coded DLL lookup and use the address-based implementation already used for other Windows compilers.

The change is in:

Components/Bites/src/OgreWIN32ConfigDialog.cpp

It fixes a specific runtime failure in MinGW shared-library builds.

CONTEXT

This issue was found while bringing up an OGRE application built with MinGW on Windows.

After the required runtime DLLs were made available beside the executable, the application progressed far enough to invoke the native OGRE configuration dialog. Startup then stopped with:

The specified image file did not contain a resource section.

Tracing this failure led to:

Components/Bites/src/OgreWIN32ConfigDialog.cpp

The Windows configuration dialog is defined as IDD_DLG_CONFIG in:

Components/Bites/misc/OgreWin32Resources.rc

Components/Bites/CMakeLists.txt compiles this resource into OgreBites.

ROOT CAUSE

In a MinGW shared build, OgreWIN32ConfigDialog obtains an HMODULE for OgreMain.dll, or OgreMain_d.dll in a debug build.

GetModuleHandle itself succeeds because OgreMain is loaded.

The problem occurs later, when that handle is passed to DialogBox while requesting IDD_DLG_CONFIG. The handle tells DialogBox which DLL contains the requested resource, but IDD_DLG_CONFIG is embedded in OgreBites.dll, not OgreMain.dll.

Changing the hard-coded name from OgreMain.dll to OgreBites.dll confirmed the diagnosis and allowed the dialog to open.

However, it is unnecessary to hard-code a DLL filename here.

The existing non-MinGW implementation uses GetModuleHandleEx with GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS and an address defined in OgreWIN32ConfigDialog.cpp. Windows can therefore determine which loaded module actually contains the implementation. In this case, that module is OgreBites.

Current MinGW-w64 supports the same address-based lookup. This PR therefore removes the MinGW-specific hard-coded DLL lookup and uses the address-based implementation for MinGW as well.

The issue is relatively narrow because it affects Windows MinGW builds using shared OGRE libraries and invoking the native Bites configuration dialog. MSVC and static MinGW builds do not exercise this particular hard-coded lookup.

PROPOSED CHANGE

In:

Components/Bites/src/OgreWIN32ConfigDialog.cpp

remove the MinGW-specific module lookup and use the address-based lookup for all Windows compilers.

Before:

#ifdef MINGW32 #ifdef OGRE_STATIC_LIB mImpl->mHInstance = GetModuleHandle(NULL); #else #if OGRE_DEBUG_MODE == 1 mImpl->mHInstance = GetModuleHandle("OgreMain_d.dll"); #else mImpl->mHInstance = GetModuleHandle("OgreMain.dll"); #endif #endif

#else static const TCHAR staticVar; GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &staticVar, &mImpl->mHInstance); #endif

After:

static const TCHAR staticVar = 0; GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &staticVar, &mImpl->mHInstance);

No resource or CMake code is changed.

staticVar is initialized because GCC rejects an uninitialized static const TCHAR when this previously excluded code is compiled under MinGW.

TEST ENVIRONMENT

OGRE: current master at test time Environment: MSYS2 MinGW64 Compiler: GCC 15.2.0 Target: x86_64-w64-mingw32 Generator: MinGW Makefiles Build type: Release Platform: Windows

The MSYS2 MinGW toolchain was selected explicitly:

set "MSYS_MINGW=D:\msys64\mingw64" 
set "PATH=%MSYS_MINGW%\bin;D:\Program Files\CMake\bin;%SystemRoot%\system32;%SystemRoot%"

where gcc where g++ where mingw32-make where windres

gcc --version gcc -dumpmachine

CONFIGURATION

Run from the repository root:

cmake -S . -B build-test ^
 -G "MinGW Makefiles" ^
 -DCMAKE_BUILD_TYPE=Release ^
 -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
 -DCMAKE_C_COMPILER=gcc.exe ^
 -DCMAKE_CXX_COMPILER=g++.exe ^
 -DCMAKE_RC_COMPILER=windres.exe

Build OgreBites:

cmake --build build-test --target OgreBites --parallel 4

REPRODUCTION

No existing target directly invokes both:

OgreBites::getNativeConfigDialog()

and:

Ogre::Root::showConfigDialog()

A temporary program was therefore used outside the tracked source tree, under:

build-test\dialog-ab\config_dialog_repro.cpp

The program is:

#include "OgreRoot.h" #include "OgreException.h" #include "OgreBitesConfigDialog.h" #include

int main() { try { Ogre::Root root("", "", "dialog-ab-ogre.log");

    std::cout << "calling getNativeConfigDialog\n" << std::flush;
    Ogre::ConfigDialog* dialog = OgreBites::getNativeConfigDialog();

    std::cout << "calling showConfigDialog\n" << std::flush;
    bool accepted = root.showConfigDialog(dialog);

    std::cout << "showConfigDialog returned "
              << (accepted ? 1 : 0) << "\n";

    return accepted ? 0 : 2;
}
catch (const Ogre::Exception& e)
{
    std::cerr << "Ogre::Exception: "
              << e.getFullDescription() << "\n";
    return 1;
}


}

Ogre::Root is constructed without loading plugins. That is sufficient for this test because showConfigDialog() reaches ConfigDialog::display(), which calls DialogBox for IDD_DLG_CONFIG.

No render-system plugin is required to test whether the dialog resource itself can be loaded.

BUILD THE REPRODUCER

After OgreBites and OgreMain have been built:

g++ -std=c++17 -DNDEBUG -O2 ^ -o build-test\bin\config_dialog_repro.exe ^ build-test\dialog-ab\config_dialog_repro.cpp ^ -Ibuild-test\include ^ -IOgreMain\include ^ -IComponents\Bites\include ^ -Lbuild-test\lib ^ -lOgreBites ^ -lOgreMain

Run:

build-test\bin\config_dialog_repro.exe

BEHAVIOR BEFORE THE CHANGE

The reproducer reaches the native configuration-dialog code:

calling getNativeConfigDialog calling showConfigDialog

ConfigDialog::display() then fails with:

Ogre::Exception: : The specified image file did not contain a resource section. in ConfigDialog::display

BEHAVIOR AFTER THE CHANGE

Apply the change and rebuild only OgreBites:

cmake --build build-test --target OgreBites --parallel 4

Then run the same unchanged reproducer:

build-test\bin\config_dialog_repro.exe

The native Windows configuration dialog is displayed successfully.

The rendering-system list is empty because the reproducer deliberately loads no render-system plugins. Loading and displaying the dialog resource is the behavior under test.