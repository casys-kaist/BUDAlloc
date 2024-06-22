#!/bin/bash

if [ ! -d "pcre2-10.42" ]; then
	wget github.com/PCRE2Project/pcre2/releases/download/pcre2-10.42/pcre2-10.42.tar.gz
	tar -zxf pcre2-10.42.tar.gz
	cd pcre2-10.42
	./configure
	make -j
	sudo make install
	cd ..
fi

if [ ! -d "zlib-1.3.1" ]; then
	wget http://zlib.net/zlib-1.3.1.tar.gz
	tar -zxf zlib-1.3.1.tar.gz
	cd zlib-1.3.1
	./configure
	make -j
	sudo make install
	cd ..
fi

if [ ! -d "openssl-1.1.1v" ]; then
	wget http://www.openssl.org/source/openssl-1.1.1v.tar.gz
	tar -zxf openssl-1.1.1v.tar.gz
	cd openssl-1.1.1v
	./Configure darwin64-x86_64-cc --prefix=/usr
	make -j
	sudo make install
	cd ..
fi

if [ ! -d "nginx-1.24.0" ]; then
	wget https://nginx.org/download/nginx-1.24.0.tar.gz
	tar zxf nginx-1.24.0.tar.gz
fi

if [ ! -d "wrk" ]; then
	git clone https://github.com/wg/wrk.git
	cd wrk
	make -j
	cd ..
fi

rm -f *.tar.gz
cd nginx-1.24.0

./configure \
	--sbin-path=/usr/local/nginx/nginx \
	--conf-path=/usr/local/nginx/nginx.conf \
	--pid-path=/usr/local/nginx/nginx.pid \
	--with-pcre=../pcre2-10.42 \
	--with-zlib=../zlib-1.3.1 \
	--with-http_ssl_module \
	--with-stream \
	--with-threads

make -j
sudo make install
