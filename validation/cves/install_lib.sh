#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run with sudo permission"
  exit
fi

# exploit_database/nginx-cve2020-24346
sudo apt-get install libpcre3-dev libpcre++-dev -y

# uafbench/binutils-cve2018
sudo apt-get install zip -y

#uafbench/giflib-bug-74
sudo apt-get install xmlto imagemagick -y

#uafbench/jpegoptim-cve
sudo apt-get install libjpeg-dev -y

#uafbench/lrzip-cve2018-11496
sudo apt-get install libz-dev libbz2-dev liblzo2-dev liblz4-dev -y

# uafbench/patch-cve2019-20633
sudo apt-get install gnulib -y

# php/8.3.0-issue-10582
sudo apt-get install sqlite3

# mruby/*
sudo apt-get install rake

#exploitdatabase/libredwg
sudo apt-get install texinfo

#exploitdatabase/lua
sudo apt-get install rcs