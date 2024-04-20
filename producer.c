//// run consumer first, then producer.
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
#include <sys/types.h>
#include <sys/wait.h>
#include "mf.h"

#define COUNT 10

char mqname[32] = "mq8";
int totalcount = COUNT;

int
main(int argc, char** argv) {
    int sentcount, qid, n_sent;
    // char sendbuffer[MAX_DATALEN]; // original code
    char sendbuffer[] = "Hello, World!AABBCCDDEEFFGGHHIIUUYYTTHHNNMMOOKKLLPPCCVVDDSSAAQQWWEE11223344556677889900--zzxxccvvbbnnmm<<TTGGHHYYUUJJKKIIOOLLPPMMNNBBVVCCXXZZAASSDDFFGGHHYYTTRREEWWQQ"; // not in original code
    int sendbuffer_len = strlen(sendbuffer); // not in original code
    struct timespec t1;

    totalcount = COUNT;
    if (argc != 2) {
        printf("usage: ./producer numberOfMessages\n");
        exit(1);
    }
    if (argc == 2)
        totalcount = atoi(argv[1]);

    clock_gettime(CLOCK_REALTIME, &t1);
    srand(t1.tv_nsec);

    mf_connect();
    qid = mf_open(mqname);
    sentcount = 0;
    while (1) {
        if (sentcount < totalcount) {
            // generate a random data size
            // n_sent = 1 + (rand() % (MAX_DATALEN - 1)); // original code
            n_sent = 1 + rand() % sendbuffer_len; // not in original code
            printf("app sending message, datalen=%d\n", n_sent);
            // data size is at least 1
            sendbuffer[0] = 1;
            int is_sent = mf_send(qid, (void*)sendbuffer, n_sent);

            if (is_sent == -1) {
                printf("send failed\n");
                break;
            }

            sentcount++;
            printf("sent data message %d\n", sentcount);
            printf("Message: %s\n", sendbuffer); // not in original code, prints entire buffer
        } else {
            sendbuffer[0] = -1;
            mf_send(qid, (void*)sendbuffer, 1);
            printf("sent END OF DATA message\n");
            break;
        }
    }

    mf_close(qid);
    mf_disconnect();
    return 0;
}

