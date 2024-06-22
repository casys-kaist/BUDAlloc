if [ ! -d "mjs" ] 
then
    git clone https://github.com/cesanta/mjs.git
fi

cd mjs
git checkout e4ea33a
gcc -DMJS_MAIN mjs.c -g -ldl -o mjs-bin