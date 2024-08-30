@ECHO OFF
setlocal enableextensions
cd /d "%~dp0"
rmdir ..\sdk
mklink /J ..\sdk ..\sdks\sdk_%1
rmdir ..\install
mklink /J ..\install ..\installs\install_%1
