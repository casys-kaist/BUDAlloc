RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
UAF_POINT="ext/libxml/libxml.c:1265"

case $BENCH_TARGET in
  "BUDAlloc")
        COMM="USE_ZEND_ALLOC=0 LD_PRELOAD="libkernel.so" ./php-7.4.0/sapi/cli/php -f ./poc.php"
    ;;
  "ffmalloc")
        COMM="USE_ZEND_ALLOC=0 LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./php-7.4.0/sapi/cli/php -f ./poc.php"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so USE_ZEND_ALLOC=0 ./php-7.4.0/sapi/cli/php -f ./poc.php"
    ;;
  * )
        COMM="USE_ZEND_ALLOC=0 ./php-7.4.0/sapi/cli/php -f ./poc.php"
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