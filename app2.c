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

#define COUNT 2

int totalcount = COUNT;

void test_messageflow_2p1mq();
void test_messageflow_4p2mq();


int main(int argc, char** argv) {
    totalcount = COUNT;
    if (argc != 2) {
        printf("usage: app2 numberOfMessages\n");
        exit(1);
    }
    if (argc == 2)
        totalcount = atoi(argv[1]);

    srand(time(0));

    //test_messageflow_2p1mq();
    test_messageflow_4p2mq();

    return 0;
}


void test_messageflow_2p1mq() {
    int ret1, qid;
    char sendbuffer[MAX_DATALEN];
    int n_sent;
    char recvbuffer[MAX_DATALEN];
    int sentcount = 0;
    int receivedcount = 0;
    int i;

    mf_connect();
    mf_create("mq1", 16); //  create mq;  size in KB

    ret1 = fork();
    if (ret1 == 0) {
        //  process - P1
        //  will create a message queue
        mf_connect();
        qid = mf_open("mq1");
        while (1) {
            n_sent = rand() % MAX_DATALEN;
            mf_send(qid, (void*)sendbuffer, n_sent);
            sentcount++;
            if (sentcount == totalcount)
                break;
        }
        mf_close(qid);
        mf_disconnect();
        exit(0);
    }
    ret1 = fork();
    if (ret1 == 0) {
        //  process - P2
        mf_connect();
        qid = mf_open("mq1");
        while (1) {
            mf_recv(qid, (void*)recvbuffer, MAX_DATALEN);
            receivedcount++;
            if (receivedcount == totalcount)
                break;
        }
        mf_close(qid);
        mf_disconnect();
        exit(0);
    }

    for (i = 0; i < 2; ++i)
        wait(NULL);

    mf_remove("mq1");
    mf_disconnect();
}




void test_messageflow_4p2mq() {
    int ret1, qid;
    // char sendbuffer[MAX_DATALEN]; // original code
    char sendbuffer[] = "Hello, World!AABBCCDDEEFFGGHHIIUUYYTTHHNNMMOOKKLLPPCCVVDDSSAAQQWWEE11223344556677889900--zzxxccvvbbnnmm<<TTGGHHYYUUJJKKIIOOLLPPMMNNBBVVCCXXZZAASSDDFFGGHHYYTTRREEWWQQ"; // not in original code
    int sendbuffer_len = strlen(sendbuffer); // not in original code
    int n_sent;
    char recvbuffer[MAX_DATALEN];
    int sentcount = 0;
    int receivedcount = 0;
    int i;

    printf("RAND_MAX is %d\n", RAND_MAX);

    mf_connect(); // in original code
    mf_create("mq1", 16); //  create mq;  size in KB
    mf_create("mq2", 16); //  create mq;  size in KB

    printf("mq1 and mq2 created\n");
    ret1 = fork();
    if (ret1 == 0) {
        // P1
        // P1 will send
        srand(time(0));
        mf_connect(); // in original code
        qid = mf_open("mq1");
        while (1) {
            // n_sent = rand() % MAX_DATALEN; // original code
            n_sent = rand() % sendbuffer_len; // not in original code
            mf_send(qid, (void*)sendbuffer, n_sent);
            printf("app sent message, datalen=%d\n", n_sent); // not in original code
            printf("Message: %s\n", sendbuffer); // not in original code, prints entire buffer
            sentcount++;
            if (sentcount == totalcount)
                break;
        }
        mf_close(qid);

        printf("P1 closing mq1\n");
        mf_disconnect();

        printf("P1 disconnecting\n");
        exit(0);
    }
    ret1 = fork();
    if (ret1 == 0) {
        // P2
        // P2 will receive
        srand(time(0));
        mf_connect(); // in original code
        qid = mf_open("mq1");
        while (1) {
            int n_received = mf_recv(qid, (void*)recvbuffer, MAX_DATALEN);
            receivedcount++;
            printf("app received message, datalen=%d\n", n_received);
            printf("Message: %s\n", recvbuffer); // not in original code, prints the buffer, prints maximum so far
            if (receivedcount == totalcount)
                break;
        }
        mf_close(qid);

        printf("P2 closing mq1\n");
        mf_disconnect();

        printf("P2 disconnecting\n");
        exit(0);
    }


    ret1 = fork();
    if (ret1 == 0) {
        // P3
        // P3 will send
        srand(time(0));
        // mf_connect(); // in original code
        qid = mf_open("mq2");
        while (1) {
            // n_sent = rand() % MAX_DATALEN; // original code
            n_sent = rand() % sendbuffer_len; // not in original code
            mf_send(qid, (void*)sendbuffer, n_sent);
            printf("app sent message, datalen=%d\n", n_sent); // not in original code
            printf("Message: %s\n", sendbuffer); // not in original code, prints entire buffer
            sentcount++;
            if (sentcount == totalcount)
                break;
        }
        mf_close(qid);

        printf("P3 closing mq2\n");
        mf_disconnect();

        printf("P3 disconnecting\n");
        exit(0);
    }
    ret1 = fork();
    if (ret1 == 0) {
        // P4
        // P4 will receive
        srand(time(0));
        // mf_connect(); // in original code
        qid = mf_open("mq2");
        while (1) {
            int n_received = mf_recv(qid, (void*)recvbuffer, MAX_DATALEN);
            receivedcount++;
            printf("app received message, datalen=%d\n", n_received);
            printf("Message: %s\n", recvbuffer); // not in original code, prints the buffer, prints maximum so far
            if (receivedcount == totalcount)
                break;
        }
        mf_close(qid);

        printf("P4 closing mq2\n");
        mf_disconnect();

        printf("P4 disconnecting\n");
        exit(0);
    }


    for (i = 0; i < 4; ++i) {
        wait(NULL);
    }


    mf_remove("mq1");
    mf_remove("mq2");
    mf_disconnect();
}


