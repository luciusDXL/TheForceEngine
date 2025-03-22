@echo off
if exist runTests.ps1  (
   start powershell -ExecutionPolicy bypass -File runTests.ps1
   exit
) else (
    echo.  
    echo. 
    echo     Cannot find  runTests.ps1  in this directory. Make sure you put BOTH files in the same folder!
    echo.
    echo.
)