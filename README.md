# USENIX Security 2024 BUDAlloc: Defeating Use-After-Free Bugs by Decoupling Virtual Address
Junho Ahn, Jaehyeon Lee, Kanghyuk Lee, Wooseok Gwak, Minseong Hwang, and Youngjin Kwon (School of Computing, KAIST)

The paper is available in [here](https://www.usenix.org/conference/usenixsecurity24/presentation/ahn).

* If you encounter any issues while running our benchmarks, please feel free to contact the authors at junhoahn@kaist.ac.kr.

## Abstraction
Use-after-free bugs are an important class of vulnerabilities that often pose serious security threats. To prevent or detect use-after-free bugs, one-time allocators have recently gained attention for their better performance scalability and immediate detection of use-after-free bugs compared to garbage collection approaches. This paper introduces BUDAlloc, a one-time-allocator for detecting and protecting use-after-free bugs in unmodified binaries. The core idea is co-designing a user-level allocator and kernel by separating virtual and physical address management. The user-level allocator manages virtual address layout, eliminating the need for system calls when creating virtual aliases, which is essential for reducing internal fragmentation caused by the one-time-allocator. BUDAlloc customizes the kernel page fault handler with eBPF for batching unmap requests when freeing objects. In SPEC CPU 2017, BUDAlloc achieves a 15% performance improvement over DangZero and reduces memory overhead by 61% compared to FFmalloc.

# BUDAlloc Artifact Evaluation
## Artifact Appendix
### Abstract
This artifact evaluation introduces the source code, runtime setup, and instructions to reproduce the results of the BUDAlloc evaluations. We evaluate BUDAlloc in terms of security, performance, and memory. For security evaluation, we perform CVE analysis, NIST Juliet test suite and HardsHeap. For performance evaluation, we evaluate BUDAlloc with SPEC CPU 2006, SPEC CPU 2017, PASEC 3.0, Apache, and Nginx Webserver. This artifact demonstrates that BUDAlloc can successfully prevent and detect use-after-free (UAF) bugs with minimal impact on performance and memory usage.

### Description & Requirements
#### Security, privacy, and ethical concerns
There are no ethical concerns associated with BUDAlloc. The source code is released under the MIT license.

#### How to Access
This artifact evaluation can be accessed via the following stable URL: [github](https://github.com/casys-kaist/BUDAlloc/tree/main).

#### Hardware Dependencies
We tested BUDAlloc with Intel(R) Xeon(R) Gold 5220R CPU at 2.2GHz with 24 cores, 172GB DRAM - 2666 MHZ, 512 GB SSD, and 10-Gigabit Network Connection. In all the experiments, we disable hyper-threading, CPU power-saving states, and frequency scaling to reduce the variance. We use Non-Uniform Memory Access (NUMA) in the PARSEC 3.0 benchmarks to fully utilize all 48 cores in the motherboard. We use time to get the resident set size (RSS) and total execution time except DangZero.

#### Software Dependencies
To support atomic operations in the BPF program, BUDAlloc requires the installation of clang-17. This can be done using the `scripts/setup.sh` script. Our setup utilizes Ubuntu 20.04 with GCC version 9.4.0. If using a newer version of GCC, the `-fcommon` and `-Wno-implicit-function-declaration` compiler options are necessary. We use default configurations for other memory allocators in all evaluations. For testing on DangZero, we use a virtual machine with KVM, as this is the default method for running Kernel-Mode-Linux in DangZero.

### Set-up
#### BUDAlloc Installation
BUDAlloc consists of two distinct components: the kernel and the user space. The BUDAlloc kernel includes the necessary kernel patches for the eBPF helper functions and custom page fault handler. The BUDAlloc user space contains both the user-level components and the eBPF custom page fault handler.

##### BUDAlloc-Kernel installation
First, you need to install the BUDAlloc Kernel.
1. Clone the BUDAlloc Kernel repository:
   ```bash
   git clone https://github.com/casys-kaist/BUDAlloc
   ```
2. Get submodules and update. This will clone BUDAlloc-Kernel repository.
   ```bash
   cd BUDAlloc
   git submodule init
   git submodule update
   ```
3. Build and install the kernel:
   ```bash
   cd BUDAlloc-Kernel
   make -j$(nproc)
   sudo make -j$(nproc) INSTALL_MOD_STRIP=1 modules_install
   sudo make install
   ```
4. Reboot your system.
    
After rebooting, install the libbpf library:
1. Navigate to the libbpf directory and build the library:
   ```bash
   cd BUDAlloc-Kernel/tools/lib/bpf
   make -j$(nproc)
   sudo make install
   ```
2. Install the kernel header files:
   ```bash
   cd BUDAlloc-Kernel
   sudo make headers_install INSTALL_HDR_PATH=/usr
   ```

Currently, the default path for libbpf is `/usr/local/lib64`. To enable linking, add this path to the linker configuration:
1. Open the linker configuration file:
   ```bash
   sudo vi /etc/ld.so.conf.d/99.conf
   ```
2. Add the following line to `99.conf`:
   ```bash
   /usr/local/lib64
   ```
3. Update the linker cache:
   ```bash
   sudo ldconfig
   ```

##### BUDAlloc-User installation
After installing the kernel, you can build the BUDAlloc user part.
1. Move to the BUDAlloc repository:
   ```bash
   cd BUDAlloc
   ```
2. Install the Clang-17 compiler:
	```bash
	./scripts/setup.sh
	```
3. Build and install the user components:
   ```bash
   make -j$(nproc)
   sudo make install
   ```
   Default build is **BUDAlloc-p**(prevent) mode.  
   To build **BUDAlloc-d(detect)** mode, follow below instruction.
   ```bash
   vim libkernel/include/kconfig.h
   # comment out #define CONFIG_BATCHED_FREE, #define CONFIG_ADOPTIVE_BATCHED_FREE
   make -j$(nproc)
   sudo make install
   ```
   
##### To run the unit tests:
1. Execute the unit tests:
   ```bash
   make unit_test
   ```

#### Installation steps for the DangZero, FFmalloc, and MarkUs
##### 1. FFmalloc
1. Install the FFmalloc shared library:
   ```bash
   git clone git@github.com:bwickman97/ffmalloc.git
   cd ffmalloc
   make sharednpmt -j
   ```

##### 2. MarkUs
1. Install the MarkUs shared library:
   ```bash
   git clone git@github.com:SamAinsworth/MarkUs-sp2020.git
   cd MarkUs-sp2020
   ./setup.sh
   ```

##### 3. DangZero
**Note: Start this in a VM.**  
  
1. Install the DangZero in a VM:  
  
   **1-1: For your convenience, we will provide a pre-installed VM.**  
   > You can use the `disk_dangzero.img` file located in your home directory, which DangZero is pre-installed.  
   > To boot your VM with `disk_dangzero.img`, run:

   > ```bash
   > ./run_qemu.sh
   > ```

   > Then, select the kernel `4.0.0-kml` from the GRUB configuration menu.  
   
     
   **1-2:  If you want to install Dangzero in your own VM, please follow the below line.**  
   You can find more information in https://github.com/vusec/dangzero
   > ```bash
   > git clone https://github.com/vusec/dangzero.git
   > cd dangzero/kml-image
   > chmod +x ./build_kml.sh
   > ./build_kml.sh
   > sudo dpkg -i *.deb
   > # Reboot and select the kernel 4.0.0-kml 
   > ```

2. Install the DangZero kernel module:
   ```bash
   cd kmod
   make
   sudo insmod dangmod.ko
   cd ..
   ```

3. Note on the memory overhead of DangZero. DangZero cannot account for the memory usage with the default scripts. Unlike other benchmarks, to get a resident set size, you have to add the additional value by uncommenting the TRACK_MEM_USAGE option at gc.h in the DangZero root before installing DangZero.

### Evaluation workflow
#### Major Claims
We evaluated the ***performance***, ***memory overhead***, and ***bug detectability*** of BUDAlloc compared to recent OTAs, FFmalloc, DangZero, and the GC-based MarkUs. In SPEC CPU 2006, BUDAlloc outperformed DangZero by 5% in full detection mode and by 15% in prevention mode, with a memory overhead of 30% compared to FFmalloc's 207%, and better bug detectability. BUDAlloc showed scalable performance improvements in multithreaded PARSEC 3.0, surpassing FFmalloc with more than 8 threads. Real-world tests with Nginx and Apache demonstrated performance and memory overhead comparable to GLIBC, without scalability issues. BUDAlloc detected 27 out of 30 use-after-free vulnerabilities from recent CVEs, and passed all robustness tests with Fuzzer and NIST Juliet, with no issues found in HardsHeap after 24 hours.

1. BUDAlloc should demonstrate acceptable performance and memory overhead on single-thread benchmarks such as SPEC CPU 2006 and SPEC CPU 2017.
2. BUDAlloc should show scalable performance improvements on multi-thread benchmarks such as PARSEC 3.0.
3. In prevention mode, BUDAlloc should successfully prevent all use-after-free and double-free bugs in the Juliet, HardsHeap, and CVE corpus.
4. In detection mode, BUDAlloc should detect all use-after-free and double-free bugs in the CVE sets.

#### Experiments
All results will be stored in `macrobench/result/{test_name}`.

##### 0. Preliminary Step
Before running the script, we recommend extending the sudo authentication timeout:
   ```bash
   sudo visudo
   # Add the line "Defaults:<User_name> timestamp_timeout=600"
   sudo -k
   ```   
**Additionally, if you are testing DangZero, you should set `--LIBCS=dangzero` in each benchmark script.**
   ```bash
   ./bench_xxx.sh --LIBCS=dangzero
   ```
  
In each script, you can set options for your own sake. Available options are as follows: 
* `--LIBCS`: Set library(s) to run. Default value is "glibc BUDAlloc ffmalloc markus".
* `--TASKSET`: Set thread number of taskset command. Default value is 19. which is required to bind the core and reduce fluctuation. This also limits the additional CPU resources consumed by MarkUs’s GC thread, unlike other test cases
* `--THREADS`: **[Only for PARSEC 3.0]** Set thread numbers to run. Default value is "1 2 4 8 16 32"
* `--CONNECTIONS`: **[Only for Apache2 and Nginx]** Set connection number of benchmark. Default value is "100 200 400 800"
* `--BENCH_SEC`: **[Only for Apache2 and Nginx]** Set the connection time for benchmark. Default value is 30

##### 1. SPEC2006
Before starting, you should obtain and install SPEC2006 in the /home/{USER} directory.
   ```bash
   tar -xvf cpu2006.tar.gz -C /home/{USER}
   mv /home/{USER}/cpu2006 /home/{USER}/SPECCPU_2006
   cd /home/{USER}/SPECCPU_2006
   ./install.sh
   ```

After installing the SPEC2006, you can execute `./bench_sepc2006.sh` to run SPEC CPU 2006 benchmarks. 
```bash
cd macrobench/spec2006
./bench_spec2006.sh [--LIBCS=value] [--TASKSET=value] # This will take a long time to complete.
```

##### 2. SPEC2017
Before starting, you should obtain and install SPEC2017 in the /home/{USER} directory.
   ```bash
   mount -t iso9660 -o ro,exec,loop cpu2017.iso /mnt
   cd /mnt
   ./install.sh
   ```

After installing SPEC2017, you can execute ./bench_spec2017.sh to run the SPEC CPU 2017 benchmarks.
   ```bash
   cd macrobench/spec2017
   ./bench_spec2017.sh [--LIBCS=value] [--TASKSET=value]   # This will take a long time to complete
   ```

##### 3. Parsec
Installing PARSEC 3.0.
   ```bash
   cd macrobench/parsec
   sudo ./install.sh

   # If you encounter issues while running install.sh, repeat the following steps until successful:
   cd parsec-benchmark
   sudo rm -rf parsec-3.0*
   cd ../
   sudo ./install.sh
   ```
   
After installing PARSEC, you can execute `./bench_parsec_threads.sh` to run the PARSEC 3.0 benchmarks.
   ```bash
   sudo ./bench_parsec_threads.sh [--LIBCS=value] [--THREADS=value]
   ```

##### 4. Apache
   ```bash
   cd macrobench/apache2
   ./install.sh
   sudo ./bench_apache2.sh [--LIBCS=value] [--CONNECTIONS=value] [--THREADS=value (default 16)] [--BENCH_SEC=value (default 30)]

   # If scripts don’t run as intended, you can manually run host and client on each server.
   # Host
   cd macrobench/common
   sudo ./run_apache_nginx_server.sh <apache2/nginx> <library> <num_threads> "No"

   # Client
   cd macrobench/common
   sudo ./run_apache_nginx_client.sh <apache2/nginx> <Num_connections> <num_threads> <BENCH_SEC> <library>
   ```

##### 5. Nginx
   ```bash
   cd macrobench/nginx
   ./install.sh
   sudo ./bench_nginx.sh [--LIBCS=value] [--CONNECTIONS=value] [--THREADS=value (default 16)] [--BENCH_SEC=value (default 30)]
   ```

##### 6. CVES
   ```bash
   cd validation/cves
   sudo ./install.sh
   make
   # If make stops, Please use this command. This will build programs sequently
   make build_serial

   make run
   vim result.csv   # To check the result
   ```

### Version
Based on the LaTeX template for Artifact Evaluation V20231005. Submission, reviewing and badging methodology followed for the evaluation of this artifact can be found at https://secartifacts.github.io/usenixsec2024/.

## Contributors
1. Junho Ahn (junhoahn@kaist.ac.kr)
2. KangHyuk Lee (babamba@kaist.ac.kr)
3. Minseong Hwang (hwmin414@kaist.ac.kr)
4. Jinho Choi (choijinho817@kaist.ac.kr)
