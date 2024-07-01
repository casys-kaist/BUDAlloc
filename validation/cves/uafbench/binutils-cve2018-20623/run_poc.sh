
RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
UAF_POINT="elfcomm.c:43" # IF SIGSEGV occured at this line, that library detects UAF bug.

case $BENCH_TARGET in
  "BUDAlloc")
        COMM="LD_PRELOAD=libkernel.so ./binutils-gdb/binutils/readelf -a binutils-readelf-heap-use-after-free"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./binutils-gdb/binutils/readelf -a binutils-readelf-heap-use-after-free"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./binutils-gdb/binutils/readelf -a binutils-readelf-heap-use-after-free"
    ;;
  * )
        COMM="./binutils-gdb/binutils/readelf -a binutils-readelf-heap-use-after-free"
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