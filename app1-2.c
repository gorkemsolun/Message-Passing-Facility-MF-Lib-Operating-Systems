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

char* mqname1 = "msgqueue1";

int main(int argc, char** argv) {
    char buf[5];

    mf_init();


        mf_print();

    mf_create("mq1", 64);
    int g = mf_open("mq1");
    printf("qid: %d\n", g);
    mf_print();
    mf_send(g, "Hello", 5);
    mf_send(g, "World", 5);
    
    mf_recv(g, buf, 5);
    printf("Received: %s\n", buf);
    mf_recv(g, buf, 5);
    printf("Received: %s\n", buf);

    mf_send(g, "kalem", 5);
    mf_send(g, "silgi", 5);
    mf_send(g, "defter", 6);
    mf_send(g, "kitap", 5);

    mf_recv(g, buf, 5);
    printf("Received: %s\n", buf);
    
    mf_send(g, "kalem", 5);

    mf_recv(g, buf, 5);
    printf("Received: %s\n", buf);



    mf_create("mq2", 32);
    int f = mf_open("mq2");
    printf("qid: %d\n", f);

    mf_send(f, "World", 5);

    mf_recv(f, buf, 5);
    mf_print();
    printf("Received: %s\n", buf);




    mf_create("mq3", 16);
    int h = mf_open("mq3");
    printf("qid: %d\n", h);
    mf_print();
    mf_send(h, "Test", 4);

    






    mf_destroy();
    return 0;
}