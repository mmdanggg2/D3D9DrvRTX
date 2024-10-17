@ECHO OFF
call ChangeLinks.bat nerf
python SetDebug.py Nerf.exe
echo #define GAME_NAME "Nerf Arena Blast v300"> ..\gamename.h
