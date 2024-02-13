#!/bin/bash

mkdir -p build
pushd build
%1 -G Ninja -S .. -B .
cmake --build .
popd
