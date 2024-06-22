#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

void measure_atomic_fetch_add(volatile int *counter, int iterations, int memory_order, double tsc_frequency_ghz, const char* order_name) {
    uint64_t start, end;

    // Warm-up
    for (int i = 0; i < 10000; ++i) {
        __atomic_fetch_add(counter, 1, memory_order);
    }

    // Measurement
    start = rdtsc();
    for (int i = 0; i < iterations; ++i) {
        __atomic_fetch_add(counter, 1, memory_order);
    }
    end = rdtsc();

    printf("Average time for __atomic_fetch_add (%s): %f nanoseconds\n", order_name, ((double)(end - start) / iterations) / tsc_frequency_ghz);
    printf("Total time for __atomic_fetch_add (%s): %f seconds\n", order_name, ((double)(end - start)) / tsc_frequency_ghz / 1e9);
}

void measure_atomic_compare_exchange(volatile int *counter, int iterations, int memory_order, double tsc_frequency_ghz, const char* order_name) {
    uint64_t start, end;
    int expected, desired;

    // Warm-up
    for (int i = 0; i < 10000; ++i) {
        expected = *counter;
        desired = expected + 1;
        __atomic_compare_exchange(counter, &expected, &desired, false, memory_order, memory_order);
    }

    // Measurement
    start = rdtsc();
    for (int i = 0; i < iterations; ++i) {
        expected = *counter;
        desired = expected + 1;
        __atomic_compare_exchange(counter, &expected, &desired, false, memory_order, memory_order);
    }
    end = rdtsc();

    printf("Average time for __atomic_compare_exchange (%s): %f nanoseconds\n", order_name, ((double)(end - start) / iterations) / tsc_frequency_ghz);
    printf("Total time for __atomic_compare_exchange (%s): %f seconds\n", order_name, ((double)(end - start)) / tsc_frequency_ghz / 1e9);
}

void measure_atomic_exchange(volatile int *counter, int iterations, int memory_order, double tsc_frequency_ghz, const char* order_name) {
    uint64_t start, end;
    int value = 0;

    // Warm-up
    for (int i = 0; i < 10000; ++i) {
        __atomic_exchange(counter, &value, &value, memory_order);
    }

    // Measurement
    start = rdtsc();
    for (int i = 0; i < iterations; ++i) {
        __atomic_exchange(counter, &value, &value, memory_order);
    }
    end = rdtsc();

    printf("Average time for __atomic_exchange (%s): %f nanoseconds\n", order_name, ((double)(end - start) / iterations) / tsc_frequency_ghz);
    printf("Total time for __atomic_exchange (%s): %f seconds\n", order_name, ((double)(end - start)) / tsc_frequency_ghz / 1e9);
}

void measure_atomic_thread_fence(int iterations, int memory_order, double tsc_frequency_ghz, const char* order_name) {
    uint64_t start, end;

    // Warm-up
    for (int i = 0; i < 10000; ++i) {
        __atomic_thread_fence(memory_order);
    }

    // Measurement
    start = rdtsc();
    for (int i = 0; i < iterations; ++i) {
        __atomic_thread_fence(memory_order);
    }
    end = rdtsc();

    printf("Average time for __atomic_thread_fence (%s): %f nanoseconds\n", order_name, ((double)(end - start) / iterations) / tsc_frequency_ghz);
    printf("Total time for __atomic_thread_fence (%s): %f seconds\n", order_name, ((double)(end - start)) / tsc_frequency_ghz / 1e9);
}

int main() {
    volatile int counter = 0;
    int iterations = 600000000; // Number of iterations for averaging
    double tsc_frequency_ghz = 2.2; // TSC frequency in GHz

    // Measure __atomic_fetch_add for different memory orders
    measure_atomic_fetch_add(&counter, iterations, __ATOMIC_RELAXED, tsc_frequency_ghz, "RELAXED");
    measure_atomic_fetch_add(&counter, iterations, __ATOMIC_ACQ_REL, tsc_frequency_ghz, "ACQ_REL");
    measure_atomic_fetch_add(&counter, iterations, __ATOMIC_SEQ_CST, tsc_frequency_ghz, "SEQ_CST");

    // Measure __atomic_compare_exchange for different memory orders
    measure_atomic_compare_exchange(&counter, iterations, __ATOMIC_RELAXED, tsc_frequency_ghz, "RELAXED");
    measure_atomic_compare_exchange(&counter, iterations, __ATOMIC_ACQ_REL, tsc_frequency_ghz, "ACQ_REL");
    measure_atomic_compare_exchange(&counter, iterations, __ATOMIC_SEQ_CST, tsc_frequency_ghz, "SEQ_CST");

    // Measure __atomic_exchange for different memory orders
    measure_atomic_exchange(&counter, iterations, __ATOMIC_RELAXED, tsc_frequency_ghz, "RELAXED");
    measure_atomic_exchange(&counter, iterations, __ATOMIC_ACQ_REL, tsc_frequency_ghz, "ACQ_REL");
    measure_atomic_exchange(&counter, iterations, __ATOMIC_SEQ_CST, tsc_frequency_ghz, "SEQ_CST");

    // Measure __atomic_thread_fence for different memory orders
    measure_atomic_thread_fence(iterations, __ATOMIC_RELAXED, tsc_frequency_ghz, "RELAXED");
    measure_atomic_thread_fence(iterations, __ATOMIC_ACQ_REL, tsc_frequency_ghz, "ACQ_REL");
    measure_atomic_thread_fence(iterations, __ATOMIC_SEQ_CST, tsc_frequency_ghz, "SEQ_CST");

    return 0;
}
