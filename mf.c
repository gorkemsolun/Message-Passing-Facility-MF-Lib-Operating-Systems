#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <semaphore.h>
#include "mf.h"

// GENERAL TODOS
// Complete test TODOs
// Create a data structure for message queues, messages, and shared memory region
// Make dynamic allocation for messages inside a message queue space

// LinkedList structure for finding places for the message queues in the shared memory region
// This structure will be used to find empty slots in the shared memory region
// It will contain start address and the size of the message queue in the shared memory region
// It is similar to hole list method in memory management
// It will be used in sorted order, the smallest start address will be at the head
typedef struct Hole {
    void* start_address;
    int size;
    struct Hole* next;
} Hole;

Hole* create_hole(void* start_address, int size) {
    Hole* hole = (Hole*)malloc(sizeof(Hole));
    hole->start_address = start_address;
    hole->size = size;
    hole->next = NULL;
    return hole;
}

// Insert a hole to the hole list in sorted order
void insert_hole(Hole* head, Hole* hole) {
    // Insert the hole in sorted order
    Hole* current = head;
    while (current->next != NULL && current->next->start_address < hole->start_address) {
        current = current->next;
    }
    hole->next = current->next;
    current->next = hole;
}

// Merge the holes in the hole list if they are contiguous in the shared memory region
void merge_holes(Hole* head) {
    Hole* current = head;
    while (current->next != NULL) {
        if (current->start_address + current->size == current->next->start_address) {
            current->size += current->next->size;
            Hole* temp = current->next;
            current->next = current->next->next;
            free(temp);
        } else {
            current = current->next;
        }
    }
}

struct MFConfig {
    int SHMEM_SIZE;
    int MAX_MSGS_IN_QUEUE;
    int MAX_QUEUES_IN_SHMEM;
    char SHMEM_NAME[MAXFILENAME];
};

// Global variables
struct MFConfig config; // Configuration parameters
void* shared_memory_address_fixed; // Start address of the shared memory region
void* shared_memory_address_queues; // Start address of the message queues in the shared memory region
int shared_memory_id; // ID of the shared memory region
int msg_queue_count = 0; // Number of message queues in the shared memory region
Hole* holes; // Hole list for finding empty slots in the shared memory region


// Helper function prototypes
int read_config_file(struct MFConfig* config);
void print_MFConfig(struct MFConfig* config);
int bytes_to_int_little_endian(char* bytes);
void int_to_bytes_little_endian(int val, char* bytes);

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
// TODO: Create a semaphore to protect the shared memory region and the message queues, I don't know if it is necessary, not implemented yet
// TODO UPDATE: I have added the semaphore array for each message queue, initialize the array after reading the configuration file, TEST NEEDED
// TODO UPDATE 2: Removed the semaphore array will be implemented later, not necessary for now
// Reads the configuration file
// TODO: Test the read_config_file function
int mf_init() {
    // Read the configuration file
    // TODO: Test read_config_file function, Gorkem tested it in mf_connect()
    int conf_status = read_config_file(&config);
    if (conf_status == MF_ERROR) {
        printf("Error: Could not read the configuration file\n");
        return (MF_ERROR);
    }

    // Create a shared memory region
    shared_memory_id = shm_open(config.SHMEM_NAME, O_CREAT | O_RDWR, 0666);
    if (shared_memory_id == -1) {
        printf("Error: Could not create the shared memory region\n");
        return (MF_ERROR);
    }

    // Set the size of the shared memory region
    int shared_memory_size = config.SHMEM_SIZE * 1024 * sizeof(char);
    int shared_memory_status = ftruncate(shared_memory_id, shared_memory_size);
    if (shared_memory_status == -1) {
        printf("Error: Could not set the size of the shared memory region\n");
        close(shared_memory_id);
        shm_unlink(config.SHMEM_NAME);
        return (MF_ERROR);
    }

    // Map the shared memory region to the address space of the calling process
    shared_memory_address_fixed = mmap(NULL, shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_id, 0);
    if (shared_memory_address_fixed == MAP_FAILED) {
        printf("Error: Could not map the shared memory region to the address space of the calling process\n");
        close(shared_memory_id);
        shm_unlink(config.SHMEM_NAME);
        return (MF_ERROR);
    }

    // Initialize the shared memory region by filling the region with zeros
    memset(shared_memory_address_fixed, 0, shared_memory_size);

    // Initialize the fixed shared memory region with config.MAX_QUEUES_IN_SHMEM message queue headers
    // Each message queue header will contain the following information:
    // - Message queue name (128 bytes) MAX_MQNAMESIZE
    // - Message queue id (4 bytes) Should be between 1 to config.MAX_QUEUES_IN_SHMEM inclusive
    // - Message queue size (4 bytes)
    // - Number of messages in the queue (4 bytes)
    // - Semaphore name (128 bytes) MAXFILENAME
    // - Semaphore ID (4 bytes)
    // - Address difference between the start address of the message queue and the start address of the shared memory region for message queues (4 bytes)

    shared_memory_address_queues = shared_memory_address_fixed + (sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 5 + sizeof(char) * MAXFILENAME) * config.MAX_QUEUES_IN_SHMEM;

    // Initialize the hole list for finding empty slots in the shared memory region
    // First hole will be the whole shared memory region for the message queues
    holes = create_hole(shared_memory_address_queues, shared_memory_size - (sizeof(char) * MF_MQ_HEADER_SIZE) * config.MAX_QUEUES_IN_SHMEM);

    return (MF_SUCCESS);
}

// This function will be invoked by the mfserver program during termination.
// Perform any necessary cleanup and deallocation operations before the program exits
// including removing the shared memory and all the synchronizatiSon objects to ensure a clean system state
// Destroys the shared memory region
// TODO: Test destroying the shared memory region
// Destroys the semaphores
// TODO: Test destroying the semaphores
// TODO: Check if the semaphore array is needed, this is related to mf_init() function
int mf_destroy() {
    // Remove the synchronization objects, free the semaphore array
    // TODO: Is this the correct way to destroy the semaphores? Other way may include closing and then unlinking the semaphore name

    // Destroy the shared memory region
    // Unmap the shared memory region from the address space of the calling process
    int shared_memory_status = munmap(shared_memory_address_fixed, config.SHMEM_SIZE);
    if (shared_memory_status == -1) {
        printf("Error: Could not unmap the shared memory region from the address space of the calling process\n");
        return (MF_ERROR);
    }

    // Close the shared memory region
    int shared_memory_close_status = close(shared_memory_id);
    if (shared_memory_close_status == -1) {
        printf("Error: Could not close the shared memory region\n");
        return (MF_ERROR);
    }

    // Remove the shared memory region, unlink the shared memory object
    int shared_memory_remove_status = shm_unlink(config.SHMEM_NAME);
    if (shared_memory_remove_status == -1) {
        printf("Error: Could not remove the shared memory region\n");
        return (MF_ERROR);
    }

    // Free global variables

    return (MF_SUCCESS);
}

// Applications linked with the MF library begin by calling the mf_connect() function, initializing the library for their use.
// This function will be called by each application (process) intending to utilize the MF library for message-based communication.
// It will perform the required initialization for the process.
// TODO: Implement the mf_connect() function
int mf_connect() {



    return (MF_SUCCESS);
}

int mf_disconnect() {
    return (MF_SUCCESS);
}

// This function creates a new message queue and initializes the necessary information for it.
// Space must be allocated from shared memory for message queues of various sizes.
// TODO: Solve the problem of allocating space for message queues of various sizes
// TODO: Create a semaphore for each message queue
// TODO: Assign a unique ID to each message queue (qid)
// TODO: Allocate space for the message queue in the shared memory region
// TODO: Initialize the message queue structure
int mf_create(char* mqname, int mqsize) {
    // Check if the count of message queues in the shared memory region is less than the maximum allowed
    if (msg_queue_count >= config.MAX_QUEUES_IN_SHMEM) {
        printf("Error: Maximum number of message queues in the shared memory region is reached\n");
        return (MF_ERROR);
    }

    // Check if the message queue size is within the limits
    if (mqsize < MIN_MQSIZE || mqsize > MAX_MQSIZE) {
        printf("Error: Message queue size is not within the limits\n");
        return (MF_ERROR);
    }

    int mqsize_bytes = mqsize * 1024 * sizeof(char);

    // Assign a unique ID to the message queue, search through fixed shared memory region for an empty slot by checking the message queue ids
    // The message queue id should be between 1 and config.MAX_QUEUES_IN_SHMEM inclusive
    // The message queue id should be unique, if a message queue with the same id exists, increment the id
    int qid = 1;
    while (qid < config.MAX_QUEUES_IN_SHMEM) {
        int qid_found = 0;
        for (int i = 0; i < config.MAX_QUEUES_IN_SHMEM; i++) {
            char mq_id_bytes[4];
            memcpy(mq_id_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE, 4);
            int mq_id = bytes_to_int_little_endian(mq_id_bytes);
            if (mq_id == qid) {
                qid_found = 1;
                break;
            }
        }

        if (qid_found == 0) {
            break;
        }

        qid++;
    }

    // Start address of the header of the message queue in the fixed shared memory region
    void* mq_header_address = shared_memory_address_fixed + qid * MF_MQ_HEADER_SIZE;

    // Set the message queue name in the message queue header
    memcpy(mq_header_address, mqname, sizeof(char) * MAX_MQNAMESIZE);

    // Set qid in the message queue header
    char qid_bytes[4];
    int_to_bytes_little_endian(qid, qid_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE, qid_bytes, 4);

    // Initialize the message count to 0
    char mq_msg_count_bytes[4];
    int_to_bytes_little_endian(0, mq_msg_count_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(int), mq_msg_count_bytes, 4);

    // Set the message queue size in the message queue header
    char mq_size_bytes[4];
    int_to_bytes_little_endian(mqsize_bytes, mq_size_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_size_bytes, 4);

    // TODO: Create a semaphore for the message queue
    // TODO: Assign a unique semaphore ID to the semaphore
    // TODO: Set the semaphore name in the message queue header
    // TODO: Set the semaphore ID in the message queue header

    // Find space for the message queue in the shared memory region through the hole list
    // The hole list will be used to find empty slots in the shared memory region
    Hole* current = holes;
    while (current != NULL) {
        if (current->size >= mqsize_bytes) {
            break;
        }
        current = current->next;
    }

    // Get the start address of the message queue in the shared memory region
    void* mq_start_address = current->start_address;

    // Update the selected hole in the hole list
    current->start_address += mqsize_bytes;
    current->size -= mqsize_bytes;

    // Update the message queue count
    msg_queue_count++;

    // Calculate the address difference between the start address of the message queue and the start address of the shared memory region for message queues
    int mq_start_address_diff = mq_start_address - shared_memory_address_queues;

    // Set the address difference in the message queue header
    char mq_start_address_diff_bytes[4];
    int_to_bytes_little_endian(mq_start_address_diff, mq_start_address_diff_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, mq_start_address_diff_bytes, 4);

    return (MF_SUCCESS);
}

int mf_remove(char* mqname) {
    return (MF_SUCCESS);
}

// This function opens a message queue for sending or receiving messages.
// If successful, it returns a message queue ID (qid) that will be used in subsequent calls to mf_send() and mf_recv().
int mf_open(char* mqname) {
    // Search for the message queue in the fixed shared memory region
    // If the message queue is found, return the message queue ID (qid)
    // Search through the message queue names in the fixed shared memory region
    for (int i = 0; i < config.MAX_QUEUES_IN_SHMEM; i++) {
        char mq_name[MAX_MQNAMESIZE];
        memcpy(mq_name, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE, MAX_MQNAMESIZE);
        // Compare the message queue name with the given message queue name
        // If the message queue is found, return the message queue ID (qid)
        if (strcmp(mq_name, mqname) == 0) {
            char mq_id_bytes[4];
            memcpy(mq_id_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE, 4);
            int qid = bytes_to_int_little_endian(mq_id_bytes);
            return qid;
        }
    }

    return (MF_ERROR);
}

// TODO: Remove the semaphore for the message queue, update the semaphore and semaphore_qids arrays
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
            // Remove the first character of the string, which is "/"
            // This is necessary because shm_open() function does not accept the first character as "/"
            memmove(config->SHMEM_NAME, config->SHMEM_NAME + 1, strlen(config->SHMEM_NAME));
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

int bytes_to_int_little_endian(char* bytes) {
    return (int)(bytes[0]) |
        (int)(bytes[1]) << 8 |
        (int)(bytes[2]) << 16 |
        (int)(bytes[3]) << 24;
}

void int_to_bytes_little_endian(int val, char* bytes) {
    bytes[0] = (char)(val & 0xFF);
    bytes[1] = (char)((val >> 8) & 0xFF);
    bytes[2] = (char)((val >> 16) & 0xFF);
    bytes[3] = (char)((val >> 24) & 0xFF);
}

int main() {
    mf_init();

    mf_create("mq1", 64);
    int g = mf_open("mq1");
    printf("qid: %d\n", g);

    mf_create("mq2", 32);
    int f = mf_open("mq2");
    printf("qid: %d\n", f);

    mf_create("mq3", 16);
    int h = mf_open("mq3");
    printf("qid: %d\n", h);









    mf_destroy();
    return 0;
}