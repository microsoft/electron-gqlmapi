@echo off
setlocal

rem Run tests against the debug target
set USE_DEBUG_MODULE=1

rem Build the debug target
cd src
call npx cmake-js build -D
cd ..

rem Start the test and wait for the remote debugger to attach
echo Connect to the node process with a remote script debugger...
node --inspect-brk ./node_modules/jest/bin/jest.js --runInBand