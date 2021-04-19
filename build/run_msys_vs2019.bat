REM launch msys2 console through visual studio console
set MSYS2_PATH_TYPE=inherit
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
msys2_shell.cmd -mingw64