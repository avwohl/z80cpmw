@echo off
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "%~dp0z80cpmw.sln" /p:Configuration=Debug /p:Platform=x64 /v:minimal
