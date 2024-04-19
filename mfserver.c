#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include "mf.h"


// write the signal handler function
// it will call mf_destroy()
// Termination can be achieved through the kill command or
// by pressing Ctrl - C or Ctrl - D at the command line if the server is running in the foreground
// TODO - Test this function
void sigint_handler(int signum) {
    printf("Caught signal, MF destroy %d\n", signum);
    mf_destroy();
    exit(0);
}


// TODO: Control if the start of the program is correct as described in the project description
int main(int argc, char* argv[]) {

    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        printf("\ncan't catch SIGINT\n");
    }

    printf("mfserver pid=%d\n", (int)getpid());

    // register the signal handler function


    // call mf_init() to read the config file
    // TODO - Test this function
    mf_init(); // will read the config file

    while (1)
        sleep(1000);

    exit(0);
}


