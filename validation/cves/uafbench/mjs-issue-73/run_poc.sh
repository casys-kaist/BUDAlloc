RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
# UAF_POINT="mjs.c:14031"

case $BENCH_TARGET in
  "mbpf")
        COMM="LD_PRELOAD=libkernel.so ./mjs/mjs-bin -f ./poc.js"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./mjs/mjs-bin -f ./poc.js"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./mjs/mjs-bin -f ./poc.js"
    ;;
  * )
        COMM="./mjs/mjs-bin -f ./poc.js"
    ;;
esac

sudo gdb -batch -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
if grep SIGSEGV ./test_output.txt >/dev/null || grep SIGABRT ./test_output.txt > /dev/null;  then 
  echo "DETECT"
else
    echo "PREVENT"
fi