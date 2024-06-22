# CVE-testing for the libkernel and memory allocator

## Getting Started

### Test Installation
It takes about 20 minutes to finish installation.
The installation script runs in parallel.
```bash
# install required apt package. Need sudo privilege
$ sudo ./install_lib.sh  

# install cve benches
$ make
# If program stops while running, build CVEs in serial
$ make build_serial
```
### Running Test
**It requires sudo privilege to run mbpf or ffmalloc**
check the `results` directory. You can check the result at result.csv
```bash
$ make run
$ vim result.csv   # check the result
```

### How we determined Detection/Prevention
In CVEs, there are several programs that has UAF bug and DF(Double Free) Bug.

For UAF bug, we checked the RIP of the segfault location. We also used address sanitizer to check RIP of the fault location, and verified BUDAlloc with Detection mode detected all the UAF bug.
For other systems(BUDAlloc with Prevention, FFmalloc), we compared the RIP of the fault. If it is same with BUDAlloc with Detection, we marked as `Detection`. If not, we marked as `Prevention` because it anyway prevented malicious behavior during the execution.

For DF bug, if BUDAlloc_d, BUDAlloc_p, FFmalloc detects double free(Kernel Panic at __ota_free() for BUDAlloc, bad ptr free for FFmalloc), we marked as `Detection`.


### For the result marked as CANNOT_DETERMINE
Because our script is made for **automating** classification based on program output, it cannot classify results if Use-After-Free bug is occured silently (which can be only detected by AddressSanitizer), crash randomly happened, etc..  
For these cases, we implicitly mark result as **CANNOT_DETERMINE** and write the reason in each CVE's README.   
If you want to precisely classify the result of that cases, please build program with ASAN, or use other methods to check it.

### Cleanup
It will remove all programs installed in the cves directory. 
```bash
$ make clean
```