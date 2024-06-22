RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
UAF_POINT="vm.c:1684"

case $BENCH_TARGET in
  "mbpf")
        COMM="LD_PRELOAD=libkernel.so ./mruby/bin/mruby ./poc.rb"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./mruby/bin/mruby ./poc.rb"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./mruby/bin/mruby ./poc.rb"
    ;;
  * )
        COMM="./mruby/bin/mruby ./poc.rb"
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