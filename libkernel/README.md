## Libkernel
Libkernel is an operating system library designed specifically for containers. 
It serves as a lightweight operating system, offering adaptable interfaces between
a full-stack LibOS (Library OS) and a more relaxed Library OS with eBPF (Extended Berkeley Packet Filter) support.

## BUILD
To build the libkernel, you have to install external libraries.
Those libraries are elfutils (required for libbpf), libbpf and libopcode.

Currently, libopcode and libbpf is given by object files. We will fix this.
To build the elfutils, you have to execute make build_extlibs.

## Things to note
### bpf objects
To include additional bpf objects, it is necessary to append ".bpf" to their names.
As an example, the file "memory_target.bpf.c" will be compiled using clangd with -bpf,
ensuring that BPF is not linked in the libkernel.so.
