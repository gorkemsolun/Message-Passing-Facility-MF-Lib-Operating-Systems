#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include "mf.h"

// Görkem Kadir Solun 22003214
// Murat Çağrı Kara 22102505


// write the signal handler function
// it will call mf_destroy()
// Termination can be achieved through the kill command or
// by pressing Ctrl - C or Ctrl - D at the command line if the server is running in the foreground
void sigint_handler(int signum) {
    printf("Caught signal, MF destroy %d\n", signum);

    if (mf_destroy() != MF_SUCCESS) {
        perror("mf_destroy failed");
    }

    printf("mfserver terminated\n");

    exit(0);
}

int main(int argc, char* argv[]) {

    // crtl - c
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        printf("\nCan't catch SIGINT\n");
        exit(1);  // Exit with an error if signal registration fails
    }
    // ctrl - d
    if (signal(SIGHUP, sigint_handler) == SIG_ERR) {
        printf("\nCan't catch SIGHUP\n");
        exit(1);  // Exit with an error if signal registration fails
    }
    // kill 
    if (signal(SIGTERM, sigint_handler) == SIG_ERR) {
        perror("Can't catch SIGTERM");
        exit(1);
    }

    printf("mfserver pid=%d\n", (int)getpid());

    // Call mf_init() to initialize the message facility
    int result = mf_init(); // will read the config file
    if (result != MF_SUCCESS) {
        printf("mf_init failed: %d\n", result);
        exit(1);
    }

    printf("mfserver initialized successfully.\n");
    printf("mfserver pid=%d\n", (int)getpid());

    while (1) { sleep(1000); }

    exit(0);
}


