#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <ctype.h>

#define MAX 80
#define SA struct sockaddr



char *version = "0.1";

char correct_string (char * str) {
    for (int i = 0; i < strlen(str) - 1; i++) {
        //printf("%c\n", str[i]);
        if (!isdigit(str[i]) && str[i] != '(' && str[i] != ')' && str[i] != '+' && str[i] != '-' && str[i] != '*' && str[i] != '/') {
            //printf("return = %c\n", str[i]);
            return str[i];
        }

    }
    //printf("return = -1\n");
    return -1;
}

int main(int argc, char **argv, char* env[])
{
    int sockfd, connfd;
    struct sockaddr_in servaddr;
    in_addr_t addr = inet_addr("127.0.0.1");
    int serv_port = 8080;
    time_t start_time = time(NULL), current_time;

    char buff[MAX];
    //char * buff;

    if (getenv("L2ADDR"))
        addr = inet_addr(getenv("L2ADDR"));

    if (getenv("L2PORT"))
        serv_port = strtol(getenv("L2PORT"), NULL, 10);

    //getopt, old friend
    int c;
    int digit_optind = 0;
    while ((c = getopt(argc, argv, "w:dl:a:p:vh")) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
            case 'a':
                addr = inet_addr(optarg);
                break;
            case 'p':
                serv_port = strtol(optarg, NULL, 10);
                break;
            case 'v':
                printf("Version: %s \n", version);
                break;
            case 'h':
                printf("\n=====HELP=====\n"
                       " Опции:\n"
                       "-a address. Default is 127.0.0.1 \n"
                       "-p port. The Default is 8080 \n"
                       "-v version \n"
                       "-h this message \n");
                break;
            case '?':
                break;
            case 0:

                break;
            default:
                printf("?? getopt returned character code %c ??\n", c);
        }
    }

    if (optind < argc) {
        strcpy(buff, argv[optind]);
        printf("String: %s\n", buff);
        if (correct_string(buff) != -1) {
            printf("String '%s' is NOT correct\n Terminating...\n", buff);
        }

    }
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Error: socket creation\n");
        exit(0);
    }
    else
        printf("Socket created\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr;
    servaddr.sin_port = htons(serv_port);
    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Error: connection with the server\n");
        exit(0);
    }
    else
        printf("Connected to the server..\n");
    printf("To Server: %s \n", buff);
    write(sockfd, buff, sizeof(buff));
    bzero(buff, sizeof(buff));

    read(sockfd, buff, sizeof(buff));
    printf("From Server: %s \n", buff);

    double wait = difftime(time(NULL), start_time);
    printf("Delay: %f \n", wait);

    close(sockfd);
}
