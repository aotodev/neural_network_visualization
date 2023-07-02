@echo off
cd ../../build

if not exist windows mkdir windows
cd windows

if not exist build_files mkdir build_files
cd build_files

if not exist shipping mkdir shipping
cd shipping

call cmake -DBUILD_DEBUG=OFF -DBUILD_RELEASE=OFF -DBUILD_SHIPPING=ON ../../../../
call cmake --build . --config Release

cd ../../shipping
call neural_network_visualization.exe
PAUSE
