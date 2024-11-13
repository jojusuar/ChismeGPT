#include "common.c"
#include <ctype.h>
#include <getopt.h>

int main(int argc, char **argv){
    char *mvalue = NULL;
    char *ivalue = NULL;
    bool iflag;
    bool mflag;
    bool pflag;
    int index;
    int c;

    opterr = 0;
    while ((c = getopt (argc, argv, "pi:m:")) != -1){
        switch (c)
        {
        case 'p':
            pflag = 1;
            break;
        case 'i':
            ivalue = optarg;
            iflag = true;
            break;
        case 'm':
            mvalue = optarg;
            mflag = true;
            break;
        case 'h':
            printf("Usage: %s [option] -i <clientID> -m <message>\n", argv[0]);
            printf("    No option:      Sends a postpaid message.\n");
            printf("    -p: <filename>  Sends a prepaid message.\n");
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 'm')
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
    if(!mflag){
        fprintf(stderr, "-m <message> is required.\n");
        printf("Usage: %s [option] -i <clientID> -m <message>\n", argv[0]);
        printf("    No option:      Sends a postpaid message.\n");
        printf("    -p: <filename>  Sends a prepaid message.\n");
        printf("    -h:             Shows this message.\n");
        return 1;
    }
    if(!iflag){
        fprintf(stderr, "-i <clientID> is required.\n");
        printf("Usage: %s [option] -i <clientID> -m <message>\n", argv[0]);
        printf("    No option:      Sends a postpaid message.\n");
        printf("    -p: <filename>  Sends a prepaid message.\n");
        printf("    -h:             Shows this message.\n");
        return 1;
    }

    char *hostname = "localhost";
    char *port = "8080";
    int connfd = open_clientfd(hostname, port);
	if(connfd < 0)
		connection_error(connfd);
    
    int message_length = strlen(ivalue)+strlen(mvalue)+7;
    char mode[4] = "POS";
    if(pflag){
        strcpy(mode, "PRE");
    }

    char *message = (char *)malloc(message_length);
    sprintf(message, "%s@#%s#%s", ivalue, mode, mvalue);
    int length = strlen(message);

    write(connfd, &length, sizeof(int));
    write(connfd, message, strlen(message));
    printf("Sent message \"%s\" by ID: %s.\n", mvalue, ivalue);

    int *response_size = (int *)malloc(sizeof(int));
    read(connfd, response_size, sizeof(int));
    char *response = (char *)malloc(*response_size);
    read(connfd, response, *response_size);
    response[*response_size] = '\0';

    printf("Server responded: \"%s\" to message \"%s\" by ID: %s.\n", response, mvalue, ivalue);
    free(message);
    free(response_size);
    free(response);
    close(connfd);
    if(pflag){
        return 10;
    }
    return 0;
}