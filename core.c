#include "core.h"

int concurrent_messages = 10;
int processing_time = 1000; //in milliseconds
int message_time = 1000; //in milliseconds

sem_t prepaidMutex;
sem_t postpaidMutex;
sem_t readyMutex;
sem_t *threadMutexes;
sem_t currentMCBMutex;
sem_t schedulerMutex;
struct map* clientMap;

List *prepaidWaiting;
List *postpaidWaiting;
List *readyQueue;

Buffer *message_buffer;

pthread_t server_tid;
pthread_t scheduler_tid;

bool schedulerRunning = false;

MessageControlBlock *currentMCB;

int main(int argc, char **argv){
    char *cvalue = NULL;
    char *pvalue = NULL;
    char *tvalue = NULL;
    bool cflag;
    bool pflag;
    bool tflag;
    int index;
    int c;

    opterr = 0;
    while ((c = getopt (argc, argv, "c:p:t:")) != -1){
        switch (c)
        {
        case 'c':
            cvalue = optarg;
            cflag = true;
            break;
        case 'p':
            pvalue = optarg;
            pflag = true;
            break;
        case 't':
            tvalue = optarg;
            tflag = true;
            break;
        case 'h':
            printf("Usage: %s [-c] <concurrent messages number> [-p] <scheduling period> [-t] <total message time>\n", argv[0]);
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 'c')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 'p')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 't')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            else if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
            fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
            return 1;
        }
    }
    if(cflag) concurrent_messages = atoi(cvalue);
    if(pflag) processing_time = atoi(pvalue);
    if(tflag) message_time = atoi(tvalue);

    sem_init(&prepaidMutex, 0, 1);
    sem_init(&postpaidMutex, 0, 1);
    sem_init(&readyMutex, 0, 1);
    sem_init(&currentMCBMutex, 0, 1);
    sem_init(&schedulerMutex, 0, 1);

    message_buffer = (Buffer *)malloc(sizeof(Buffer));
    message_buffer->array = (MessageControlBlock **)malloc(concurrent_messages*sizeof(MessageControlBlock *));

    threadMutexes = (sem_t *)malloc(concurrent_messages*sizeof(sem_t));

    prepaidWaiting = newList();
    postpaidWaiting = newList();
    readyQueue = newList();

    pthread_t worker_tid;
    for(int i = 0; i < concurrent_messages; i++){
        sem_init(&threadMutexes[i], 0, 1);
        message_buffer->array[i] = NULL;
        int *buffer_index_ptr = (int *)malloc(sizeof(int));
        *buffer_index_ptr = i;
        pthread_create(&worker_tid, NULL, workerThread, (void *)buffer_index_ptr);
    }

    pthread_create(&server_tid, NULL, serverThread, NULL);

    MessageControlBlock *readyMCB;
    while(true){
        sem_wait(&currentMCBMutex);
        readyMCB = currentMCB;
        currentMCB = NULL;
        sem_post(&currentMCBMutex);
        if(readyMCB != NULL){
            printf("Dispatching message '%s' from ID: %s at FD %d\n", readyMCB->message->textblock, readyMCB->message->clientID, readyMCB->response_fd);
            int i = 0;
            do{
                int blocked = sem_trywait(&threadMutexes[i]);
                if(!blocked){
                    if(message_buffer->array[i] == NULL){
                        message_buffer->array[i] = readyMCB;
                        readyMCB = NULL;
                        sem_post(&threadMutexes[i]);
                        break;
                    }
                    else if(!readyMCB->prepaid && message_buffer->array[i]->prepaid){
                        MessageControlBlock *swapMCB = message_buffer->array[i];
                        message_buffer->array[i] = readyMCB;
                        readyMCB = NULL;
                        sem_wait(&readyMutex);
                        tailInsert(readyQueue, (void *)swapMCB);
                        sem_post(&readyMutex);
                        sem_post(&threadMutexes[i]);
                        break;
                    }
                    sem_post(&threadMutexes[i]);
                }
                i = (i + 1)%concurrent_messages;
            }while(true);
        }    
    }

    pthread_join(server_tid, NULL);
    sem_destroy(&prepaidMutex);
    sem_destroy(&postpaidMutex);
    sem_destroy(&readyMutex);
    sem_destroy(&schedulerMutex);
    for(int i = 0; i < concurrent_messages; i++){
        sem_destroy(&threadMutexes[i]);
        free(message_buffer->array[i]);
    }
    free(threadMutexes);
    free(message_buffer);
    deleteList(prepaidWaiting);
    deleteList(postpaidWaiting);
    deleteList(readyQueue);
    mapClose(clientMap);
    return 0;
}

void *serverThread(void *arg){
    clientMap = mapNew();

    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8080";
    listenfd = open_listenfd(port);

    if (listenfd < 0){
		connection_error(listenfd);
    }

    int connfd;
    printf("Core started with %d threads, %dms scheduling period, %dms total message time.\n", concurrent_messages, processing_time, message_time);
    while(true){
        clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

        Message *message = (Message *)malloc(sizeof(Message));
        
        int *length = (int *)malloc(sizeof(int));
        read(connfd, length, sizeof(int));
        char *raw = (char *)malloc(*length + 1);
        read(connfd, raw, *length);
        raw[*length] = '\0';
        free(length);

        char *clientID = strtok(raw, "@");
        char *textblock = strtok(NULL, "@");

        message->clientID = (char *)malloc(strlen(clientID));
        message->textblock = (char *)malloc(strlen(textblock));

        strcpy(message->clientID, clientID);
        strcpy(message->textblock, textblock);
        free(raw);

        MessageControlBlock *controlBlock = (MessageControlBlock *)malloc(sizeof(MessageControlBlock));
        controlBlock->message = message;
        controlBlock->response_fd = connfd;
        controlBlock->time_remaining = message_time;

        char mode[6];
        strncpy(mode, message->textblock, 5);
        if(strcmp(mode, "#PRE#") == 0){
            int *prepaidCount = mapGet(message->clientID, clientMap);
            if(prepaidCount == NULL){
                prepaidCount = (int *)malloc(sizeof(int));
                *prepaidCount = 0;
                mapAdd(message->clientID, prepaidCount, clientMap);
            }
            if(*prepaidCount < 10){
                controlBlock->prepaid = true;
                (*prepaidCount)++;
                sem_wait(&prepaidMutex);
                tailInsert(prepaidWaiting, (void *)controlBlock);
                sem_post(&prepaidMutex);
            }
            else{
                char *response = "You've spent your 10 prepaid messages credit.";
                int response_size = strlen(response);
                write(connfd, &response_size, sizeof(int));
                write(connfd, response, strlen(response));
                close(connfd);
                free(controlBlock);
            }
        }
        else{
            sem_wait(&postpaidMutex);
            tailInsert(postpaidWaiting, (void *)controlBlock);
            sem_post(&postpaidMutex);
        }
        sem_wait(&schedulerMutex);
        if(!schedulerRunning && shouldStartScheduler()){
            printf("STARTING SCHEDULER THREAD...\n");
             pthread_create(&scheduler_tid, NULL, schedulerThread, NULL);
        }
        sem_post(&schedulerMutex);
    }
    return NULL;
}

bool shouldStartScheduler(){
    int messageCount = 0;
    sem_wait(&readyMutex);
    messageCount += readyQueue->length;
    sem_post(&readyMutex);
    sem_wait(&prepaidMutex);
    messageCount += prepaidWaiting->length;
    sem_post(&prepaidMutex);
    sem_wait(&postpaidMutex);
    messageCount += postpaidWaiting->length;
    sem_post(&postpaidMutex);
    if(messageCount == 1){
        return true;
    }
    return false;
}

void *schedulerThread(void *arg){
    sem_wait(&schedulerMutex);
    schedulerRunning = true;
    sem_post(&schedulerMutex);
    pthread_detach(pthread_self());
    MessageControlBlock *nextMCB = NULL;
    while(true){
        sem_wait(&postpaidMutex);
        if (postpaidWaiting->length > 0) {
            nextMCB = (MessageControlBlock *)pop(postpaidWaiting, 0);
        }
        sem_post(&postpaidMutex);
        sem_wait(&readyMutex);
        if (nextMCB == NULL) {
            nextMCB = (MessageControlBlock *)pop(readyQueue, 0);
        }
        sem_post(&readyMutex);
        if (nextMCB == NULL) {
            sem_wait(&prepaidMutex);
            if (prepaidWaiting->length > 0) {
                nextMCB = (MessageControlBlock *)pop(prepaidWaiting, 0);
            }
            sem_post(&prepaidMutex);
        }
        if(nextMCB != NULL){
            while(true){
                sem_wait(&currentMCBMutex);
                if(currentMCB == NULL){
                    currentMCB = nextMCB;
                    nextMCB = NULL;
                    sem_post(&currentMCBMutex);
                    break;
                }
                sem_post(&currentMCBMutex);
            }
        }
        else{
            sem_wait(&schedulerMutex);
            schedulerRunning = false;
            sem_post(&schedulerMutex);
            printf("STOPPING SCHEDULER THREAD...\n");
            pthread_exit(NULL);
        }
    }
    return NULL;
}

void *workerThread(void *arg){
    pthread_detach(pthread_self());
    int buffer_index = *(int *)arg;
    free(arg);
    char *response = "Your message has successully finished processing!";
    int response_size = strlen(response);
    sem_t *my_mutex = &threadMutexes[buffer_index];
    MessageControlBlock *currentMCB;
    while(true){
        sem_wait(my_mutex);
        currentMCB = message_buffer->array[buffer_index];
        if(currentMCB != NULL){
            printf("CPU burst in thread %d by message '%s' from ID: %s at FD %d\n", buffer_index, currentMCB->message->textblock, currentMCB->message->clientID, currentMCB->response_fd);
            if(currentMCB->time_remaining <= processing_time){
                usleep(currentMCB->time_remaining*1000);
                write(currentMCB->response_fd, &response_size, sizeof(int));
                write(currentMCB->response_fd, response, strlen(response));
                close(currentMCB->response_fd);
                message_buffer->array[buffer_index] = NULL;
                freeControlBlock(currentMCB);
            }
            else{
                usleep(processing_time*1000);
                currentMCB->time_remaining -= processing_time;
            }
        }
        sem_post(my_mutex);
    }
    return NULL;
}

void freeControlBlock(MessageControlBlock *mcb){
    free(mcb->message->clientID);
    free(mcb->message->textblock);
    free(mcb->message);
    free(mcb);
}