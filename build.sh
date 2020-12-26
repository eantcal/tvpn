#!/bin/sh

cd driver
./clean.sh
make && cd ..

mkdir -p build
cd build
cmake ..
make
cd -

