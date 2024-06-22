#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run with sudo permission"
  exit
fi

if [ ! -d "parsec-benchmark" ]; then
	git clone https://github.com/cirosantilli/parsec-benchmark.git
fi

cd parsec-benchmark
./configure
./get-inputs -n
. ./env.sh
sudo apt-get install -y gettext
patch -p1 < ../optimize_off.patch
parsecmgmt -a build -p x264
patch -R -p1 < ../optimize_off.patch
parsecmgmt -a build -p all