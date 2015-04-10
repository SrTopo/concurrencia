#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>

#define NUM_THREADS      4
#define MAX_COUNT 10000000

// Just used to send the index of the id
struct tdata {
    int tid;
};

/* Simple fair FUTEX mutex implementation */
typedef struct simple_futex {
    unsigned number;
    unsigned turn;
    unsigned current;
} simple_futex;

void lock(simple_futex *futex) {
    unsigned number = __atomic_fetch_add(&futex->number, 1, __ATOMIC_RELAXED);
    unsigned turn;

    int tries = 0;
    while (number != futex->turn && tries < 100) {
        tries++;
        sched_yield();
    }

    turn = futex->turn;
    while (number != (turn = futex->turn)) {
        syscall(__NR_futex, &futex->turn, FUTEX_WAIT, turn, NULL, 0, 0);
        turn = futex->turn;
    }
    futex->current = number;
}

void unlock(simple_futex *futex) {
    int current = __atomic_add_fetch(&futex->turn, 1, __ATOMIC_RELEASE);
    int tries = 0;

    while (current > futex->current && tries < 100) {
        tries++;
    }

    if (current > futex->current) {
        syscall(__NR_futex, &futex->turn, FUTEX_WAKE, INT_MAX, NULL, 0, 0);
    }
}
/* END FUTEX */

simple_futex mutex;
int counter = 0;

void *count(void *ptr) {
    long i, max = MAX_COUNT/NUM_THREADS;
    int tid = ((struct tdata *) ptr)->tid;

    for (i=0; i < max; i++) {
        lock(&mutex);
        counter += 1;
        unlock(&mutex);
    }

    printf("End %d counter: %d\n", tid, counter);
    pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
    pthread_t threads[NUM_THREADS];
    int rc, i;
    struct tdata id[NUM_THREADS];

    for(i=0; i<NUM_THREADS; i++){
        id[i].tid = i;
        rc = pthread_create(&threads[i], NULL, count, (void *) &id[i]);
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    for(i=0; i<NUM_THREADS; i++){
        pthread_join(threads[i], NULL);
    }

    printf("Counter value: %d Expected: %d\n", counter, MAX_COUNT);
    return 0;
}
