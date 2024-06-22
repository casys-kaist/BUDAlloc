# https://bugs.python.org/issue24613

if [ ! -d "../cpython" ]; then
    wget https://www.python.org/ftp/python/2.7.10/Python-2.7.10.tar.xz
    tar -xf Python-2.7.10.tar.xz
    rm Python-2.7.10.tar.xz
fi

cd Python-2.7.10
./configure
make -j


