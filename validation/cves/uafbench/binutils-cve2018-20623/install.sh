if [ -d "binutils-gdb" ] ;
then
  rm -r binutils-gdb
fi

if [ ! -f "binutils-2_31-branch.zip" ]
then
  wget "https://github.com/bminor/binutils-gdb/archive/refs/heads/binutils-2_31-branch.zip"
fi

unzip binutils-2_31-branch.zip
mv binutils-gdb-binutils-2_31-branch binutils-gdb

cd binutils-gdb

./configure
make -j

# FIXME
# build err

# $PWD/../gcc-7.4.0/configure --prefix=$HOME/GCC-7.4.0 --enable-languages=c,c++ --disable-multilib
# make -j