@ECHO OFF
setlocal enableextensions
cd /d "%~dp0"
rmdir ..\sdk
mklink /J ..\sdk ..\sdks\sdk_%1
rmdir ..\install
mkdir ..\installs\install_%1
mkdir ..\installs\install_%1\System
mklink /J ..\install ..\installs\install_%1
