#!/bin/sh

cd driver
./clean.sh
make && cd ..
./autogen.sh
./configure && make


