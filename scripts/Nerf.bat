@ECHO OFF
call ChangeLinks.bat nerf
python SetProperties.py --debug-exe Nerf.exe
echo #define GAME_NAME "Nerf Arena Blast v300"> ..\gamename.h
