#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/file.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define MAX 80
#define SA struct sockaddr



typedef struct my_stack
{
    int data[MAX];
    int top;
}my_stack;

int precedence(char);
void init(my_stack *);
int empty(my_stack *);
int full(my_stack *);
int pop(my_stack *);
void push(my_stack *,int);
int top(my_stack *);   //value of the top element
void infix_to_postfix(char infix[],char postfix[]);
char correct_string (char * str) {
    for (int i = 0; i < strlen(str) - 1; i++)
    if (!isdigit(str[i]) && str[i] != '(' && str[i] != ')' && str[i] != '+' && str[i] != '-' && str[i] != '*' && str[i] != '/')
            return str[i];
    return -1;
}


struct stack{
    long double number;
    struct stack* next;

};

struct dict{                //this should be a hash table map
    struct in_addr ip;
    struct stack* stack;
    pthread_mutex_t personal_mutex;
};

struct thread_arg{
    int sockfd;
    int *dict_len;
    struct in_addr addr;
};

pthread_mutex_t stacks_mutex;
static struct dict *stacks;
static FILE* logfile;
static long start_time;
char *version = "0.4";

static unsigned int wait_time = 0, communism = 0, success = 0, fail = 0, sigusr_flag = 0;
// Function designed for chat between client and server.
void* func(void*);

void sig_func(){       //work in progress
    sigusr_flag = 1;
    char buff[] = "Info will be printed after next request\n";
    write(1, buff, sizeof(buff));
    //write(logfile, buff, sizeof(buff));
}
void sig_exit(){       //work in progress
    exit(0);
}

void daemonize(){
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // working with signals

    struct sigaction sigint = {sig_exit, 0, SA_RESTART};
    sigaction(SIGINT, &sigint, 0);
    struct sigaction sigterm = {sig_exit, 0, SA_RESTART};
    sigaction(SIGTERM, &sigterm, 0);
    struct sigaction sigquit = {sig_exit, 0, SA_RESTART};
    sigaction(SIGQUIT, &sigquit, 0);

    //signal(SIGCHLD,SIG_IGN);
    //signal(SIGTSTP,SIG_IGN);
    /* Set new file permissions */
    umask(0);
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    chdir(cwd);

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
}

// Driver function
int main(int argc, char **argv, char* env[])
{
    int sockfd, connfd, error = 0;
    long current_time;
    in_addr_t addr = INADDR_ANY;
    int serv_port = 8080;
    unsigned int len;
    struct sockaddr_in servaddr;
    char logname[PATH_MAX];
    strcpy(logname, "/tmp/lab2.log");
    struct sigaction sig = {sig_func, 0, SA_RESTART};
    error = sigaction(SIGUSR1, &sig, 0);
    if (error){
        printf("sigaction failed, SIGUSR1 will not be handled \n ");
    }
    //checking the env
    if (getenv("L2WAIT"))
        wait_time = strtol(getenv("L2WAIT"), NULL, 10);

    if (getenv("L2LOGFILE"))
        strcpy(logname, getenv("L2LOGFILE"));

    if (getenv("L2ADDR"))
        addr = inet_addr(getenv("L2ADDR"));

    if (getenv("L2PORT"))
        serv_port = strtol(getenv("L2PORT"), NULL, 10);

    //getopt, old friend
    int c;
    int digit_optind = 0;
    while ((c = getopt(argc, argv, "w:dl:a:p:vhc")) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
            case 'l':
                strcpy(logname, optarg);
                break;
            case 'w':
                wait_time = strtol(optarg, NULL, 10);
                break;
            case 'd':
                daemonize();
                break;
            case 'a':
                addr = inet_addr(optarg);
                break;
            case 'p':
                serv_port = strtol(optarg, NULL, 10);
                break;
            case 'v':
                printf("%s \n", version);
                break;
            case 'c':
                communism = 1;
                break;
            case 'h':
                printf("\n \t This is help message:\n"
                       "This is simple TCP server, it has several startup optiions \n"
                       "-l helps specify logfile name. The default is /tmp/lab2.log \n"
                       "-w specifies waiting time during connection \n"
                       "-d starts server in daemon mode \n"
                       "-a specifies listening address (work in progress).\n"
                       "-p specifies listening port. The default is 8080 \n"
                       "-v outputs program version \n"
                       "-c starts server with one stack \n"
                       "-h prints this message \n");
                break;
            case '?':
                break;
            default:
                break;
        }
    }

    logfile = fopen(logname, "w");


    // socket create and verification

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(logfile, "socket creation failed...\n");
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        fprintf(logfile, "Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));
    fflush(logfile);
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr;
    servaddr.sin_port = htons(serv_port);
    printf("Address:%s \n", inet_ntoa(servaddr.sin_addr));
    // Binding newly created socket to given IP and verification
    error = bind(sockfd, (SA*)&servaddr, sizeof(servaddr));
    if (error != 0) {
        fprintf(logfile, "socket bind failed...\n");
        printf("socket bind failed...\n");
        switch (error){
            case EACCES:
                printf("Error: protected address, try sudo \n");
                break;
            case EADDRINUSE:
                printf("Error: address already in use \n");
                break;
            case EBADF:
                printf("Error: bad socket \n");
                break;

            case EINVAL:
                printf("Error: socket already bound \n");
                break;

            case ENOTSOCK:
                printf("Error: this is not a socket \n");
                break;

            default:
                printf("Unknown error \n");
        }
        printf("Critical error, exiting... \n");
        fflush(logfile);
        exit(2);
    }
    else
        fprintf(logfile, "Socket successfully binded..\n");
    fflush(logfile);
    // Now server is ready to listen and verification


    int i = 0;
    error = 0;
    len = sizeof(struct sockaddr_in);
    stacks = malloc(sizeof(struct dict));
    struct thread_arg *argad = malloc(sizeof(struct thread_arg)), arg = *argad;

    arg.dict_len = malloc(sizeof(int));
    *arg.dict_len = 1;
    stacks[0].ip.s_addr = inet_addr("127.0.0.1");
    stacks[0].stack = malloc(sizeof(struct stack));
    stacks[0].stack->next = NULL;
    stacks[0].stack->next = 0;
    start_time = time(NULL);
    fprintf(logfile,"%s\t Getopt parsed, starting up the server \n", ctime(&start_time));
    pthread_mutex_init(&stacks_mutex, NULL);
    if ((listen(sockfd, 5)) != 0) {
        fprintf(logfile, "Listen failed...\n");
    }
    for (; ;) {

        if (sigusr_flag){
            printf("info: \n");
            int runtime = (int)difftime(time(NULL), start_time);
            printf("time running: %i seconds (approx %i minutes) \n", runtime, runtime/60);
            printf("requests served: %i \n", success);
            printf("errors on requests: %i \n", fail);
            fprintf(logfile, "time running: %i (approx %i minutes) \n", runtime, runtime/60);
            fprintf(logfile, "requests served: %i \n", success);
            fprintf(logfile, "errors on requests: %i \n", fail);
            fflush(logfile);
        }

        struct sockaddr_in cli;
        fflush(logfile);
        // Accept the data packet from client and verification

        connfd = accept(sockfd, (SA*)&cli, &len);
        if (connfd < 0) {
            current_time = time(NULL);
            fprintf(logfile, "%s\t server acccept failed...\n", ctime(&current_time));
        }

        arg.sockfd = connfd;
        arg.addr = cli.sin_addr;

        // chatting between client and server
        pthread_t thread;
        error = pthread_create(&thread, NULL, &func, &arg);
        current_time = time(NULL);
        if (error) fprintf(logfile, "%s\t pthread create failed\n", ctime(&current_time));
        if (error) break;

        error = pthread_detach(thread);
        if (error) fprintf(logfile, "%s \t pthread detach failed, might cause mem leak\n", ctime(&current_time));
        else fprintf(logfile, "%s \t thread successfully created and detached \n", ctime(&current_time));
        fflush(logfile);
    }

    printf("Critical failure, finishing... \n\n");
    exit(1);
}

void* func(void* arg_ptr)
{
    int sockfd, err, uid = 0, req = 0;
    char *err_ptr = NULL;
    struct thread_arg arg = *(struct thread_arg*)arg_ptr;
    sockfd = arg.sockfd;
    struct stack *stack = NULL;
    char buff[MAX], resp[MAX];
    long double n;
    bzero(buff, MAX);
    if (communism) stack = stacks->stack;
    read(sockfd, buff, sizeof(buff));
    // client auth
    long current_time = time(NULL);
    fprintf(logfile, "%s \t Looking for client : %s \n", ctime(&current_time), inet_ntoa(arg.addr));
    for (int i = 0; i < *arg.dict_len; i++){
        if (communism) break;
        if (stacks[i].ip.s_addr == arg.addr.s_addr){
            stack = stacks[i].stack;
            uid = i;
            break;
        }
    };

    pthread_mutex_lock(&stacks_mutex);
    if (stack == NULL){
        fprintf(logfile, "%s \t  client %s not found, creating new entry \n", ctime(&current_time), inet_ntoa(arg.addr));
        stacks = realloc(stacks, (++*arg.dict_len)*sizeof(struct dict));
        uid = *arg.dict_len-1;
        stacks[uid].ip.s_addr = arg.addr.s_addr;
        stacks[uid].stack = malloc(sizeof(stack));
        stack = stacks[uid].stack;
        stack->next = NULL;
        pthread_mutex_init(&stacks[uid].personal_mutex, NULL);
    }
    pthread_mutex_unlock(&stacks_mutex);
    // read the message from client and copy it in buffer


    pthread_mutex_lock(&stacks[uid].personal_mutex);
    fprintf(logfile, "%s \t From client: %s \n", ctime(&current_time),buff);
    bzero(resp, MAX);
    char c = correct_string(buff);
    if (c == -1) {
        fprintf(logfile, "%s \t String is correct\n", ctime(&current_time));
        infix_to_postfix(buff, resp);
        strcpy(buff, resp);
        req = 1;
    }
    else {
        fprintf(logfile, "%s \t Incorrect string\n", ctime(&current_time));
        snprintf(buff, MAX, "ERROR -3: String contains invalidcharaster: %c\n", c);
        req = 1;
    }
    /*if (strncmp(buff, "PUSH", 4) == 0){
        n = strtod(buff+5, &err_ptr);
        if (n == 0 && err_ptr == buff+5){
            bzero(buff, MAX);
            strcpy(buff, "Error 400: This is not a number");
            fail++;
            req = 1;
        }else{
            struct stack *stack_tmp = malloc(sizeof(struct stack));
            stack_tmp->next = stack;
            stack = stack_tmp;
            stack->number = n;
            bzero(buff, MAX);
            strcpy(buff, "200 OK");
            success++;
            req = 1;
        }
    }

    if (strncmp(buff, "POP", 3) == 0){
        if (stack->next == NULL){
            bzero(buff, MAX);
            strcpy(buff, "Error: Empty stack");
            fail++;
            req = 1;
        }else{
            struct stack *stack_tmp = stack;
            n = stack->number;
            stack = stack->next;
            free(stack_tmp);
            bzero(buff, MAX);
            sprintf(buff, "%Le", n);
            success++;
            req = 1;
        }
    }*/

    pthread_mutex_unlock(&stacks[uid].personal_mutex);
    stacks[uid].stack = stack;

    if (req == 0){
        bzero(buff, MAX);
        strcpy(buff, "Error: wrong request");
        fail++;
    }

    sleep(wait_time);

    // print buffer which contains the client contents
    fprintf(logfile, "%s \t To client : %s \n", ctime(&current_time), buff);

    // and send that buffer to client
    write(sockfd, buff, sizeof(buff));
    close(sockfd);
    fflush(logfile);
    ////!!!!!!!!!
    //exit(2);
    //return NULL;
}

void infix_to_postfix(char infix[],char postfix[])
{
    my_stack s;
    char x,token;
    int i,j;    //i-index of infix,j-index of postfix
    init(&s);
    j=0;
    for(i=0;infix[i]!='\0';i++)
    {
        token=infix[i];
        if (!isspace(token)){
        if(isalnum(token))
            postfix[j++]=token;
        else
            if(token=='(')
               push(&s,'(');
        else
            if(token==')')
                while((x=pop(&s))!='(')
                      postfix[j++]=x;
                else
                {
                    while (precedence(token) <= precedence(top(&s)) && !empty(&s))
                    {
                        x=pop(&s);
                        postfix[j++]=x;
                    }
                    push(&s,token);
                }
            }
    }

    while(!empty(&s))
    {
        x=pop(&s);
        postfix[j++]=x;
    }

    postfix[j]='\0';
}

int precedence(char x)
{
    if(x=='(')
        return(0);
    if(x=='+'||x=='-')
        return(1);
    if(x=='*'||x=='/'||x=='%')
        return(2);

    return(3);
}

void init(my_stack *s)
{
    s->top=-1;
}

int empty(my_stack *s)
{
    if(s->top==-1)
        return(1);

    return(0);
}

int full(my_stack *s)
{
    if(s->top==MAX-1)
        return(1);

    return(0);
}

void push(my_stack *s,int x)
{
    s->top=s->top+1;
    s->data[s->top]=x;
}

int pop(my_stack *s)
{
    int x;
    x=s->data[s->top];
    s->top=s->top-1;
    return(x);
}

int top(my_stack *p)
{
    return (p->data[p->top]);
}
