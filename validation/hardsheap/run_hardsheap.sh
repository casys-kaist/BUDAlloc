#!/bin/bash

mkdir -p HardsHeap/artifact/secure-allocators/budalloc/
cp run.sh HardsHeap/artifact/secure-allocators/budalloc/

cd ./HardsHeap/
./build.sh
./setup.sh
cd artifact
./run.py -r $(pwd)/../ -o output -a budalloc

