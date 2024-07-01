RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
UAF_POINT="stream.c:1756"

case $BENCH_TARGET in
  "BUDAlloc")
        COMM="LD_PRELOAD=libkernel.so ./lrzip/lrzip -t lrzip-uaf-read_stream.lrz"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./lrzip/lrzip -t lrzip-uaf-read_stream.lrz"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./lrzip/lrzip -t lrzip-uaf-read_stream.lrz"
    ;;
  * )
        COMM="./lrzip/lrzip -t lrzip-uaf-read_stream.lrz"
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