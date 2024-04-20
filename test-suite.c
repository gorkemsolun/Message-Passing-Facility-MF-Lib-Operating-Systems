#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <time.h>
#include "mf.h"

int main(int argc, char** argv) {
     
     // start the timer for the batch file
     struct timespec start, end;
      mf_init();
     // time to create mq given sizes and remove them
     for(int i = MIN_MQSIZE; i <= MAX_MQSIZE; i+=(MAX_MQSIZE-MIN_MQSIZE) / (10 - 1)) { 
          clock_gettime(CLOCK_MONOTONIC, &start);
          mf_max_mq_given_size(i);
          clock_gettime(CLOCK_MONOTONIC, &end);
          float elapsedTime = (float)(end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) / 1000000000.0);
          if (elapsedTime > 0) {
               printf("Time to create max #mq(size = %d): %f\n", i, elapsedTime);
          }
          clock_gettime(CLOCK_MONOTONIC, &start);
          mf_del_max_mq_given_size();
          clock_gettime(CLOCK_MONOTONIC, &end);
          elapsedTime = (float)(end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) / 1000000000.0);
          if (elapsedTime > 0) {
               printf("Time to remove max #mq(size = %d): %f\n", i, elapsedTime);
          }
     }
     
     mf_destroy();
     return 0;
}
