#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 12
#define LCG_MULT ((uint64_t)0x5DEECE66D)
#define LCG_ADD ((uint64_t)0xB)
#define LCG_MOD_MASK ((uint64_t)1 << 48) - 1

typedef struct {
    int tid;                    // thread id
    int tcnt;                   // number of threads
    int *tstatus;               // each thread sets the bit corresponding to its tid to 0 when finished
    pthread_mutex_t *ts_mutex;  // mutex for setting tstatus
    uint64_t *pillar_counts;     // number of times a pillar generates at each position
    pthread_mutex_t *pc_mutex;  // mutex for reading_writing each pillar position
} thread_info;

void *tallyPillarsThread(void *arg) {
    thread_info *info = (thread_info*)arg;

    int x, y, z;
    uint64_t s_upper, s_lower, seed;
    uint64_t start = (uint64_t)((5*info->tid) << 17);
    uint64_t step = (uint64_t)((5*info->tcnt) << 17);
    // search upper bits that satisfy first requirement (nextInt(5) == 0)
    for(s_upper = start; s_upper <= LCG_MOD_MASK; s_upper += step) {
        // bruteforce lower bits
        for(s_lower = 0; s_lower < (1 << 17); ++s_lower) {
            seed = s_upper | s_lower;
            
            // check constraint on x coordinate (nextInt(16) < 4)
            seed = (LCG_MULT*seed + LCG_ADD) & LCG_MOD_MASK;
            x = seed >> 44;
            if(x >= 4) continue;

            // check constraint on z coordinate (nextInt(16) >= 10)
            seed = (LCG_MULT*seed + LCG_ADD) & LCG_MOD_MASK;
            z = (seed >> 44) - 10;
            if(z < 0) continue;

            // get y coordinate (nextInt(32))
            seed = (LCG_MULT*seed + LCG_ADD) & LCG_MOD_MASK;
            y = seed >> 43;

            // incrementer counter for pillar position
            pthread_mutex_lock(&(info->pc_mutex[x+4*y+128*z]));
            ++(info->pillar_counts[x+4*y+128*z]);
            pthread_mutex_unlock(&(info->pc_mutex[x+4*y+128*z]));
        }
    }

    // set corresponding bit of tstatus to 0;
    pthread_mutex_lock(info->ts_mutex);
    // &s tstatus with a bitmask that sets the bit corresponding to the tid to 0
    *(info->tstatus) &= -1^(1 << info->tid);
    pthread_mutex_unlock(info->ts_mutex);
}

int main() {
    // setup pillar tally array
    uint64_t *pillar_counts = (uint64_t*)calloc(4*32*6,sizeof(uint64_t));
    pthread_mutex_t *pc_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*4*32*6);
    for(int i = 0; i < 4*32*6; ++i) {
        pthread_mutex_init(&pc_mutex[i], NULL);
    }
    
    // setup tally threads
    int tstatus = (1 << NUM_THREADS) - 1;
    pthread_mutex_t ts_mutex;
    pthread_mutex_init(&ts_mutex, NULL);

    pthread_t *threads = malloc(sizeof(pthread_t)*NUM_THREADS);
    thread_info *info = malloc(sizeof(thread_info)*NUM_THREADS);
    for(int t = 0; t < NUM_THREADS; ++t) {
        info[t].tid = t;
        info[t].tcnt = NUM_THREADS;
        info[t].tstatus = &tstatus;
        info[t].ts_mutex = &ts_mutex;
        info[t].pillar_counts = pillar_counts;
        info[t].pc_mutex = pc_mutex;
    }

    // start threads
    for(int t = 0; t < NUM_THREADS; ++t) {
        pthread_create(&threads[t], NULL, tallyPillarsThread, (void*)&info[t]);
    }

    // monitor count
    int cont = 1;
    int x, y, z, maxX, maxY, maxZ;
    uint64_t count, max;
    while(cont) {
        pthread_mutex_lock(&ts_mutex);
        if(!tstatus) cont = 0;
        pthread_mutex_unlock(&ts_mutex);

        maxX = maxY = maxZ = 0;
        max = 0;
        for(x = 0; x < 4; ++x) {
            for(y = 0; y < 32; ++y) {
                for(z = 0; z < 6; ++z) {
                    pthread_mutex_lock(&pc_mutex[x+4*y+128*z]);
                    count = pillar_counts[x+4*y+128*z];
                    pthread_mutex_unlock(&pc_mutex[x+4*y+128*z]);

                    if(count > max) {
                        maxX = x;
                        maxY = y;
                        maxZ = z;
                        max = count;
                    }
                }
            }
        }

        printf("Most common pillar with incidence %ld at (%d,%d,%d)\n",max, 8+maxX, 6+maxY, 8+10+maxZ);

        sleep(10);
    }
    
    free(pillar_counts);
    free(pc_mutex);
    free(threads);
    free(info);

    return 0;
}
