#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include "mf.h"

struct MFConfig {
    int SHMEM_SIZE;
    int MAX_MSGS_IN_QUEUE;
    int MAX_QUEUES_IN_SHMEM;
    char SHMEM_NAME[MAXFILENAME]; // TODO: check maxfilename parameter
};

// Start of the library functions

// TODO: Create a shared memory region where the message queues will be stored and initialize
// TODO: Create a semaphore to protect the shared memory region and the message queues
// TODO: Read the configuration file and initialize the shared memory region and the semaphore
// TODO: Upon successful initialization, return 0 else return -1
int mf_init() {
    // Read the configuration file
    // TODO: Test read_config_file function
    struct MFConfig config;
    read_config_file(&config);





    return (MF_SUCCESS);
}

int mf_destroy() {
    return (MF_SUCCESS);
}

int mf_connect() {
    return (MF_SUCCESS);
}

int mf_disconnect() {
    return (MF_SUCCESS);
}

int mf_create(char* mqname, int mqsize) {
    return (MF_SUCCESS);
}

int mf_remove(char* mqname) {
    return (MF_SUCCESS);
}


int mf_open(char* mqname) {
    return (MF_SUCCESS);
}

int mf_close(int qid) {
    return(MF_SUCCESS);
}


int mf_send(int qid, void* bufptr, int datalen) {
    printf("mf_send called\n");
    return (MF_SUCCESS);
}

int mf_recv(int qid, void* bufptr, int bufsize) {
    printf("mf_recv called\n");
    return (MF_SUCCESS);
}

int mf_print() {
    return (MF_SUCCESS);
}

// End of the library functions
// Start of the helper functions

// Read the configuration file and fill the MFConfig structure
int read_config_file(struct MFConfig* config) {
    // Open the configuration file
    FILE* file = fopen(CONFIG_FILENAME, "r");
    if (file == NULL) {
        printf("Error: Could not open the configuration file\n");
        exit(MF_ERROR);
    }

    // Reading the configuration file line by line
    // and filling the MFConfig structure
    // Beware that lines starting with '#' are comments
    // and only the first two words in a line are considered
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') {
            continue;
        }

        char key[256];
        char value[256];
        sscanf(line, "%s %s", key, value);

        if (strcmp(key, "SHMEM_SIZE") == 0) {
            config->SHMEM_SIZE = atoi(value);
        } else if (strcmp(key, "MAX_MSGS_IN_QUEUE") == 0) {
            config->MAX_MSGS_IN_QUEUE = atoi(value);
        } else if (strcmp(key, "MAX_QUEUES_IN_SHMEM") == 0) {
            config->MAX_QUEUES_IN_SHMEM = atoi(value);
        } else if (strcmp(key, "SHMEM_NAME") == 0) {
            strcpy(config->SHMEM_NAME, value);
        }
    }

    // Debugging: Print the MFConfig structure
    print_MFConfig(config);

    fclose(file);
}
// Print the MFConfig structure
// NOTE: This function may only be used for debugging purposes
void print_MFConfig(
    struct MFConfig* config
) {
    printf("SHMEM_SIZE: %d\n", config->SHMEM_SIZE);
    printf("MAX_MSGS_IN_QUEUE: %d\n", config->MAX_MSGS_IN_QUEUE);
    printf("MAX_QUEUES_IN_SHMEM: %d\n", config->MAX_QUEUES_IN_SHMEM);
    printf("SHMEM_NAME: %s\n", config->SHMEM_NAME);
}