if [ ! -d "gifsicle" ] 
then
    git clone https://github.com/kohler/gifsicle.git
fi

cd gifsicle
git checkout fad477c
./bootstrap.sh
./configure
make clean
make