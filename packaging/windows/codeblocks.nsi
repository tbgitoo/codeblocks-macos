; Code::Blocks Windows v3.4 installer.
; IDE files come from packaging\windows\stage. 7zr.exe and the WinLibs archive are not packed.
; The optional compiler page downloads those two pinned files only when its checkbox is set.

Unicode true
RequestExecutionLevel admin
ManifestDPIAware true

!include LogicLib.nsh
!include nsDialogs.nsh
!include x64.nsh
!include FileFunc.nsh
!include StrFunc.nsh
${StrCase}
${StrTrimNewLines}
!insertmacro DriveSpace
!insertmacro DirState

!define PRODUCT "Code::Blocks"
!define VERSION "3.4"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\CodeBlocks"
!define STAGE "stage"

!define SEVENZIP_URL "https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe"
!define SEVENZIP_SHA "9BF582B894DFA665F852D7E9112B02290A1C303C2D95A21FCDC41FFCB03143B7"
!define WINLIBS_URL "https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-12.0.0-ucrt-r3/winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z"
!define WINLIBS_SHA "0475A96B4D687A3B85DD979A982883ED9172C55B2516E953664232D46471096F"
!define WINLIBS_VERSION "winlibs personal build version gcc-14.2.0-mingw-w64ucrt-12.0.0-r3"

Name "${PRODUCT}"
OutFile "codeblocks-v3.4-win64.exe"
InstallDir "C:\Program Files\CodeBlocks"
InstallDirRegKey HKLM "${UNINSTKEY}" "InstallLocation"
ShowInstDetails show
ShowUninstDetails show
BrandingText "${PRODUCT} ${VERSION}"

VIProductVersion "3.4.0.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${PRODUCT} ${VERSION}"
VIAddVersionKey "LegalCopyright" "GNU General Public License version 3"

Var InstallCompiler
Var CompilerCheckbox
Var ReplaceAnswer
Var CurlExe
Var PowerShellExe
Var UnpackDir
Var OwnUnpack
Var CompilerRunning
Var CompilerError

Page license
Page directory "" "" DirectoryLeave
Page custom CompilerPage CompilerPageLeave
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

LicenseData "..\..\COPYING"

Function .onInit
  SetShellVarContext all
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  StrCpy $InstallCompiler "1"
  StrCpy $ReplaceAnswer ""
  StrCpy $OwnUnpack "0"
  StrCpy $CompilerRunning "0"
  StrCpy $CompilerError ""
  ReadRegStr $0 HKLM "${UNINSTKEY}" "InstallLocation"
  ${If} $0 != ""
    StrCpy $INSTDIR $0
  ${EndIf}
FunctionEnd

Function un.onInit
  SetShellVarContext all
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
FunctionEnd

Function .onUserAbort
  ${If} $CompilerRunning == "1"
    Call CleanupCompilerTemps
  ${EndIf}
FunctionEnd

Function NormalizeDir
  Exch $0
  Push $1
  Push $2
  StrCpy $1 $0 1 -1
  ${If} $1 == "\"
    StrLen $2 $0
    IntOp $2 $2 - 1
    StrCpy $0 $0 $2
  ${EndIf}
  Pop $2
  Pop $1
  Exch $0
FunctionEnd

Function EnsureReplaceAllowed
  IfFileExists "$INSTDIR\codeblocks.exe" 0 replace_ok
  Push $INSTDIR
  Call NormalizeDir
  Pop $1
  ReadRegStr $0 HKLM "${UNINSTKEY}" "InstallLocation"
  ${If} $0 != ""
    Push $0
    Call NormalizeDir
    Pop $0
  ${EndIf}
  ${If} $0 == $1
    Goto replace_ok
  ${EndIf}
  IfSilent 0 replace_ask
  MessageBox MB_ICONSTOP "This folder already contains Code::Blocks. A silent install will not replace it."
  Abort
replace_ask:
  ${If} $ReplaceAnswer == "yes"
    Goto replace_ok
  ${EndIf}
  MessageBox MB_YESNO|MB_ICONEXCLAMATION "This folder already contains Code::Blocks. Replace the program files in this folder?$\r$\n$\r$\nAn existing MinGW folder will be left unchanged." IDYES replace_yes
  Abort
replace_yes:
  StrCpy $ReplaceAnswer "yes"
replace_ok:
FunctionEnd

Function DirectoryLeave
  Call EnsureReplaceAllowed
FunctionEnd

Function CompilerPage
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 80u "This can download the recommended MinGW compiler. It gets 7-Zip 26.05 7zr.exe and winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z from their publishers. The compiler archive is about 101 MB and the extracted tree is about 0.8 GiB. A network connection is required.$\r$\n$\r$\nGCC, GDB, GNU Binutils, and GNU Make in that archive are GPL software from WinLibs, not from this installer. 7zr.exe is 7-Zip 26.05, GNU LGPL, from https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe . An existing MinGW folder in the selected directory is left unchanged."
  Pop $0
  ${NSD_CreateCheckbox} 0 88u 100% 12u "Download and install the recommended MinGW compiler"
  Pop $CompilerCheckbox
  ${If} $InstallCompiler == "1"
    ${NSD_Check} $CompilerCheckbox
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function CompilerPageLeave
  ${NSD_GetState} $CompilerCheckbox $InstallCompiler
FunctionEnd

Function FindCurl
  StrCpy $CurlExe ""
  IfFileExists "$WINDIR\Sysnative\curl.exe" 0 findcurl_sys
  StrCpy $CurlExe "$WINDIR\Sysnative\curl.exe"
  Return
findcurl_sys:
  IfFileExists "$SYSDIR\curl.exe" 0 findcurl_done
  StrCpy $CurlExe "$SYSDIR\curl.exe"
findcurl_done:
FunctionEnd

Function FindPowerShell
  StrCpy $PowerShellExe ""
  IfFileExists "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe" 0 findps_sys
  StrCpy $PowerShellExe "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
  Return
findps_sys:
  IfFileExists "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" 0 findps_done
  StrCpy $PowerShellExe "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe"
findps_done:
FunctionEnd

Function CleanupCompilerTemps
  Delete "$PLUGINSDIR\7zr.exe"
  Delete "$PLUGINSDIR\7zr.sha256"
  Delete "$PLUGINSDIR\winlibs.7z"
  Delete "$PLUGINSDIR\winlibs.sha256"
  ${If} $OwnUnpack == "1"
  ${AndIf} $UnpackDir != ""
    RMDir /r "$UnpackDir"
    StrCpy $OwnUnpack "0"
  ${EndIf}
  StrCpy $CompilerRunning "0"
FunctionEnd

Function HashFile
  ; Input: $R8 path, $R7 expected uppercase digest. Output: $R9 1 on match.
  StrCpy $R9 "0"
  Delete "$PLUGINSDIR\hash.sha256"
  System::Call 'kernel32::SetEnvironmentVariable(t "CB_V34_ARCHIVE", t "$R8") i .r0'
  System::Call 'kernel32::SetEnvironmentVariable(t "CB_V34_HASHOUT", t "$PLUGINSDIR\hash.sha256") i .r0'
  ${If} $0 == 0
    StrCpy $CompilerError "Could not pass the file path to PowerShell."
    Return
  ${EndIf}
  nsExec::ExecToLog '"$PowerShellExe" -NoProfile -NonInteractive -Command "Set-Content -LiteralPath $$env:CB_V34_HASHOUT -Value (Get-FileHash -LiteralPath $$env:CB_V34_ARCHIVE -Algorithm SHA256).Hash -Encoding ascii"'
  Pop $0
  ${If} $0 != 0
    StrCpy $CompilerError "SHA-256 could not be calculated."
    Delete "$PLUGINSDIR\hash.sha256"
    Return
  ${EndIf}
  FileOpen $1 "$PLUGINSDIR\hash.sha256" r
  ${If} $1 == ""
    StrCpy $CompilerError "SHA-256 output was not written."
    Return
  ${EndIf}
  FileRead $1 $2
  FileClose $1
  Delete "$PLUGINSDIR\hash.sha256"
  ${StrTrimNewLines} $2 $2
  ${StrCase} $2 $2 "U"
  ${If} $2 != $R7
    StrCpy $CompilerError "SHA-256 did not match the pinned digest."
    Return
  ${EndIf}
  StrCpy $R9 "1"
FunctionEnd

Function InstallCompilerIfRequested
  ${If} $InstallCompiler != "1"
    DetailPrint "Compiler download was not selected."
    Return
  ${EndIf}
  ${DirState} "$INSTDIR\MinGW" $0
  ${If} $0 != 0
    DetailPrint "Left the existing MinGW folder unchanged."
    Return
  ${EndIf}

  InitPluginsDir
  System::Call 'kernel32::GetTickCount() i .r0'
  StrCpy $UnpackDir "$INSTDIR\cb-v34-$0"
  ${DirState} "$UnpackDir" $1
  ${If} $1 != 0
    StrCpy $CompilerError "Could not install the compiler because $UnpackDir already exists."
    Goto compiler_fail
  ${EndIf}
  CreateDirectory "$UnpackDir"
  ${If} ${Errors}
    StrCpy $CompilerError "Could not create a temporary folder for the compiler."
    Goto compiler_fail
  ${EndIf}
  StrCpy $OwnUnpack "1"
  StrCpy $CompilerRunning "1"

  StrCpy $2 $INSTDIR 3
  StrCpy $3 $PLUGINSDIR 3
  ${DriveSpace} "$2" "/D=F" $4
  ${If} $4 < 1153434
    StrCpy $CompilerError "Not enough free space on $2 for the compiler."
    Goto compiler_fail
  ${EndIf}
  ${StrCase} $5 $2 "U"
  ${StrCase} $6 $3 "U"
  ${If} $5 != $6
    ${DriveSpace} "$3" "/D=F" $4
    ${If} $4 < 112640
      StrCpy $CompilerError "Not enough free space on $3 for the compiler download."
      Goto compiler_fail
    ${EndIf}
  ${EndIf}

  Call FindCurl
  ${If} $CurlExe == ""
    StrCpy $CompilerError "curl.exe was not found."
    Goto compiler_fail
  ${EndIf}
  Call FindPowerShell
  ${If} $PowerShellExe == ""
    StrCpy $CompilerError "powershell.exe was not found."
    Goto compiler_fail
  ${EndIf}

  DetailPrint "Downloading 7-Zip 26.05 7zr.exe"
  nsExec::ExecToLog '"$CurlExe" -fL --retry 2 -o "$PLUGINSDIR\7zr.exe" "${SEVENZIP_URL}"'
  Pop $0
  ${If} $0 != 0
    StrCpy $CompilerError "The 7zr.exe download failed."
    Goto compiler_fail
  ${EndIf}
  StrCpy $R8 "$PLUGINSDIR\7zr.exe"
  StrCpy $R7 "${SEVENZIP_SHA}"
  Call HashFile
  ${If} $R9 != "1"
    Goto compiler_fail
  ${EndIf}

  DetailPrint "Downloading the pinned WinLibs archive"
  nsExec::ExecToLog '"$CurlExe" -fL --retry 2 -o "$PLUGINSDIR\winlibs.7z" "${WINLIBS_URL}"'
  Pop $0
  ${If} $0 != 0
    StrCpy $CompilerError "The MinGW archive download failed."
    Goto compiler_fail
  ${EndIf}
  StrCpy $R8 "$PLUGINSDIR\winlibs.7z"
  StrCpy $R7 "${WINLIBS_SHA}"
  Call HashFile
  ${If} $R9 != "1"
    Goto compiler_fail
  ${EndIf}

  DetailPrint "Extracting the MinGW archive"
  nsExec::ExecToLog '"$PLUGINSDIR\7zr.exe" x -y -o"$UnpackDir" "$PLUGINSDIR\winlibs.7z"'
  Pop $0
  ${If} $0 != 0
    StrCpy $CompilerError "Extracting the MinGW archive failed."
    Goto compiler_fail
  ${EndIf}
  ${IfNot} ${FileExists} "$UnpackDir\mingw64\bin\gcc.exe"
  ${OrIfNot} ${FileExists} "$UnpackDir\mingw64\bin\g++.exe"
  ${OrIfNot} ${FileExists} "$UnpackDir\mingw64\bin\gdb.exe"
    StrCpy $CompilerError "The extracted compiler is missing gcc.exe, g++.exe, or gdb.exe."
    Goto compiler_fail
  ${EndIf}
  FileOpen $1 "$UnpackDir\mingw64\version_info.txt" r
  ${If} $1 == ""
    StrCpy $CompilerError "The extracted compiler is missing version_info.txt."
    Goto compiler_fail
  ${EndIf}
  FileRead $1 $2
  FileClose $1
  ${StrTrimNewLines} $2 $2
  ${If} $2 != "${WINLIBS_VERSION}"
    StrCpy $CompilerError "The extracted compiler version line did not match."
    Goto compiler_fail
  ${EndIf}

  ${DirState} "$INSTDIR\MinGW" $0
  ${If} $0 != 0
    StrCpy $CompilerError "Could not install the compiler because $INSTDIR\MinGW now exists."
    Goto compiler_fail
  ${EndIf}
  ClearErrors
  Rename "$UnpackDir\mingw64" "$INSTDIR\MinGW"
  ${If} ${Errors}
    StrCpy $CompilerError "Could not move the compiler into the install folder."
    Goto compiler_fail
  ${EndIf}
  ClearErrors
  FileOpen $1 "$INSTDIR\MinGW\.codeblocks-v34-mingw" w
  ${If} $1 == ""
    RMDir /r "$INSTDIR\MinGW"
    StrCpy $CompilerError "Could not record ownership of the compiler folder."
    Goto compiler_fail
  ${EndIf}
  FileWrite $1 "${WINLIBS_SHA}$\r$\n"
  ${If} ${Errors}
    FileClose $1
    RMDir /r "$INSTDIR\MinGW"
    StrCpy $CompilerError "Could not record ownership of the compiler folder."
    Goto compiler_fail
  ${EndIf}
  FileClose $1
  Call CleanupCompilerTemps
  DetailPrint "Installed MinGW from the pinned WinLibs archive."
  Return

compiler_fail:
  Call CleanupCompilerTemps
  DetailPrint "$CompilerError"
  MessageBox MB_ICONEXCLAMATION "Code::Blocks was installed. The MinGW compiler was not.$\r$\n$\r$\n$CompilerError"
FunctionEnd

Section
  Call EnsureReplaceAllowed
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\CodeBlocks.lnk" "$INSTDIR\codeblocks.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "${PRODUCT}"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\codeblocks.exe"
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  Call InstallCompilerIfRequested
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\codeblocks.exe"
  Delete "$INSTDIR\codeblocks.dll"
  Delete "$INSTDIR\cb_console_runner.exe"
  Delete "$INSTDIR\wxmsw334u_gcc_custom.dll"
  Delete "$INSTDIR\libgcc_s_seh-1.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\exchndl.dll"
  Delete "$INSTDIR\dbghelp.dll"
  Delete "$INSTDIR\dbgcore.dll"
  Delete "$INSTDIR\mgwhelp.dll"
  Delete "$INSTDIR\symsrv.dll"
  Delete "$INSTDIR\symsrv.yes"
  RMDir /r "$INSTDIR\share\CodeBlocks"
  RMDir "$INSTDIR\share"
  IfFileExists "$INSTDIR\MinGW\.codeblocks-v34-mingw" 0 un_nomin
    RMDir /r "$INSTDIR\MinGW"
  un_nomin:
  Delete "$INSTDIR\uninstall.exe"
  Delete "$SMPROGRAMS\CodeBlocks.lnk"
  DeleteRegKey HKLM "${UNINSTKEY}"
  RMDir "$INSTDIR"
SectionEnd
