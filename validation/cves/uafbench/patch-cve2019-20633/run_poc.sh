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
        COMM="LD_PRELOAD=libkernel.so ./patch/src/patch -p1 -Rf < ./poc"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so ./patch/src/patch -p1 -Rf < ./poc"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so ./patch/src/patch -p1 -Rf < ./poc"
    ;;
  * )
        COMM="./patch/src/patch -p1 -Rf < ./poc"
    ;;
esac

# NOGDB
# gdb doesn't catch anything. Using shell command instead...
# sudo gdb -batch -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
sudo sh -c "${COMM}" > test_output.txt 2>&1
if grep "${DF_MSG1}" ./test_output.txt >/dev/null || grep "${DF_MSG2}" ./test_output.txt >/dev/null ; then 
  echo "DETECT"
else
  echo "VULNERABLE"
fi