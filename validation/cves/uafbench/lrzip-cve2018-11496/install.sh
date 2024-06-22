
if [ ! -d "lrzip" ]
then
    git clone https://github.com/ckolivas/lrzip.git
fi


cd lrzip
git stash
git pull
git checkout ed51e14

# If build failed, move ltmain.sh to the lrzip folder and run ./autogen.sh && make -j again
./autogen.sh
cp ../ltmain.sh ltmain.sh
./autogen.sh

./configure
make -j
