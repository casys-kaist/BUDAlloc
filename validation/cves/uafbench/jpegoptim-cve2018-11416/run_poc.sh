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
  "mbpf")
        COMM="LD_PRELOAD=libkernel.so ./jpegoptim/jpegoptim ./crash_file.jpg"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./jpegoptim/jpegoptim ./crash_file.jpg"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./jpegoptim/jpegoptim ./crash_file.jpg"
    ;;
  * )
        COMM="./jpegoptim/jpegoptim ./crash_file.jpg"
    ;;
esac

sudo gdb -batch -ex "set follows-fork-mode auto" -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
if grep "${DF_MSG1}" ./test_output.txt >/dev/null || grep "${DF_MSG2}" ./test_output.txt >/dev/null ; then 
  echo "DETECT"
else
  echo "VULNERABLE"
fi