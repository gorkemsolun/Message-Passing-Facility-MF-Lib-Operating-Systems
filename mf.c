#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include "mf.h"

struct MFConfig {
    int SHMEM_SIZE;
    int MAX_MSGS_IN_QUEUE;
    int MAX_QUEUES_IN_SHMEM;
    char SHMEM_NAME[MAXFILENAME]; // TODO: check maxfilename parameter
};

// Helper function prototypes
int read_config_file(struct MFConfig* config);
void print_MFConfig(struct MFConfig* config);

// Start of the library functions

// Creates a shared memory region where the message queues will be stored and initialize
// Shared memory region will be used to store the message queues and the messages.
// It will be a contiguous memory region, circular buffer messages will be stored one after the other.
// The stored messages will be read by the receiver and removed from the region and queue in order of arrival (FIFO).
// Dynamic allocation for messages inside a message queue space will not be used.
// Instead, a new message(item) will simply be added to the end of the other items in the allocated space(buffer).
// A fixed portion of your shared memory can be allocated for storing management information and structures.
// The fixed portion may include the number of message queues, the size of each message queue, the number of messages in each queue,
// names of the semaphores, and configuration parameters.
// TODO: Test the created shared memory region and the creation of the shared memory region
// TODO: Create a semaphore to protect the shared memory region and the message queues
// TODO: Read the configuration file and initialize the shared memory region and the semaphore
// TODO: Upon successful initialization, return 0 else return -1
int mf_init() {
    // Read the configuration file
    // TODO: Test read_config_file function, Gorkem tested it in mf_connect()
    struct MFConfig config;
    int conf_status = read_config_file(&config);
    if (conf_status == MF_ERROR) {
        printf("Error: Could not read the configuration file\n");
        return (MF_ERROR);
    }

    // Create a shared memory region
    int shared_memory_id = shm_open(config.SHMEM_NAME, O_CREAT | O_RDWR, 0666);
    if (shared_memory_id == -1) {
        printf("Error: Could not create the shared memory region\n");
        return (MF_ERROR);
    }

    // Set the size of the shared memory region
    int shared_memory_size = config.SHMEM_SIZE;
    int shared_memory_status = ftruncate(shared_memory_id, shared_memory_size);
    if (shared_memory_status == -1) {
        printf("Error: Could not set the size of the shared memory region\n");
        close(shared_memory_id);
        shm_unlink(config.SHMEM_NAME);
        return (MF_ERROR);
    }

    // Map the shared memory region to the address space of the calling process
    void* shared_memory_address = mmap(NULL, shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_id, 0);
    if (shared_memory_address == MAP_FAILED) {
        printf("Error: Could not map the shared memory region to the address space of the calling process\n");
        close(shared_memory_id);
        shm_unlink(config.SHMEM_NAME);
        return (MF_ERROR);
    }


    return (MF_SUCCESS);
}

int mf_destroy() {
    return (MF_SUCCESS);
}

int mf_connect() {
    // TODO: remove 2 lines below after testing
    struct MFConfig config;
    read_config_file(&config);

    return (MF_SUCCESS);
}

int mf_disconnect() {
    return (MF_SUCCESS);
}

// Create a message queue with the given name and size
// Applications linked with the MF library begin by calling the mf connect() function, initializing the library for their use.
// Space must be allocated from shared memory for message queues of various sizes.
// TODO: Solve the problem of allocating space for message queues of various sizes
// TODO: Create a semaphore for each message queue
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

    // TODO: Check if all the fields are filled
    // TODO: Check if the values are valid and within the limits, this may be necessary

    // Debugging: Print the MFConfig structure
    print_MFConfig(config);

    fclose(file);

    return (MF_SUCCESS);
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