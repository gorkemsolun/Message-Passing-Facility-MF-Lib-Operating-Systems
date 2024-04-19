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
// Update for above 2, Gorkem has done these need to check

// Reading the configuration file should be done in connect function instead of init function

// Implement the semaphores for synchronization

// Semaphores and maximum number of messages in a queue problem, what happens if the maximum number of messages can't be reached or overflows

// Shared memory mapping should be done in connect function instead of init function

// Space allocation should be done by multiples of 4



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
// TODO: Test the insert_hole function, it may not be working correctly for multiple holes
// TODO UPDATE: Gorkem has tested the insert_hole function it looks like it is working correctly, need to test again
void insert_hole(Hole** head, Hole* hole) {
    // Insert the hole in sorted order
    if (*head == NULL || (*head)->start_address >= hole->start_address) {
        hole->next = *head;
        *head = hole;
    } else {
        Hole* current = *head;
        while (current->next != NULL && current->next->start_address < hole->start_address) {
            current = current->next;
        }
        hole->next = current->next;
        current->next = hole;
    }
}

// Merge the holes in the hole list if they are contiguous in the shared memory region
// TODO: Test the merge_holes function
// TODO UPDATE: Gorkem has tested the merge_holes function, it looks like it is working correctly, need to test again
void merge_holes(Hole** head) {
    Hole* current = *head;
    while (current != NULL && current->next != NULL) {
        if (current->start_address + current->size == current->next->start_address) {
            current->size += current->next->size;
            Hole* temp = current->next;
            current->next = temp->next;
            free(temp);
        } else {
            current = current->next;
        }
    }
}

// Cleanup the hole list
// TODO: Test the free_holes function, we may need to check memory leaks
void free_holes(Hole** head) {
    Hole* current = *head;
    while (current != NULL) {
        Hole* temp = current;
        current = current->next;
        free(temp);
    }
    *head = NULL;
}

// Configuration structure for the MF library
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
char empty_sem_additon[MAXFILENAME] = "empty"; // Semaphore name addition for no message in the message queue
char full_sem_additon[MAXFILENAME] = "full"; // Semaphore name addition for insufficient space in the message queue
char access_mutex_sem_additon[MAXFILENAME] = "access_mutex"; // Semaphore name addition for access mutex in the message queue


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
// TODO UPDATE 2: Removed the semaphore array, it will be implemented later, not necessary for now
// Reads the configuration file
// TODO: Test the read_config_file function
// TODO Update: Gorkem tested it, need to test again
int mf_init() {
    // Read the configuration file
    // TODO: Test read_config_file function, Gorkem tested it
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
    // - Number of messages in the queue (4 bytes), message count
    // - Semaphore name (128 bytes) MAXFILENAME
    // - Address difference between the start address of the message queue and the start address of the shared memory region for message queues (4 bytes)
    // - Address difference between the start address of the next message in the message queue and the start address of the message queue (4 bytes)
    // - Address difference between the end address of the last message in the message queue and the start address of the message queue (4 bytes)
    // - Reference count (4 bytes)

    shared_memory_address_queues = shared_memory_address_fixed + (sizeof(char) * MF_MQ_HEADER_SIZE) * config.MAX_QUEUES_IN_SHMEM;

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
int mf_destroy() {
    // Remove the synchronization objects

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

    // Free the hole list
    free_holes(&holes);

    // TODO: Free global variables, check if there are any global variables that need to be freeds


    return (MF_SUCCESS);
}

// Applications linked with the MF library begin by calling the mf_connect() function, initializing the library for their use.
// This function will be called by each application (process) intending to utilize the MF library for message-based communication.
// It will perform the required initialization for the process.
// TODO: move reading the configuration file to mf_connect() function from mf_init() function
// TODO: Implement the mf_connect() function
int mf_connect() {


    return (MF_SUCCESS);
}

// This function will be invoked by an application (process)that no longer requires the messaging library.
// The library will remove this process from the list of active processes utilizing the library.
// TODO: Implement the mf_disconnect() function 
int mf_disconnect() {


    return (MF_SUCCESS);
}

// This function creates a new message queue and initializes the necessary information for it.
// Space must be allocated from shared memory for message queues of various sizes.
// Solve the problem of allocating space for message queues of various sizes
// TODO: Create a semaphore for each message queue
// Assign a unique ID to each message queue (qid)
// Allocate space for the message queue in the shared memory region
// Initialize the message queue structure
// TODO: Set allocated spaces size to multiples of 4, this may not be necessary
// TODO: Test the maximum number of message queues in the shared memory region, check if the maximum number of message queues is reached, what happens if the maximum number of message queues is reached
int mf_create(char* mqname, int mqsize) {
    // Check if the count of message queues in the shared memory region is less than the maximum alslowed
    if (msg_queue_count >= config.MAX_QUEUES_IN_SHMEM) {
        printf("Error: Maximum number of message queues in the shared memory region is reached\n");
        return (MF_ERROR);
    }

    // Check if the message queue size is within the limits
    if (mqsize < MIN_MQSIZE || mqsize > MAX_MQSIZE) {
        printf("Error: Message queue size is not within the limits\n");
        return (MF_ERROR);
    }

    // Calculate the message queue size in bytes
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
    void* mq_header_address = shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE;

    // Set the message queue name in the message queue header
    memcpy(mq_header_address, mqname, sizeof(char) * MAX_MQNAMESIZE);

    // Set qid in the message queue header
    char qid_bytes[4];
    int_to_bytes_little_endian(qid, qid_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE, qid_bytes, 4);

    // Initialize the message count to 0
    char mq_msg_count_bytes[4];
    int_to_bytes_little_endian(0, mq_msg_count_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_msg_count_bytes, 4);

    // Set the message queue size in the message queue header
    char mq_size_bytes[4];
    int_to_bytes_little_endian(mqsize_bytes, mq_size_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(int), mq_size_bytes, 4);

    // Create a semaphore base name for the message queue
    // The semaphore base name will be "semaphore" + qid
    char sem_name[MAXFILENAME] = "/semaphore";
    // Add the message queue id to the base semaphore name
    char qid_str[4];
    sprintf(qid_str, "%d", qid);
    strcat(sem_name, qid_str);

    // Set the semaphore base name in the message queue header
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME, sem_name, sizeof(char) * MAXFILENAME);

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
    char mq_start_address_difference_bytes[4];
    int_to_bytes_little_endian(mq_start_address_diff, mq_start_address_difference_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 3, mq_start_address_difference_bytes, 4);

    // Set the address difference between the start address of the next message in the message queue and the start address of the message queue to 0
    char mq_next_msg_address_difference_bytes[4];
    int_to_bytes_little_endian(0, mq_next_msg_address_difference_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, mq_next_msg_address_difference_bytes, 4);

    // Set the address difference between the end address of the last message in the message queue and the start address of the message queue to 0
    char mq_end_msg_address_difference_bytes[4];
    int_to_bytes_little_endian(0, mq_end_msg_address_difference_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, mq_end_msg_address_difference_bytes, 4);

    // Set the reference count to 0
    char mq_ref_count_bytes[4];
    int_to_bytes_little_endian(0, mq_ref_count_bytes);
    memcpy(mq_header_address + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 6, mq_ref_count_bytes, 4);

    printf("Message queue created with message queue name: %s, message queue id: %d, message queue size: %d\n", mqname, qid, mqsize);

    return (MF_SUCCESS);
}

// This function removes the message queue specified by the message queue name.
// It deallocates the space in the shared memory used by the message queue.
int mf_remove(char* mqname) {
    // Search for the message queue header in the fixed shared memory region
    // If the message queue is found, deallocate the space in the shared memory used by the message queue
    // Search through the message queue names in the fixed shared memory region
    for (int i = 0; i < config.MAX_QUEUES_IN_SHMEM; i++) {
        // Get the message queue name
        char mq_name[MAX_MQNAMESIZE];
        memcpy(mq_name, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE, MAX_MQNAMESIZE);

        // Compare the message queue name with the given message queue name
        // If the message queue is found, deallocate the space in the shared memory used by the message queue
        if (strcmp(mq_name, mqname) == 0) {
            // Get the reference count of the message queue
            char mq_ref_count_bytes[4];
            memcpy(mq_ref_count_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 6, 4);
            int mq_ref_count = bytes_to_int_little_endian(mq_ref_count_bytes);

            // Check the reference count of the message queue
            // If the reference count is greater than 0, do not deallocate the space in the shared memory used by the message queue
            if (mq_ref_count > 0) {
                printf("Error: Message queue is still in use\n");
                return (MF_ERROR);
            }

            // Get the message queue size
            char mq_size_bytes[4];
            memcpy(mq_size_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int), 4);
            int mq_size = bytes_to_int_little_endian(mq_size_bytes);

            // Get the address difference between the start address of the message queue and the start address of the shared memory region for message queues
            char mq_start_address_difference_bytes[4];
            memcpy(mq_start_address_difference_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 3, 4);
            int mq_start_address_diff = bytes_to_int_little_endian(mq_start_address_difference_bytes);

            // Calculate the start address of the message queue in the shared memory region
            void* mq_start_address = shared_memory_address_queues + mq_start_address_diff;

            // Update the hole list with the deallocated space
            insert_hole(&holes, create_hole(mq_start_address, mq_size));

            // Update the hole list by merging the holes if they are contiguous
            merge_holes(&holes);

            // Update the message queue count
            msg_queue_count--;

            // Clear the message queue header in the fixed shared memory region by filling it with zeros
            memset(shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE, 0, MF_MQ_HEADER_SIZE);

            // Clear the message queue in the shared memory region by filling it with zeros
            memset(mq_start_address, 0, mq_size);

            return (MF_SUCCESS);
        }
    }

    // If the message queue is not found, return an error
    printf("Error: Message queue with the given message queue name is not found\n");
    return (MF_ERROR);
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

            // Get the reference count of the message queue
            char mq_ref_count_bytes[4];
            memcpy(mq_ref_count_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 6, 4);
            int mq_ref_count = bytes_to_int_little_endian(mq_ref_count_bytes);

            // Increment the reference count of the message queue   
            mq_ref_count++;
            int_to_bytes_little_endian(mq_ref_count, mq_ref_count_bytes);
            memcpy(shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 6, mq_ref_count_bytes, 4);

            return qid;
        }
    }

    // If the message queue is not found, return an error
    printf("Error: Message queue with the given message queue name is not found\n");
    return (MF_ERROR);
}

// This function closes the message queue specified by the message queue ID (qid).
// It decrements the reference count of the message queue.
// Assumed that the message queue will be closed by the process that opened it.
// Assumed that the handling of the message queue will be done by the process that opened it.
int mf_close(int qid) {
    // Search for the message queue header in the fixed shared memory region  
    // If the message queue is found, decrement the reference count of the message queue
    for (int i = 0; i < config.MAX_QUEUES_IN_SHMEM; i++) {
        // Get the message queue ID
        char mq_id_bytes[4];
        memcpy(mq_id_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE, 4);
        int mq_id = bytes_to_int_little_endian(mq_id_bytes);

        // Compare the message queue ID with the given message queue ID
        if (mq_id == qid) {

            // Get the reference count of the message queue
            char mq_ref_count_bytes[4];
            memcpy(mq_ref_count_bytes, shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 6, 4);
            int mq_ref_count = bytes_to_int_little_endian(mq_ref_count_bytes);

            // Decrement the reference count of the message queue
            mq_ref_count--;
            int_to_bytes_little_endian(mq_ref_count, mq_ref_count_bytes);
            memcpy(shared_memory_address_fixed + i * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 6, mq_ref_count_bytes, 4);

            return(MF_SUCCESS);
        }
    }

    // If the message queue is not found, return an error
    return (MF_ERROR);
}

// This function sends a message to the message queue specified by the message queue ID (qid).
// TODO: It can block the caller until space is available in the queue.
// The message, obtained from the memory space pointed to by bufptr, is copied to the message queue buffer in the shared memory of the library.
// Data length specifies the size of the message in bytes.
int mf_send(int qid, void* bufptr, int datalen) {
    // Get the base semaphore name
    // TODO: This may be removed
    /* char sem_name[MAXFILENAME];
    memcpy(sem_name, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME, sizeof(char) * MAXFILENAME); */

    // Create a semaphore base name for the message queue
    // The semaphore base name will be "semaphore" + qid
    char sem_name[MAXFILENAME] = "/semaphore";
    // Add the message queue id to the base semaphore name
    char qid_str[4];
    sprintf(qid_str, "%d", qid);
    strcat(sem_name, qid_str);

    printf("Semaphore name: %s\n", sem_name);

    // Assemble the semaphore name for empty message queue
    char empty_sem_name[MAXFILENAME];
    strcpy(empty_sem_name, sem_name);
    strcat(empty_sem_name, empty_sem_additon);

    // Assemble the semaphore name for full message queue
    char full_sem_name[MAXFILENAME];
    strcpy(full_sem_name, sem_name);
    strcat(full_sem_name, full_sem_additon);

    // Assemble the semaphore name for access mutex in the message queue
    char access_mutex_sem_name[MAXFILENAME];
    strcpy(access_mutex_sem_name, sem_name);
    strcat(access_mutex_sem_name, access_mutex_sem_additon);

    // Open the semaphores
    sem_t* empty_sem = sem_open(empty_sem_name, O_CREAT, 0666, 0);
    sem_t* full_sem = sem_open(full_sem_name, O_CREAT, 0666, 0);
    sem_t* access_mutex_sem = sem_open(access_mutex_sem_name, O_CREAT, 0666, 1);

    printf("Empty semaphore name: %s\n", empty_sem_name);
    printf("Full semaphore name: %s\n", full_sem_name);
    printf("Access mutex semaphore name: %s\n", access_mutex_sem_name);


    // Check variable if the message queue is full
    int is_sent = 0;

    // Block the caller until space is available in the queue
    while (!is_sent) {

        printf("Access mutex semaphore is waited in send\n");
        // Wait for the access mutex semaphore
        sem_wait(access_mutex_sem);

        printf("Access mutex semaphore is acquired in send\n");

        // Search for the message queue header in the fixed shared memory region
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
            printf("Release the access mutex semaphore in send\n");
            sem_post(access_mutex_sem);
            sem_wait(empty_sem);
            continue;
        }

        // Update the data length to get actual data length
        datalen = datalen * sizeof(char);





        // Get message count
        char mq_msg_count_bytes[4];
        memcpy(mq_msg_count_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, 4);
        int mq_msg_count = bytes_to_int_little_endian(mq_msg_count_bytes);

        // Check if the message queue is full
        if (mq_msg_count == config.MAX_MSGS_IN_QUEUE) {
            sem_post(access_mutex_sem);
            sem_wait(empty_sem);
            continue;
        }

        // Get the start address difference of the message queue in the shared memory region
        char mq_start_address_difference_bytes[4];
        memcpy(mq_start_address_difference_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 3, 4);
        int mq_start_address_diff = bytes_to_int_little_endian(mq_start_address_difference_bytes);

        // Calculate the start address of the message queue in the shared memory region
        void* mq_start_address = shared_memory_address_queues + mq_start_address_diff;

        // Get message queue size
        char mq_size_bytes[4];
        memcpy(mq_size_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int), 4);
        int mq_size = bytes_to_int_little_endian(mq_size_bytes);

        // Get the address difference between the start address of the next message in the message queue and the start address of the message queue
        char mq_next_msg_address_difference_bytes[4];
        memcpy(mq_next_msg_address_difference_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, 4);
        int mq_next_msg_address_diff = bytes_to_int_little_endian(mq_next_msg_address_difference_bytes);

        // Get the address difference between the end address of the last message in the message queue and the start address of the message queue
        char mq_end_msg_address_difference_bytes[4];
        memcpy(mq_end_msg_address_difference_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, 4);
        int mq_end_msg_address_diff = bytes_to_int_little_endian(mq_end_msg_address_difference_bytes);

        // The message format in the message queue will be as follows:
        // - Message length (4 bytes)
        // - Message data (datalen bytes)

        // Find an empty slot in the message queue
        // If the message queue is empty
        if (mq_msg_count == 0) {
            // Calculate the start address of the message in the message queue
            void* mq_msg_start_address = mq_start_address;

            // Calculate the end address of the message in the message queue
            void* mq_msg_end_address = mq_msg_start_address + sizeof(int) + datalen;

            // Check if the message fits in the message queue
            if (mq_msg_end_address > mq_start_address + mq_size) {
                printf("Error: Message does not fit in the message queue even though the message queue is empty\n");
                return (MF_ERROR);
            }

            // Copy the message length to the message queue
            char msg_len_bytes[4];
            int_to_bytes_little_endian(datalen, msg_len_bytes);
            memcpy(mq_msg_start_address, msg_len_bytes, 4);

            // Copy the message data to the message queue
            memcpy(mq_msg_start_address + sizeof(int), bufptr, datalen);

            // Update the message count in the message queue header
            mq_msg_count++;
            int_to_bytes_little_endian(mq_msg_count, mq_msg_count_bytes);
            memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_msg_count_bytes, 4);

            // Update the address difference between the end address of the last message in the message queue and the start address of the message queue
            mq_end_msg_address_diff = mq_msg_end_address - mq_start_address;
            int_to_bytes_little_endian(mq_end_msg_address_diff, mq_end_msg_address_difference_bytes);
            memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, mq_end_msg_address_difference_bytes, 4);

            is_sent = 1;

            printf("Shared memory address: %p\n", shared_memory_address_fixed);
        }
        // If the end address of the last message is bigger than the start address of the next message
        else if (mq_end_msg_address_diff > mq_next_msg_address_diff) {
            // Two conditions may occur here:
            // 1. The difference between the end address of the last message and end of the message queue is bigger than the start address of the next message
            // In this case, the message can be added to end of the last message
            // 2. The difference between the end address of the last message and end of the message queue is smaller than the start address of the next message
            // In this case, the message can be added to the start of the message queue.
            // Beware that, we also need to check if the message fits in the message queue,
            // So, we have 2 cases in the second case.
            // Check the start address of the message queue and the start address of the next message.
            // If the message does not fit in the message queue, block the caller until space is available in the queue
            // TODO: Block the caller until space is available in the queue
            // Else, add the message to the start of the message queue

            // 1. The difference between the end address of the last message and end of the message queue is bigger than the start address of the next message
            // Check if the message fits in the message queue by checking the difference between the end address of the last message and end of the message queue
            if (mq_size - mq_end_msg_address_diff >= datalen + sizeof(int)) {
                // Calculate the start address of the message in the message queue
                void* mq_msg_start_address = mq_start_address + mq_end_msg_address_diff;

                // Calculate the end address of the message in the message queue
                void* mq_msg_end_address = mq_msg_start_address + sizeof(int) + datalen;

                // Copy the message length to the message queue
                char msg_len_bytes[4];
                int_to_bytes_little_endian(datalen, msg_len_bytes);
                memcpy(mq_msg_start_address, msg_len_bytes, 4);

                // Copy the message data to the message queue
                memcpy(mq_msg_start_address + sizeof(int), bufptr, datalen);

                // Update the message count in the message queue header
                mq_msg_count++;
                int_to_bytes_little_endian(mq_msg_count, mq_msg_count_bytes);
                memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_msg_count_bytes, 4);

                // Update the address difference between the end address of the last message in the message queue and the start address of the message queue
                mq_end_msg_address_diff = mq_msg_end_address - mq_start_address;
                int_to_bytes_little_endian(mq_end_msg_address_diff, mq_end_msg_address_difference_bytes);
                memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, mq_end_msg_address_difference_bytes, 4);
            
                is_sent = 1;
            }
            // 2. The difference between the end address of the last message and end of the message queue is smaller than the start address of the next message
            // In this case, we have 2 cases as described above.
            else {
                // Calculate the start address of the message in the message queue
                void* mq_msg_start_address = mq_start_address;

                // Calculate the end address of the message in the message queue
                void* mq_msg_end_address = mq_msg_start_address + sizeof(int) + datalen;

                // Calculate the start address of the next message in the message queue
                void* mq_next_msg_start_address = mq_start_address + mq_next_msg_address_diff;

                // Check if the message fits in the message queue
                // If the message does not fit in the message queue, block the caller until space is available in the queue
                if (mq_msg_end_address > mq_next_msg_start_address) {
                    sem_post(access_mutex_sem);
                    sem_wait(empty_sem);
                    continue;
                }

                // If the message fits in the message queue
                if (mq_msg_end_address <= mq_next_msg_start_address) {
                    // Copy the message length to the message queue
                    char msg_len_bytes[4];
                    int_to_bytes_little_endian(datalen, msg_len_bytes);
                    memcpy(mq_msg_start_address, msg_len_bytes, 4);

                    // Copy the message data to the message queue
                    memcpy(mq_msg_start_address + sizeof(int), bufptr, datalen);

                    // Update the message count in the message queue header
                    mq_msg_count++;
                    int_to_bytes_little_endian(mq_msg_count, mq_msg_count_bytes);
                    memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_msg_count_bytes, 4);

                    // Update the address difference between the end address of the last message in the message queue and the start address of the message queue
                    mq_end_msg_address_diff = mq_msg_end_address - mq_start_address;
                    int_to_bytes_little_endian(mq_end_msg_address_diff, mq_end_msg_address_difference_bytes);
                    memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, mq_end_msg_address_difference_bytes, 4);

                    is_sent = 1;
                }
            }
        }
        // If the end address of the last message is smaller than the start address of the next message
        // This means that there is an empty slot between the end address of the last message and the start address of the next message
        // Here, we have 2 cases as in the previous cases the second case
        else if (mq_end_msg_address_diff < mq_next_msg_address_diff) {
            // Calculate the start address of the message in the message queue
            void* mq_msg_start_address = mq_start_address + mq_end_msg_address_diff;

            // Calculate the end address of the message in the message queue
            void* mq_msg_end_address = mq_msg_start_address + sizeof(int) + datalen;

            // Calculate the start address of the next message in the message queue
            void* mq_next_msg_start_address = mq_start_address + mq_next_msg_address_diff;

            // Check if the message fits in the message queue
            if (mq_msg_end_address > mq_next_msg_start_address) {
                sem_post(access_mutex_sem);
                sem_wait(empty_sem);
                continue;
            }

            // If the message fits in the message queue
            if (mq_msg_end_address <= mq_next_msg_start_address) {
                // Copy the message length to the message queue
                char msg_len_bytes[4];
                int_to_bytes_little_endian(datalen, msg_len_bytes);
                memcpy(mq_msg_start_address, msg_len_bytes, 4);

                // Copy the message data to the message queue
                memcpy(mq_msg_start_address + sizeof(int), bufptr, datalen);

                // Update the message count in the message queue header
                mq_msg_count++;
                int_to_bytes_little_endian(mq_msg_count, mq_msg_count_bytes);
                memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_msg_count_bytes, 4);

                // Update the address difference between the end address of the last message in the message queue and the start address of the message queue
                mq_end_msg_address_diff = mq_msg_end_address - mq_start_address;
                int_to_bytes_little_endian(mq_end_msg_address_diff, mq_end_msg_address_difference_bytes);
                memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, mq_end_msg_address_difference_bytes, 4);

                is_sent = 1;
            }
        }
        // If the end address of the last message is equal to the start address of the next message
        // This means that the message queue is full
        else {
            sem_post(access_mutex_sem);
            sem_wait(empty_sem);
            continue;
        }
    }

    // Signal mutex semaphore
    sem_post(access_mutex_sem);

    // Signal full semaphore
    sem_post(full_sem);

    return (MF_SUCCESS);
}

// This function retrieves a message from the message queue, blocking the caller if no message is available for removal.
// The messages are removed from the message queue in the order they were added (FIFO).
// The message is copied to the memory space pointed to by bufptr.
// After the message is copied, the message is removed from the message queue.
// The bufsize parameter of this function specifies the size of the application buffer where the incoming message is to be stored.
// It does not represent the length of the incoming message.
// If the incoming message is larger than the buffer size, the message is truncated.
// The bufsize parameter value (i.e., application buffer size) must be larger or equal to MAXDATALEN to ensure sufficient space in the application buffer for any incoming message.
int mf_recv(int qid, void* bufptr, int bufsize) {
    // Get the base semaphore name
    // TODO: This may be removed
    /* char sem_name[MAXFILENAME];
    memcpy(sem_name, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME, sizeof(char) * MAXFILENAME); */

    // Create a semaphore base name for the message queue
    // The semaphore base name will be "semaphore" + qid
    char sem_name[MAXFILENAME] = "/semaphore";
    // Add the message queue id to the base semaphore name
    char qid_str[4];
    sprintf(qid_str, "%d", qid);
    strcat(sem_name, qid_str);

    printf("Semaphore name: %s\n", sem_name);

    // Assemble the semaphore name for empty message queue
    char empty_sem_name[MAXFILENAME];
    strcpy(empty_sem_name, sem_name);
    strcat(empty_sem_name, empty_sem_additon);

    // Assemble the semaphore name for full message queue
    char full_sem_name[MAXFILENAME];
    strcpy(full_sem_name, sem_name);
    strcat(full_sem_name, full_sem_additon);

    // Assemble the semaphore name for access mutex in the message queue
    char access_mutex_sem_name[MAXFILENAME];
    strcpy(access_mutex_sem_name, sem_name);
    strcat(access_mutex_sem_name, access_mutex_sem_additon);

    // Open the semaphores
    sem_t* empty_sem = sem_open(empty_sem_name, O_CREAT, 0666, 0);
    sem_t* full_sem = sem_open(full_sem_name, O_CREAT, 0666, 0);
    sem_t* access_mutex_sem = sem_open(access_mutex_sem_name, O_CREAT, 0666, 1);

    printf("Empty semaphore name: %s\n", empty_sem_name);
    printf("Full semaphore name: %s\n", full_sem_name);
    printf("Access mutex semaphore name: %s\n", access_mutex_sem_name);

    int is_received = 0;

    while (!is_received) {

        printf("Access mutex semaphore is waited in receive\n");

        // Wait for the access mutex semaphore
        sem_wait(access_mutex_sem);

        printf("Access mutex semaphore is acquired in receive\n");

        // Search for the message queue in the fixed shared memory region
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

        // If the message queue is not found
        if (qid_found == 0) {
            printf("Release the access mutex semaphore\n");
            sem_post(access_mutex_sem);
            sem_wait(full_sem);
            continue;
        }

        // Get message count
        char mq_msg_count_bytes[4];
        memcpy(mq_msg_count_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, 4);
        int mq_msg_count = bytes_to_int_little_endian(mq_msg_count_bytes);

        // If the message queue is empty, block the caller until a message is available
        if (mq_msg_count == 0) {
            sem_post(access_mutex_sem);
            sem_wait(full_sem);
            continue;
        }

        is_received = 1;

        // Get the start address difference of the message queue in the shared memory region
        char mq_start_address_difference_bytes[4];
        memcpy(mq_start_address_difference_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 3, 4);
        int mq_start_address_diff = bytes_to_int_little_endian(mq_start_address_difference_bytes);

        // Calculate the start address of the message queue in the shared memory region
        void* mq_start_address = shared_memory_address_queues + mq_start_address_diff;

        // Get the address difference between the start address of the next message in the message queue and the start address of the message queue
        char mq_next_msg_address_difference_bytes[4];
        memcpy(mq_next_msg_address_difference_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, 4);
        int mq_next_msg_address_diff = bytes_to_int_little_endian(mq_next_msg_address_difference_bytes);

        // Get the address difference between the end address of the last message in the message queue and the start address of the message queue
        char mq_end_msg_address_difference_bytes[4];
        memcpy(mq_end_msg_address_difference_bytes, shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, 4);
        int mq_end_msg_address_diff = bytes_to_int_little_endian(mq_end_msg_address_difference_bytes);

        // The message format in the message queue will be as follows:
        // - Message length (4 bytes)
        // - Message data (datalen bytes)

        // Calculate the start address of the message in the message queue
        void* mq_msg_start_address = mq_start_address + mq_next_msg_address_diff;

        // Get the message length from the message queue
        char msg_len_bytes[4];
        memcpy(msg_len_bytes, mq_msg_start_address, 4);
        int msg_len = bytes_to_int_little_endian(msg_len_bytes);

        // Calculate the minimum message size to copy and update the buffer size
        if (msg_len < bufsize) {
            bufsize = msg_len;
        }

        // Copy the message data to the buffer
        memcpy(bufptr, mq_msg_start_address + sizeof(int), bufsize);

        // Update the message count in the message queue header
        mq_msg_count--;
        int_to_bytes_little_endian(mq_msg_count, mq_msg_count_bytes);
        memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(int) * 2, mq_msg_count_bytes, 4);

        // Erase the message from the message queue by filling the message with zeros
        memset(mq_msg_start_address, 0, sizeof(int) + msg_len);

        // Update the next message address difference in the message queue header
        // If the message queue is empty, set the next and last message address difference to 0
        if (mq_msg_count == 0) {
            int_to_bytes_little_endian(0, mq_next_msg_address_difference_bytes);
            memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, mq_next_msg_address_difference_bytes, 4);

            int_to_bytes_little_endian(0, mq_end_msg_address_difference_bytes);
            memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 5, mq_end_msg_address_difference_bytes, 4);
        }
        // Calculate the next message address difference in the message queue
        else {
            // Calculate the start address of the next message in the message queue
            void* mq_next_msg_start_address = mq_msg_start_address + sizeof(int) + msg_len;

            // Check if the 4 bytes after the next message start address to be sure that a message exists there
            char next_msg_len_bytes[4];
            memcpy(next_msg_len_bytes, mq_next_msg_start_address, 4);
            int next_msg_len = bytes_to_int_little_endian(next_msg_len_bytes);

            // If the next message length is 0, this means that there is no message, so set the next message address difference to 0,
            // as the next message is at the start of the message queue
            if (next_msg_len == 0) {
                int_to_bytes_little_endian(0, mq_next_msg_address_difference_bytes);
                memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, mq_next_msg_address_difference_bytes, 4);
            }
            // If the next message length is not 0, this means that there is a message, so calculate the next message address difference
            else {
                int next_msg_address_diff = mq_next_msg_start_address - mq_start_address;
                int_to_bytes_little_endian(next_msg_address_diff, mq_next_msg_address_difference_bytes);
                memcpy(shared_memory_address_fixed + (qid - 1) * MF_MQ_HEADER_SIZE + sizeof(char) * MAX_MQNAMESIZE + sizeof(char) * MAXFILENAME + sizeof(int) * 4, mq_next_msg_address_difference_bytes, 4);
            }
        }

    }

    // Signal mutex semaphore
    sem_post(access_mutex_sem);

    // Signal empty semaphore
    sem_post(empty_sem);

    // Return the actual message length
    return bufsize;
}

// Prints the status of the current shared memory and its message queues.
int mf_print() {
    // If the shared memory is not initialized, return an error
    if (holes != NULL) {
        Hole* head = holes;
        char* start;
        int k = 1;

        // Print the holes and message queues in the shared memory
        // Traverse the holes and message queues in the shared memory
        // TODO: This control part is wrong, it should be fixed
        if (*((char*)shared_memory_address_fixed) == *((char*)(head->start_address))) {
            start = "Hole";
        } else {
            start = "MQ";
        }
        do {
            if (head->next != NULL) {
                if (start == "Hole") {
                    printf("%s->", start);
                    start = "MQ";
                } else {
                    printf("%s%d->", start, k++);
                    start = "Hole";
                }
            } else {
                if (start == "Hole") {
                    printf("%s", start);
                    start = "MQ";
                } else {
                    printf("%s%d", start, k++);
                    start = "Hole";
                }
            }
            head = head->next;
        } while (head != NULL);

        printf("\n");
        return (MF_SUCCESS);
    }

    return (MF_ERROR);
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

// Check if the value is negative, if it is, convert it to a positive value
int char_overflow_check(int value) {
    if (value < 0) {
        value += 256;
    }
    return value;
}

// Convert 4 bytes to an integer in little-endian order
int bytes_to_int_little_endian(char* bytes) {
    return (int)(char_overflow_check(bytes[0])) |
        (int)(char_overflow_check(bytes[1])) << 8 |
        (int)(char_overflow_check(bytes[2])) << 16 |
        (int)(char_overflow_check(bytes[3])) << 24;
}

// Convert an integer to 4 bytes in little-endian order
void int_to_bytes_little_endian(int val, char* bytes) {
    bytes[0] = (char)(val & 0xFF);
    bytes[1] = (char)((val >> 8) & 0xFF);
    bytes[2] = (char)((val >> 16) & 0xFF);
    bytes[3] = (char)((val >> 24) & 0xFF);
}

// As the semaphores are started being used in the library, we can't use the main function for testing the library functions.
/* int main() {
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


    mf_remove("mq1");
    mf_remove("mq2");

    mf_create("mq2", 32);
    mf_remove("mq2");



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
} */

/* #include <time.h>
#define COUNT 10
char* semname1 = "/semaphore1";
char* semname2 = "/semaphore2";
sem_t* sem1, * sem2;
char* mqname1 = "msgqueue1";

int main(int argc, char** argv) {
    int ret, i, qid;
    char sendbuffer[MAX_DATALEN];
    int n_sent, n_received;
    char recvbuffer[MAX_DATALEN];
    int sentcount;
    int receivedcount;
    int totalcount;

    totalcount = COUNT;
    if (argc == 2)
        totalcount = atoi(argv[1]);

    sem1 = sem_open(semname1, O_CREAT, 0666, 0); // init sem
    sem2 = sem_open(semname2, O_CREAT, 0666, 0); // init sem

    mf_init();

    srand(time(0));
    printf("RAND_MAX is %d\n", RAND_MAX);

    ret = fork();
    if (ret > 0) {
        // parent process - P1
        // parent will create a message queue

        mf_connect();

        mf_create(mqname1, 16); //  create mq;  16 KB

        qid = mf_open(mqname1);

        sem_post(sem1);

        while (1) {
            n_sent = rand() % MAX_DATALEN;
            ret = mf_send(qid, (void*)sendbuffer, n_sent);
            printf("app sent message, datalen=%d\n", n_sent);
            sentcount++;
            if (sentcount == totalcount)
                break;
        }
        mf_close(qid);
        sem_wait(sem2);
        // we are sure other process received the messages

        mf_remove(mqname1);   // remove mq
        mf_disconnect();
    } else if (ret == 0) {
        // child process - P2
        // child will connect, open mq, use mq
        printf("child process1\n");
        sem_wait(sem1);
        // we are sure mq was created
        printf("child process2\n");

        mf_connect();

        qid = mf_open(mqname1);

        while (1) {
            n_received = mf_recv(qid, (void*)recvbuffer, MAX_DATALEN);
            printf("app received message, datalen=%d\n", n_received);
            receivedcount++;
            if (receivedcount == totalcount)
                break;
        }
        mf_close(qid);
        mf_disconnect();
        sem_post(sem2);
    }
    return 0;
} */