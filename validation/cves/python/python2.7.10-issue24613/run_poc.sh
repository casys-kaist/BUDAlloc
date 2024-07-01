RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
UAF_POINT="Modules/arraymodule.c:1403"

case $BENCH_TARGET in
  "BUDAlloc")
        COMM="LD_PRELOAD="libkernel.so" ./Python-2.7.10/python ./poc.py"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./Python-2.7.10/python ./poc.py"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./Python-2.7.10/python ./poc.py"
    ;;
  * )
        COMM="./Python-2.7.10/python ./poc.py"
    ;;
esac

sudo gdb -batch -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
if grep SIGSEGV ./test_output.txt >/dev/null || grep SIGABRT ./test_output.txt > /dev/null;  then 
  if grep "${UAF_POINT}" ./test_output.txt > /dev/null; then 
    echo "DETECT"
  else 
    echo "PREVENT"
  fi
else
  echo "VULNERABLE"
fi