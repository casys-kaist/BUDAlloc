#pragma once

long sbpf_syscall0(long n);
long sbpf_syscall1(long n, long a1);
long sbpf_syscall2(long n, long a1, long a2);
long sbpf_syscall3(long n, long a1, long a2, long a3);
long sbpf_syscall4(long n, long a1, long a2, long a3, long a4);
long sbpf_syscall5(long n, long a1, long a2, long a3, long a4, long a5);
long sbpf_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6);