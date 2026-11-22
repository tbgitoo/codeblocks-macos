OGRE - MinGW module handle in OgreWin32GLSupport.cpp



STATUS



Initial GL-only reproducer completed.



The GL-only test established that RenderSystem\_GL.dll can execute

Win32GLSupport::initialiseWGL() while RenderSystem\_GL3Plus.dll is absent.



In that configuration, the current MinGW lookup:



GetModuleHandle("RenderSystem\_GL3Plus.dll")



returns NULL, yet the temporary OgreWglDummy window is still created

successfully and WGL/FSAA enumeration completes.



Therefore NULL alone has not produced an observable failure.



The next test targets the more significant case where both render-system DLLs

exist and the current code obtains a valid handle to the wrong module.



This remains a separate follow-up to OGRE PR #3823.



Start with OgreWin32GLSupport.cpp only.



OgreWin32Window.cpp can be tested separately afterward.





TEST ENVIRONMENT



Use the same MSYS2 MinGW64 environment used for the previous OGRE tests.



Run from Command Prompt:



set "MSYS\_MINGW=D:\\msys64\\mingw64"

set "PATH=%MSYS\_MINGW%\\bin;D:\\Program Files\\CMake\\bin;%SystemRoot%\\system32;%SystemRoot%"



Verify the toolchain:



where gcc

where g++

where mingw32-make

where windres



gcc --version

gcc -dumpmachine



Expected compiler:



GCC 15.2.0

target x86\_64-w64-mingw32





PROBLEM



OgreWin32GLSupport.cpp contains a MinGW-specific lookup:



GetModuleHandle("RenderSystem\_GL3Plus.dll")



The returned HINSTANCE is used for the temporary OgreWglDummy window class

created by Win32GLSupport::initialiseWGL():



dummyClass.hInstance = hinst;



RegisterClass(\&dummyClass);



CreateWindow(

&#x20;   dummyText,

&#x20;   dummyText,

&#x20;   WS\_POPUP | WS\_CLIPCHILDREN,

&#x20;   0, 0, 32, 32,

&#x20;   0, 0,

&#x20;   hinst,

&#x20;   0);



...



UnregisterClass(dummyText, hinst);



OgreWin32GLSupport.cpp is compiled into the static helper library

OgreGLSupport.



That helper is linked into multiple render-system DLLs, including:



RenderSystem\_GL.dll

RenderSystem\_GL3Plus.dll

RenderSystem\_GLES2.dll



Therefore the code executing Win32GLSupport::initialiseWGL() is not

necessarily located in RenderSystem\_GL3Plus.dll.





ALREADY ESTABLISHED: GL-ONLY CASE



A previous external test built and loaded:



RenderSystem\_GL.dll



without:



RenderSystem\_GL3Plus.dll



The host successfully reached Win32GLSupport initialization and reported WGL

FSAA modes:



FSAA \[ 0] \[ 2] \[ 4] \[ 8] \[16]



The host also established:



RenderSystem\_GL.dll: loaded

RenderSystem\_GL3Plus.dll: not loaded



Therefore the hard-coded lookup in this configuration returns NULL.



Despite that, WGL initialization succeeds.



This means that simply demonstrating:



GetModuleHandle("RenderSystem\_GL3Plus.dll") -> NULL



is not sufficient to establish an observable defect.





NEW REPRODUCER: BOTH DLLs PRESENT



The next reproducer should build both:



RenderSystem\_GL.dll

RenderSystem\_GL3Plus.dll



The important test case is:



1\. RenderSystem\_GL3Plus.dll is loaded.

2\. RenderSystem\_GL.dll is also loaded.

3\. Win32GLSupport::initialiseWGL() executes from the copy of

&#x20;  OgreWin32GLSupport.cpp linked into RenderSystem\_GL.dll.

4\. The current MinGW code nevertheless obtains:



&#x20;  GetModuleHandle("RenderSystem\_GL3Plus.dll")



5\. Because GL3Plus is now loaded, this lookup returns a valid HINSTANCE.



However, it is the HINSTANCE of:



RenderSystem\_GL3Plus.dll



rather than the DLL containing the currently executing copy of

OgreWin32GLSupport.cpp:



RenderSystem\_GL.dll



This creates the test condition:



executing code:

RenderSystem\_GL.dll



dummyWndProc:

RenderSystem\_GL.dll



WNDCLASS.hInstance:

RenderSystem\_GL3Plus.dll



CreateWindow hInstance:

RenderSystem\_GL3Plus.dll





BUILD CONFIGURATION



Configure OGRE with both legacy GL and GL3Plus enabled.



From the OGRE repository root:



cmake -S . -B build-glsupport ^

\-G "MinGW Makefiles" ^

\-DCMAKE\_BUILD\_TYPE=Release ^

\-DCMAKE\_MAKE\_PROGRAM=mingw32-make.exe ^

\-DCMAKE\_C\_COMPILER=gcc.exe ^

\-DCMAKE\_CXX\_COMPILER=g++.exe ^

\-DCMAKE\_RC\_COMPILER=windres.exe ^

\-DOGRE\_BUILD\_RENDERSYSTEM\_GL3PLUS=ON ^

\-DOGRE\_BUILD\_RENDERSYSTEM\_GLES2=OFF ^

\-DOGRE\_BUILD\_SAMPLES=OFF ^

\-DOGRE\_BUILD\_TESTS=OFF ^

\-DOGRE\_BUILD\_TOOLS=OFF



Build both render systems:



cmake --build build-glsupport --target RenderSystem\_GL --parallel 4



cmake --build build-glsupport --target RenderSystem\_GL3Plus --parallel 4



Verify:



build-glsupport\\bin\\RenderSystem\_GL.dll



exists.



Verify:



build-glsupport\\bin\\RenderSystem\_GL3Plus.dll



exists.





TEMPORARY TEST HOST



Use the existing disposable external test area:



build-glsupport\\glsupport-ab\\



Do not add the host to the OGRE source tree.



Do not commit the host.



Do not add diagnostic changes to tracked OGRE source.



The host should construct Ogre::Root and explicitly load both plugins.



Load RenderSystem\_GL3Plus.dll first.



Then load RenderSystem\_GL.dll.



Do not rely on plugins.cfg.



Report explicitly:



OgreMain.dll:

loaded/not loaded and path



RenderSystem\_GL3Plus.dll:

loaded/not loaded and path



RenderSystem\_GL.dll:

loaded/not loaded and path



Enumerate the installed render systems afterward.





EXTERNAL HANDLE CHECK



The test host should reproduce the relevant Win32 module queries externally.



After both DLLs are loaded, call:



GetModuleHandle("RenderSystem\_GL3Plus.dll")



and:



GetModuleHandle("RenderSystem\_GL.dll")



Use GetModuleFileName() to report the module corresponding to each returned

handle.



Expected result:



GetModuleHandle("RenderSystem\_GL3Plus.dll")

&#x20;   -> valid handle

&#x20;   -> RenderSystem\_GL3Plus.dll



GetModuleHandle("RenderSystem\_GL.dll")

&#x20;   -> valid handle

&#x20;   -> RenderSystem\_GL.dll



This establishes that the current hard-coded lookup does not merely produce

NULL when legacy GL is active.



When GL3Plus is also present, the lookup can produce a valid handle to a

different render-system DLL.





ADDRESS-BASED EXTERNAL CHECK



Obtain an address known to reside inside RenderSystem\_GL.dll, for example an

exported plugin function.



Use:



GetModuleHandleEx(

&#x20;   GET\_MODULE\_HANDLE\_EX\_FLAG\_FROM\_ADDRESS |

&#x20;   GET\_MODULE\_HANDLE\_EX\_FLAG\_UNCHANGED\_REFCOUNT,

&#x20;   address\_in\_RenderSystem\_GL,

&#x20;   \&hinst);



Use GetModuleFileName() to report the resulting module.



Expected result:



address in RenderSystem\_GL.dll

&#x20;   -> RenderSystem\_GL.dll



This does not prove that the exported function has the same address as data in

OgreWin32GLSupport.cpp.



It demonstrates the Windows FROM\_ADDRESS module-resolution mechanism using an

address known to reside in the same final DLL.





WINDOW-CLASS TEST



Using only the external temporary test code, investigate whether a valid but

incorrect HINSTANCE produces observable differences from the correct module

handle.



Test at least:



A. hInstance = NULL



B. hInstance = RenderSystem\_GL.dll



C. hInstance = RenderSystem\_GL3Plus.dll while the WNDPROC resides in

&#x20;  RenderSystem\_GL.dll or in a disposable external probe DLL



For each case, record:



RegisterClass return value

GetLastError after failure

CreateWindow return value

GetLastError after failure

DestroyWindow result

UnregisterClass result

GetLastError after failure



Where useful, inspect the registered class using GetClassInfo or

GetClassInfoEx.



Do not infer a failure from an incorrect module identity alone.



The purpose is to determine whether Windows exhibits any observable behavioral

difference.





SAME-CLASS / TWO-MODULE TEST



Pay particular attention to the class name:



OgreWglDummy



Both copies of OgreWin32GLSupport.cpp use the same class name.



Investigate the situation where both render-system DLLs are present and the

current MinGW implementation associates both initializations with:



RenderSystem\_GL3Plus.dll



even when one initialization originates from RenderSystem\_GL.dll.



Determine experimentally whether this can cause:



ERROR\_CLASS\_ALREADY\_EXISTS



class lookup ambiguity



registration failure



CreateWindow failure



UnregisterClass failure



one renderer interacting with a class registered by the other renderer



different behavior depending on plugin initialization order



Do not describe ERROR\_CLASS\_ALREADY\_EXISTS alone as a race or defect unless it

causes an observable problem in a realistic OGRE call sequence.





CONCURRENCY



A previous investigation did not identify a meaningful OGRE race condition.



Concurrent attempts to register the same class name can naturally produce

registration contention.



Do not make concurrency the primary justification for this change.



Only report a race if an OGRE-reachable sequence produces an observable

incorrect result that depends on timing.





SEPARATE CLEANUP ISSUE



The investigation identified an independent exception-safety issue:



RegisterClass(\&dummyClass);



HWND hwnd = CreateWindow(...);



if (hwnd == NULL)

&#x20;   OGRE\_EXCEPT(...);



...



UnregisterClass(dummyText, hinst);



If RegisterClass succeeds but CreateWindow fails, the exception occurs before

UnregisterClass.



This can leave OgreWglDummy registered.



Also, the return values from RegisterClass and UnregisterClass are currently

ignored.



This is a separate issue.



Do not include cleanup changes in the module-handle patch unless testing shows

that they are directly required to reproduce the module-handle problem.



Record this as a possible separate follow-up.





A/B SOURCE TEST



Use the already committed module-handle fix for the source-level A/B test.



Baseline:



commit 972cdd1c6



Fixed:



branch fix-mingw-glsupport-module-handle



For A:



git switch --detach 972cdd1c6



Rebuild the required render systems and run the same test host.



For B:



git switch fix-mingw-glsupport-module-handle



Rebuild the same render systems and run exactly the same host.



Do not modify tracked OGRE source during either test.





BASELINE IMPLEMENTATION



The baseline MinGW path uses:



GetModuleHandle("RenderSystem\_GL3Plus.dll")



With GL3Plus loaded, the important expected observation is:



legacy GL code executing from:

RenderSystem\_GL.dll



hard-coded HINSTANCE:

RenderSystem\_GL3Plus.dll





FIXED IMPLEMENTATION



The proposed implementation removes the MinGW-specific DLL-name lookup and

uses the existing address-based mechanism:



static const TCHAR staticVar = 0;



GetModuleHandleEx(

&#x20;   GET\_MODULE\_HANDLE\_EX\_FLAG\_FROM\_ADDRESS |

&#x20;   GET\_MODULE\_HANDLE\_EX\_FLAG\_UNCHANGED\_REFCOUNT,

&#x20;   \&staticVar,

&#x20;   \&hinst);



For the copy inside RenderSystem\_GL.dll, expected:



hinst -> RenderSystem\_GL.dll



For the copy inside RenderSystem\_GL3Plus.dll, expected:



hinst -> RenderSystem\_GL3Plus.dll



The initializer:



= 0



is required because the tested MinGW GCC rejects the uninitialized:



static const TCHAR staticVar;





SUCCESS CRITERIA



There are two possible useful outcomes.



Outcome 1: observable behavioral defect



The wrong non-NULL module handle causes a reproducible OGRE-relevant problem,

such as:



registration failure

window-creation failure

class collision

cleanup failure

plugin-order-dependent behavior

or another demonstrated Win32 side effect



In that case, document the smallest reliable reproducer.





Outcome 2: no observable behavioral defect



Both NULL and incorrect non-NULL module handles are tolerated in the tested

OgreWglDummy lifecycle.



In that case, do not claim a runtime bug.



The evidence would instead establish only that:



the hard-coded lookup can identify the wrong module



the address-based lookup identifies the actual containing module



the current behavior happens to tolerate the mismatch



The PR should then be presented as removal of an incorrect hard-coded

assumption / code cleanup rather than as a fix for an observed failure.





PROPOSED PATCH



The patch is already committed on:



fix-mingw-glsupport-module-handle



The intended change is limited to:



RenderSystems/GLSupport/src/win32/OgreWin32GLSupport.cpp



Remove the MinGW-specific hard-coded module-name lookup.



Use the address-based GetModuleHandleEx implementation for MinGW as well.



Do not expand the patch based on speculative failure modes.





OUT OF SCOPE



Do not change:



OgreWin32Window.cpp

OgreD3D9Plugin.cpp

OgreWIN32ConfigDialog.cpp

CMake files

RegisterClass/UnregisterClass exception handling



OgreWin32Window.cpp gets its own reproducer afterward.



The RegisterClass/CreateWindow exception-cleanup issue should be investigated

separately.



Only combine additional changes if independent reproducer evidence shows that

they are genuinely part of the same defect.

