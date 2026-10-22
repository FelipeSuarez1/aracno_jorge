
@ REM ######################################
@ REM # Variable to ignore <CR> in DOS
@ REM # line endings
@ set SHELLOPTS=igncr

@ REM ######################################
@ REM # Variable to ignore mixed paths
@ REM # i.e. G:/$SOPC_KIT_NIOS2/bin
@ set CYGWIN=nodosfilewarning



@ set QUARTUS_BIN=%QUARTUS_ROOTDIR%\\bin64
@ REM set QUARTUS_BIN=C:\altera\15.0\quartus\\bin64
@if exist %QUARTUS_BIN%\\quartus_pgm.exe (goto DownLoad)

:DownLoad
%QUARTUS_BIN%\\quartus_pgm.exe -m jtag -c 1 -o "p;MAXII_Power_Monitor.pof"
pause
