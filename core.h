// Authored by José Julio Suárez (jojusuar@espol.edu.ec)

#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <ctype.h>
#include <getopt.h>
#include "list.h"
#include "map.h"
#include "common.h"

typedef struct message_control_block{
    Message *message;
    int response_fd;
    bool prepaid;
    int time_remaining;
}MessageControlBlock;

typedef struct mcb_buffer{
    MessageControlBlock **array;
}Buffer;

void *serverThread(void *);

void *schedulerThread(void *);

void *workerThread(void *);

void freeControlBlock(MessageControlBlock *);