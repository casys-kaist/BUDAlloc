#!/bin/bash

INSTALL_DIR=`realpath ./httpd`
DOCUMENT_ROOT=`realpath ./`

if [ ! -d "wrk" ]; then
	git clone https://github.com/wg/wrk.git
	cd wrk
	make -j
	cd ..
fi

if [ ! -d "httpd-2.4.58.tar.gz" ]; then
	wget https://archive.apache.org/dist/httpd/httpd-2.4.58.tar.gz
	tar zxf httpd-2.4.58.tar.gz
fi

# Install dependencies
sudo apt-get install build-essential libssl-dev libexpat-dev libpcre3-dev libapr1-dev libaprutil1-dev

rm -f *.tar.gz
cd httpd-2.4.58
./configure --prefix=$INSTALL_DIR
make -j
make install -j
cd ..
cp ./httpd/conf/httpd.conf ./
sed -i "s|DocumentRoot \".*\"|DocumentRoot \"$DOCUMENT_ROOT\"|" ./httpd.conf
sed -i "s|<Directory \".*\">|<Directory \"$DOCUMENT_ROOT\">|" ./httpd.conf


# make -j
# sudo make install
