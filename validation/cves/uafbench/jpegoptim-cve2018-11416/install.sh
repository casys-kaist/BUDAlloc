if [ ! -d "jpegoptim" ]
then 
    git clone https://github.com/tjko/jpegoptim.git
fi

cd jpegoptim
git stash
git pull
git checkout d23abf2

./configure
make -j