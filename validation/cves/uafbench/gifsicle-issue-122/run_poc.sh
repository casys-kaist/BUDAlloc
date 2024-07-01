# DF Case

RED="\e[31m"
GREEN="\e[32m"
PURPLE="\e[35m"
ENDCOLOR="\e[0m"

BENCH_TARGET=$1
COMM=""
DF_MSG1="double free"
DF_MSG2="free bad"

case $BENCH_TARGET in
  "BUDAlloc")
        COMM="LD_PRELOAD=libkernel.so ./gifsicle/src/gifsicle ./poc test.gif"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./gifsicle/src/gifsicle ./poc test.gif"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./gifsicle/src/gifsicle ./poc test.gif"
    ;;
  * )
        COMM="./gifsicle/src/gifsicle ./poc test.gif"
    ;;
esac

sudo gdb -batch  -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
if grep "${DF_MSG1}" ./test_output.txt >/dev/null || grep "${DF_MSG2}" ./test_output.txt >/dev/null ; then 
  echo "DETECT"
else
  echo "VULNERABLE"
fi