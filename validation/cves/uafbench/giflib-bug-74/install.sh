if [ ! -d "giflib-code" ] 
then
    git clone git://git.code.sf.net/p/giflib/code giflib-code
fi


cp ltmain.sh giflib-code/ltmain.sh

cd giflib-code
git checkout 72e31ff

# if not work, put ltmain.sh into code folder

./autogen.sh
./configure
make -j