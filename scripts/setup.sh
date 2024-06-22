#!/bin/bash

# Install clang-17
wget https://apt.llvm.org/llvm.sh
chmod 755 llvm.sh 
sudo ./llvm.sh 17
rm -rf llvm.sh
