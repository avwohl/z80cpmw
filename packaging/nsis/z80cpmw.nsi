; z80cpmw NSIS Installer Script
; For direct distribution outside Microsoft Store

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; Application metadata
!define APPNAME "Z80CPM"
!define COMPANYNAME "Aaron Wohl"
!define DESCRIPTION "Z80 CPU emulator for CP/M and vintage operating systems"
!define VERSIONMAJOR 1
!define VERSIONMINOR 0
!define VERSIONBUILD 10
!define HELPURL "https://github.com/avwohl/z80cpmw"
!define UPDATEURL "https://github.com/avwohl/z80cpmw/releases"
!define ABOUTURL "https://github.com/avwohl/z80cpmw"

; Installer attributes
Name "${APPNAME}"
OutFile "z80cpmw-${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}-setup.exe"
InstallDir "$PROGRAMFILES64\${COMPANYNAME}\${APPNAME}"
InstallDirRegKey HKLM "Software\${COMPANYNAME}\${APPNAME}" "InstallDir"
RequestExecutionLevel admin

; Version information
VIProductVersion "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "${COMPANYNAME}"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2025 ${COMPANYNAME}"
VIAddVersionKey "FileDescription" "${DESCRIPTION}"
VIAddVersionKey "FileVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
VIAddVersionKey "ProductVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"

; Modern UI configuration
!define MUI_ABORTWARNING
!define MUI_ICON "..\..\z80cpmw\z80cpmw.ico"
!define MUI_UNICON "..\..\z80cpmw\z80cpmw.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

; Installer sections
Section "Main Application" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"

    ; Main executable
    File "..\..\bin\Release\z80cpmw.exe"

    ; Required DLLs
    File "..\..\bin\Release\wxbase331u_vc_x64_custom.dll"
    File "..\..\bin\Release\wxmsw331u_core_vc_x64_custom.dll"
    File "..\..\bin\Release\tiff.dll"
    File "..\..\bin\Release\jpeg62.dll"
    File "..\..\bin\Release\libpng16.dll"
    File "..\..\bin\Release\libwebp.dll"
    File "..\..\bin\Release\libwebpdecoder.dll"
    File "..\..\bin\Release\libwebpdemux.dll"
    File "..\..\bin\Release\libsharpyuv.dll"
    File "..\..\bin\Release\zlib1.dll"
    File "..\..\bin\Release\pcre2-16.dll"
    File "..\..\bin\Release\liblzma.dll"

    ; ROM files
    SetOutPath "$INSTDIR\roms"
    File "..\..\bin\Release\roms\emu_avw.rom"
    File "..\..\bin\Release\roms\emu_romwbw.rom"
    File "..\..\bin\Release\roms\SBC_simh_std.rom"

    ; Disk images
    SetOutPath "$INSTDIR\disks"
    File "..\..\bin\Release\disks\hd1k_combo.img"
    File "..\..\bin\Release\disks\hd1k_games.img"

    ; Create start menu shortcuts
    SetOutPath "$INSTDIR"
    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\z80cpmw.exe"
    CreateShortCut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Create desktop shortcut
    CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\z80cpmw.exe"

    ; Write registry keys for uninstaller
    WriteRegStr HKLM "Software\${COMPANYNAME}\${APPNAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\z80cpmw.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${COMPANYNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "HelpLink" "${HELPURL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "URLUpdateInfo" "${UPDATEURL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "URLInfoAbout" "${ABOUTURL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "VersionMajor" ${VERSIONMAJOR}
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "VersionMinor" ${VERSIONMINOR}
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1

    ; Calculate installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "EstimatedSize" "$0"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; File associations (optional)
Section "File Associations" SecFileAssoc
    ; .img files
    WriteRegStr HKCR ".img" "" "z80cpmw.DiskImage"
    WriteRegStr HKCR "z80cpmw.DiskImage" "" "CP/M Disk Image"
    WriteRegStr HKCR "z80cpmw.DiskImage\DefaultIcon" "" "$INSTDIR\z80cpmw.exe,0"
    WriteRegStr HKCR "z80cpmw.DiskImage\shell\open\command" "" '"$INSTDIR\z80cpmw.exe" "%1"'

    ; .dsk files
    WriteRegStr HKCR ".dsk" "" "z80cpmw.DiskImage"

    ; .rom files
    WriteRegStr HKCR ".rom" "" "z80cpmw.ROMFile"
    WriteRegStr HKCR "z80cpmw.ROMFile" "" "Z80 ROM File"
    WriteRegStr HKCR "z80cpmw.ROMFile\DefaultIcon" "" "$INSTDIR\z80cpmw.exe,0"
    WriteRegStr HKCR "z80cpmw.ROMFile\shell\open\command" "" '"$INSTDIR\z80cpmw.exe" "%1"'
SectionEnd

; Uninstaller section
Section "Uninstall"
    ; Remove files
    Delete "$INSTDIR\z80cpmw.exe"
    Delete "$INSTDIR\wxbase331u_vc_x64_custom.dll"
    Delete "$INSTDIR\wxmsw331u_core_vc_x64_custom.dll"
    Delete "$INSTDIR\tiff.dll"
    Delete "$INSTDIR\jpeg62.dll"
    Delete "$INSTDIR\libpng16.dll"
    Delete "$INSTDIR\libwebp.dll"
    Delete "$INSTDIR\libwebpdecoder.dll"
    Delete "$INSTDIR\libwebpdemux.dll"
    Delete "$INSTDIR\libsharpyuv.dll"
    Delete "$INSTDIR\zlib1.dll"
    Delete "$INSTDIR\pcre2-16.dll"
    Delete "$INSTDIR\liblzma.dll"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove ROM files
    Delete "$INSTDIR\roms\emu_avw.rom"
    Delete "$INSTDIR\roms\emu_romwbw.rom"
    Delete "$INSTDIR\roms\SBC_simh_std.rom"
    RMDir "$INSTDIR\roms"

    ; Remove disk images
    Delete "$INSTDIR\disks\hd1k_combo.img"
    Delete "$INSTDIR\disks\hd1k_games.img"
    RMDir "$INSTDIR\disks"

    ; Remove install directory
    RMDir "$INSTDIR"

    ; Remove shortcuts
    Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
    Delete "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"
    RMDir "$SMPROGRAMS\${APPNAME}"
    Delete "$DESKTOP\${APPNAME}.lnk"

    ; Remove registry keys
    DeleteRegKey HKLM "Software\${COMPANYNAME}\${APPNAME}"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

    ; Remove file associations
    DeleteRegKey HKCR "z80cpmw.DiskImage"
    DeleteRegKey HKCR "z80cpmw.ROMFile"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "Install the main application files (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecFileAssoc} "Associate .img, .dsk, and .rom files with z80cpmw"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Functions
Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "This application requires a 64-bit version of Windows."
        Abort
    ${EndIf}
FunctionEnd
