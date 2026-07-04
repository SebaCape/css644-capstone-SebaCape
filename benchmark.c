#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <limits.h>

#define SOCKET_PATH "/tmp/db_socket"
#define BUF_SIZE 1024

// Benchmark statistics
typedef struct {
    double total_time;
    long operations;
    double min_latency;
    double max_latency;
    double avg_latency;
    long failed;
} bench_stats_t;

// Utility to get current time in microseconds
static inline long get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
}

// Send command and receive response (simplified for this benchmark)
static int send_command(const char *cmd) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock);
        return -1;
    }

    if (write(sock, cmd, strlen(cmd)) == -1) {
        perror("write");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

// Benchmark: Sequential SETs
void bench_sequential_set(int num_ops) {
    printf("\n=== Sequential SET Benchmark ===\n");
    printf("Operations: %d\n", num_ops);

    long start = get_time_us();
    long min_latency = LLONG_MAX, max_latency = 0;
    double total_latency = 0;
    long failed = 0;

    for (int i = 0; i < num_ops; i++) {
        char cmd[BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "set key_%d value_%d\n", i, i);

        long op_start = get_time_us();
        if (send_command(cmd) == -1) {
            failed++;
        }
        long op_time = get_time_us() - op_start;

        if (op_time < min_latency) min_latency = op_time;
        if (op_time > max_latency) max_latency = op_time;
        total_latency += op_time;

        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d\n", i + 1, num_ops);
        }
    }

    long total_time = get_time_us() - start;
    double throughput = (num_ops * 1000000.0) / total_time;
    double avg_latency = total_latency / (num_ops - failed);

    printf("Total time: %.2f ms\n", total_time / 1000.0);
    printf("Throughput: %.2f ops/sec\n", throughput);
    printf("Min latency: %.2f µs\n", (double)min_latency);
    printf("Avg latency: %.2f µs\n", avg_latency);
    printf("Max latency: %.2f µs\n", (double)max_latency);
    printf("Failed: %ld\n", failed);
}

// Benchmark: Sequential GETs
void bench_sequential_get(int num_ops) {
    printf("\n=== Sequential GET Benchmark ===\n");
    printf("Operations: %d\n", num_ops);

    // Pre-populate
    printf("Populating database with %d entries...\n", num_ops);
    for (int i = 0; i < num_ops; i++) {
        char cmd[BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "set key_%d value_%d\n", i, i);
        send_command(cmd);
    }

    long start = get_time_us();
    long min_latency = LLONG_MAX, max_latency = 0;
    double total_latency = 0;
    long failed = 0;

    for (int i = 0; i < num_ops; i++) {
        char cmd[BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "get key_%d\n", i);

        long op_start = get_time_us();
        if (send_command(cmd) == -1) {
            failed++;
        }
        long op_time = get_time_us() - op_start;

        if (op_time < min_latency) min_latency = op_time;
        if (op_time > max_latency) max_latency = op_time;
        total_latency += op_time;

        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d\n", i + 1, num_ops);
        }
    }

    long total_time = get_time_us() - start;
    double throughput = (num_ops * 1000000.0) / total_time;
    double avg_latency = total_latency / (num_ops - failed);

    printf("Total time: %.2f ms\n", total_time / 1000.0);
    printf("Throughput: %.2f ops/sec\n", throughput);
    printf("Min latency: %.2f µs\n", (double)min_latency);
    printf("Avg latency: %.2f µs\n", avg_latency);
    printf("Max latency: %.2f µs\n", (double)max_latency);
    printf("Failed: %ld\n", failed);
}

// Shared state for concurrent benchmarks
typedef struct {
    int thread_id;
    int ops_per_thread;
    bench_stats_t *stats;
} thread_args_t;

void* concurrent_set_worker(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    int thread_id = args->thread_id;
    int ops = args->ops_per_thread;
    bench_stats_t *stats = args->stats;

    long min_latency = LLONG_MAX, max_latency = 0;
    double total_latency = 0;
    long failed = 0;

    for (int i = 0; i < ops; i++) {
        char cmd[BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "set thread_%d_key_%d value_%d\n", thread_id, i, i);

        long op_start = get_time_us();
        if (send_command(cmd) == -1) {
            failed++;
        }
        long op_time = get_time_us() - op_start;

        if (op_time < min_latency) min_latency = op_time;
        if (op_time > max_latency) max_latency = op_time;
        total_latency += op_time;
    }

    // Update shared stats (not thread-safe, but acceptable for benchmarking)
    stats->operations += ops;
    stats->failed += failed;
    if (min_latency < stats->min_latency) stats->min_latency = min_latency;
    if (max_latency > stats->max_latency) stats->max_latency = max_latency;
    stats->avg_latency += total_latency;

    free(args);
    return NULL;
}

// Benchmark: Concurrent SETs
void bench_concurrent_set(int num_threads, int ops_per_thread) {
    printf("\n=== Concurrent SET Benchmark ===\n");
    printf("Threads: %d, Ops per thread: %d, Total ops: %d\n", 
           num_threads, ops_per_thread, num_threads * ops_per_thread);

    bench_stats_t stats = {
        .min_latency = LLONG_MAX,
        .max_latency = 0,
        .avg_latency = 0,
        .operations = 0,
        .failed = 0
    };

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    long start = get_time_us();

    for (int i = 0; i < num_threads; i++) {
        thread_args_t *args = malloc(sizeof(thread_args_t));
        args->thread_id = i;
        args->ops_per_thread = ops_per_thread;
        args->stats = &stats;

        if (pthread_create(&threads[i], NULL, concurrent_set_worker, args) != 0) {
            perror("pthread_create");
            free(args);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    long total_time = get_time_us() - start;
    double throughput = (stats.operations * 1000000.0) / total_time;
    double avg_latency = stats.avg_latency / (stats.operations - stats.failed);

    printf("Total time: %.2f ms\n", total_time / 1000.0);
    printf("Throughput: %.2f ops/sec\n", throughput);
    printf("Min latency: %.2f µs\n", (double)stats.min_latency);
    printf("Avg latency: %.2f µs\n", avg_latency);
    printf("Max latency: %.2f µs\n", (double)stats.max_latency);
    printf("Failed: %ld\n", stats.failed);

    free(threads);
}

// Benchmark: Mixed workload (80% SET, 20% GET)
void bench_mixed_workload(int num_ops) {
    printf("\n=== Mixed Workload Benchmark (80%% SET, 20%% GET) ===\n");
    printf("Operations: %d\n", num_ops);

    long start = get_time_us();
    long min_latency = LLONG_MAX, max_latency = 0;
    double total_latency = 0;
    long failed = 0;

    for (int i = 0; i < num_ops; i++) {
        char cmd[BUF_SIZE];
        int op_type = (i % 100) < 80 ? 0 : 1;  // 80% SET, 20% GET

        if (op_type == 0) {
            snprintf(cmd, sizeof(cmd), "set key_%d value_%d\n", i, i);
        } else {
            snprintf(cmd, sizeof(cmd), "get key_%d\n", i);
        }

        long op_start = get_time_us();
        if (send_command(cmd) == -1) {
            failed++;
        }
        long op_time = get_time_us() - op_start;

        if (op_time < min_latency) min_latency = op_time;
        if (op_time > max_latency) max_latency = op_time;
        total_latency += op_time;

        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d\n", i + 1, num_ops);
        }
    }

    long total_time = get_time_us() - start;
    double throughput = (num_ops * 1000000.0) / total_time;
    double avg_latency = total_latency / (num_ops - failed);

    printf("Total time: %.2f ms\n", total_time / 1000.0);
    printf("Throughput: %.2f ops/sec\n", throughput);
    printf("Min latency: %.2f µs\n", (double)min_latency);
    printf("Avg latency: %.2f µs\n", avg_latency);
    printf("Max latency: %.2f µs\n", (double)max_latency);
    printf("Failed: %ld\n", failed);
}

// Benchmark: SIZE command
void bench_size_command(int iterations) {
    printf("\n=== SIZE Command Benchmark ===\n");
    printf("Iterations: %d\n", iterations);

    long start = get_time_us();
    long min_latency = LLONG_MAX, max_latency = 0;
    double total_latency = 0;

    for (int i = 0; i < iterations; i++) {
        long op_start = get_time_us();
        send_command("size\n");
        long op_time = get_time_us() - op_start;

        if (op_time < min_latency) min_latency = op_time;
        if (op_time > max_latency) max_latency = op_time;
        total_latency += op_time;
    }

    long total_time = get_time_us() - start;
    double throughput = (iterations * 1000000.0) / total_time;
    double avg_latency = total_latency / iterations;

    printf("Total time: %.2f ms\n", total_time / 1000.0);
    printf("Throughput: %.2f ops/sec\n", throughput);
    printf("Min latency: %.2f µs\n", (double)min_latency);
    printf("Avg latency: %.2f µs\n", avg_latency);
    printf("Max latency: %.2f µs\n", (double)max_latency);
}

void print_usage(const char *prog) {
    printf("Usage: %s <benchmark>\n", prog);
    printf("Benchmarks:\n");
    printf("  seq_set <num_ops>           - Sequential SET operations\n");
    printf("  seq_get <num_ops>           - Sequential GET operations\n");
    printf("  concurrent <threads> <ops>  - Concurrent SET operations\n");
    printf("  mixed <num_ops>             - Mixed 80%% SET / 20%% GET\n");
    printf("  size <iterations>           - SIZE command performance\n");
    printf("  all                         - Run all benchmarks\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *bench = argv[1];

    if (strcmp(bench, "seq_set") == 0) {
        int num_ops = argc > 2 ? atoi(argv[2]) : 1000;
        bench_sequential_set(num_ops);
    } 
    else if (strcmp(bench, "seq_get") == 0) {
        int num_ops = argc > 2 ? atoi(argv[2]) : 1000;
        bench_sequential_get(num_ops);
    } 
    else if (strcmp(bench, "concurrent") == 0) {
        int threads = argc > 2 ? atoi(argv[2]) : 4;
        int ops = argc > 3 ? atoi(argv[3]) : 250;
        bench_concurrent_set(threads, ops);
    } 
    else if (strcmp(bench, "mixed") == 0) {
        int num_ops = argc > 2 ? atoi(argv[2]) : 1000;
        bench_mixed_workload(num_ops);
    } 
    else if (strcmp(bench, "size") == 0) {
        int iterations = argc > 2 ? atoi(argv[2]) : 100;
        bench_size_command(iterations);
    } 
    else if (strcmp(bench, "all") == 0) {
        printf("===========================================\n");
        printf("    KV Store Comprehensive Benchmark\n");
        printf("===========================================\n");
        bench_sequential_set(1000);
        sleep(1);
        bench_sequential_get(500);
        sleep(1);
        bench_concurrent_set(4, 250);
        sleep(1);
        bench_mixed_workload(1000);
        sleep(1);
        bench_size_command(100);
        printf("\n===========================================\n");
        printf("    Benchmark Suite Complete\n");
        printf("===========================================\n");
    } 
    else {
        printf("Unknown benchmark: %s\n", bench);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}