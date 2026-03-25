@ECHO OFF
call ChangeLinks.bat ds9_the_fallen
python SetProperties.py --debug-exe DS9.exe --no-unicode
echo #define GAME_NAME "DS9 The Fallen"> ..\gamename.h
