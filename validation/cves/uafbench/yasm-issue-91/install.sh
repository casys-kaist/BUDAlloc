if [ ! -d "yasm" ] 
then
    git clone https://github.com/yasm/yasm.git
fi

cd yasm
git checkout 6caf151
./autogen.sh
./configure
make clean
make -j