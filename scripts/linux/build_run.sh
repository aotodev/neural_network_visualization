#!/bin/bash
#...

export CXX=/bin/g++
export C=/bin/gcc

mkdir -p ../../build/linux/build_files/shipping
cd ../../build/linux/build_files/shipping

cmake -DBUILD_DEBUG=OFF -DBUILD_RELEASE=OFF -DBUILD_SHIPPING=ON -DUSE_WAYLAND=OFF ../../../../
cmake --build . --config Release

cd ../../shipping
chmod +x neural_network_visualization
./neural_network_visualization

read -rsp $'Press escape to continue...\n' -d $'\e'
