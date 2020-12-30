#!/bin/sh

cd driver
./clean.sh
./build.sh
cd ..

mkdir -p build
cd build
cmake ..
make
cd -

