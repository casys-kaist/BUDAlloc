#DF
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
        COMM="LD_PRELOAD=libkernel.so ./giflib-code/util/gifsponge < ./poc.gif > /dev/null"
    ;;
  "ffmalloc")
        COMM="LD_PRELOAD=/home/babamba/ffmalloc/libffmallocnpmt.so bash ./giflib-code/util/gifsponge < ./poc.gif  > /dev/null"
    ;;
  "dangzero")
        COMM="LD_PRELOAD=/trusted/wrap.so  ./giflib-code/util/gifsponge < ./poc.gif > /dev/null"
    ;;
  * )
        COMM="bash ./giflib-code/util/gifsponge < ./poc.gif  > /dev/null"
    ;;
esac

# PREVENT when SEGFAULT happens
# DF case should cause Kernel PANIC by ota_free()

# mbpf_d RIP 100100001170
# ffmalloc RIP 561234f81170


# NOGDB
# Using gdb makes the execution stop. Using shell instead...
# sudo gdb -batch  -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
# sudo gdb -batch  -ex r -ex bt --args env ${COMM} > test_output.txt 2>&1
# sudo gdb -batch  -ex r -ex bt --args env ${COMM} > /dev/null 2>&1
sudo sh -c "${COMM}" > test_output.txt 2>&1
if grep "Segmentation fault" ./test_output.txt > /dev/null; then 
  echo "CANNOT_DETERMINE"
else
  echo "VULNERABLE"
fi