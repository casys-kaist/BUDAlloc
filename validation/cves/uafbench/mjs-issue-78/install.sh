if [ ! -d "mjs" ] 
then
    git clone https://github.com/cesanta/mjs.git
fi

cd mjs
git checkout 9eae0e6
gcc -DMJS_MAIN mjs.c -ldl -g -o mjs-bin