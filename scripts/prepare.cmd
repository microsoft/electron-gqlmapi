@echo off

rem Clean the bin directory
if exist bin (
    del /q bin\*.*
) else (
    mkdir bin
)

rem Build the release target
cd src
call npx cmake-js compile

rem Copy the outputs to the bin directory
copy .\build\Release\*.node ..\bin\
copy .\build\Release\graphql*.dll ..\bin\