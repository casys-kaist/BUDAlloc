## Microbenchmarks
The goal of these experiments is to measure the microbench of sBPF.

## memory_perf.c
Usage: 
	--huge: use MAP_HUGETLB
	--random: read/write random (if not, sequential read/write is used)
	--mmap: using file mapped memory else anonymous memory is used

To use HUGETLB, you have to set /proc/sys/vm/nr_hugepages.

## vfork_perf.c
Usage: 
	--wait: Comparing fork and vfork with wait(). Default disabled.
	--return: Returns vfork_perf immediately. (Used for the internal usage only.)
