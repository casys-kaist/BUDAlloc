if [ ! -d "recutils" ]
then
    wget https://ftp.gnu.org/gnu/recutils/recutils-1.8.tar.gz
    tar -xvf recutils-1.8.tar.gz
    rm recutils-1.8.tar.gz
    mv recutils-1.8 recutils
fi

cd recutils
./configure
make -j