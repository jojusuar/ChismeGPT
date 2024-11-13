#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("STARTING STRESS TEST, sending 100 mixed prepaid and postpaid messages...\n\n");

    for(int i = 0; i < 100; i += 2) {
        char id[12];

        if(fork() == 0) {
            sprintf(id, "%d", i);
            char *args[] = {"./client", "-p", "-i", id, "-m", "Hello core, this is a prepaid message!", NULL};
            execvp(args[0], args);
            perror("execvp failed");
            exit(1);
        }
        else {
            if(fork() == 0) {
                sprintf(id, "%d", i + 1);
                char *args[] = {"./client", "-i", id, "-m", "Hello core, this is a postpaid message!", NULL};
                execvp(args[0], args);
                perror("execvp failed");
                exit(1);
            }
        }
    }
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, 0)) > 0){
        if(status == 1){
            printf("A message failed to be processed.\n");
        }
    }
    printf("\nNow sending 15 prepaid messages with the same ID...\n\n");
    for(int i = 0; i < 15; i++){
        char id[5] = "1234";
        if(fork() == 0){
            char *args[] = {"./client", "-p", "-i", id, "-m", "Hello core, this is a prepaid message!", NULL};
            execvp(args[0], args);
        }
    }
    while ((pid = waitpid(-1, &status, 0)) > 0){
        if(status == 1){
            printf("A message failed to be processed.\n");
        }
    }

    printf("All child processes have completed.\n");
}