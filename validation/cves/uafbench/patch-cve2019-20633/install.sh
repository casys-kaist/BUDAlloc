if [ -d "patch" ]
then
  rm -r patch
fi

if [ ! -f "patch-2.7.6.tar.gz" ]
then
    wget https://git.savannah.gnu.org/cgit/patch.git/snapshot/patch-2.7.6.tar.gz
    tar -xvf patch-2.7.6.tar.gz
    rm patch-2.7.6.tar.gz
    mv patch-2.7.6 patch
fi

cd patch

if [ -d "gnulib" ]
then
  rm -r gnulib
fi

if [ ! -f "master.zip" ]
then
  wget https://github.com/coreutils/gnulib/archive/refs/heads/master.zip
fi
unzip master.zip

mv gnulib-master gnulib

patch -p1 -s < ../compatible.patch
chmod +x ./bootstrap
./bootstrap --no-git --gnulib-srcdir=./gnulib
./configure
make -j