#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <wait.h>
#include <pthread.h>


struct Process{
    int pid;
    char name[19];
    char status[11];
    char timeStart[9];
    char timeEnd[9];
    char timeElapsed[9];
    time_t timeStartInSec;

};

typedef struct node {
    long client_id;
    int pipefdContoClw;
    int pipefdCltoConr;
    int chPid;
    struct node * next;
} node_t;

struct fd_for_ch{   //for sending to client handler thread that communicates with conn. handler
    int msgsock;
    int pipefd1r; //pipefd conHand to clHand reading end
    int pipefd2w;  //pipefd cliHand to conHand writing end
};

struct Process list[300];
int list_point;
node_t *head;
long last_id;


void signalHandler(int signo);
void client_handler_death(int signo);
bool contains_char(char token[]);
bool is_num(char token[]);
void* input_thread(void*ptr);
void* ch_input(void*ptr);

int main(void)
{
	int sock, length;
	struct sockaddr_in server;
	int msgsock;
	int count_w, count_r;
    int pipefdContoCl[2];  //conn to CH
    int pipefdCltoCon[2];   //CH to conn
    head=NULL;
    last_id=0;
    pthread_t thread1;

    signal(SIGCHLD,client_handler_death);

    int p= pthread_create(&thread1, NULL, input_thread,NULL);
    if(p!=0){
        perror("thread");
        exit(1);
    }

	/* Create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	/* Name socket using wildcards */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = 0;
	if (bind(sock, (struct sockaddr *) &server, sizeof(server))) {
		perror("binding stream socket");
		exit(1);
	}
	/* Find out assigned port number and print it out */
	length = sizeof(server);
	if (getsockname(sock, (struct sockaddr *) &server, (socklen_t*) &length)) {
		perror("getting socket name");
		exit(1);
	}
    char buf[30];
    int count_p=sprintf(buf, "Socket has port #%d\n", ntohs(server.sin_port));
	count_w= write(STDOUT_FILENO, buf, count_p);
	if (count_w==-1){
        perror("port number print");
        exit(1);
    }

	/* Start accepting connections */
	int le= listen(sock, 5);
    if(le==-1){
        perror("listen");
        exit(1);
    }
	while(true){
        msgsock = accept(sock, 0, 0);
		if (msgsock == -1){
			perror("accept");
            exit(1);
        }
        if(pipe(pipefdContoCl)==-1){
            perror("pipe con to cl");
            exit(1);
        }
        if(pipe(pipefdCltoCon)==-1){
            perror("pipe cl to con");
            exit(1);
        }
        int cpid= fork();
        if (cpid ==-1){
            perror("fork");
            exit(1);
        }
        else if(cpid==0){
            //child: clienthandler
            close(pipefdContoCl[1]);   //conn to ch
            close(pipefdCltoCon[0]);   //ch to conn
            signal(SIGCHLD, signalHandler);
            int count_r, count_w;
            char *token;
            char *ptr;
            char *cmd;
            char delim[] = " ";
            int size=1025;
            char string[size];
            char string_len[5];
            char output_buf[10000];
            int process_limit= 300;
            char print[1024];
            list_point=0;
            pid_t cpid;
            pthread_t thread1;

            struct fd_for_ch mspipe;
            mspipe.msgsock=msgsock;
            mspipe.pipefd1r= pipefdContoCl[0];
            mspipe.pipefd2w= pipefdCltoCon[1];

            int p= pthread_create(&thread1,NULL,ch_input, (void*)&mspipe);
            if(p!=0){
                perror("thread in ch");
                exit(1);
            }
            
            while(true){
                count_r= read(msgsock, string, size-1);
                if (count_r==-1){
                    perror("reading from socket");
                    exit(1);
                }
                else if(count_r==0){
                    goto jump2exit;
                }
                string[count_r]='\0';
                cmd= strtok(string,"\n");
                do{                               //incase multiple commands from client queue up
                    for(int i=0; i<strlen(cmd); i++){
                        string[i]= tolower(cmd[i]);
                    }
                    token = strtok_r(cmd, delim, &ptr);
                    int op;
                    int mode;
                    
                    /*
                        modes
                        0 = arithmetic
                        1 = run process
                        2 = kill process
                        3 = show list
                        4 = exit
                        5 = print
                    */

                    if (strcmp(token, "add") == 0){
                        op = 0;
                        mode=0;
                    }
                    else if (strcmp(token, "sub") == 0){
                        op = 1;
                        mode=0;
                    }
                    else if (strcmp(token, "mul") == 0){
                        op = 2;
                        mode=0;
                    }
                    else if (strcmp(token, "div") == 0){
                        op = 3;
                        mode=0;
                    }
                    else if(strcmp(token, "run") ==0){
                        mode=1;
                    }
                    else if(strcmp(token, "kill")==0){
                        mode=2;
                    }
                    else if(strcmp(token, "list")==0){
                        mode=3;
                    }
                    else if(strcmp(token, "exit")==0){
                        mode=4;
                    }
                    else if(strcmp(token, "print")==0){
                        mode=5;
                    }
                    else {
                        char * prompt= "0 Invalid command\n\n";
                        count_w= write(msgsock, prompt, strlen(prompt));
                        if (count_w==-1){
                            perror("write s to c inv cmd");
                            exit(1);
                        }
                        goto nextcmd;
                    }
                    
                    token= strtok_r(NULL, delim, &ptr);
                    
                    if (token==NULL){
                        if((mode>=0 && mode<=2) || mode==5){   //checking command validity for arithmetic, run, kill, print commands
                            char * prompt= "0 Incomplete command\n\n";
                            count_w= write(msgsock, prompt, strlen(prompt));
                            if (count_w==-1){
                                perror("write stoc incmp cmd");
                                exit(1);
                            }
                            goto nextcmd;
                        }
                    }
                    else if(token!=NULL){
                        if(mode==5){    //checking command validity for print command
                            strcpy(print, token);
                        }
                        else if(mode==3 || mode==4){ //checking command validity for list, exit commands
                            char * prompt= "0 Invalid command\n\n";
                            count_w= write(msgsock, prompt, strlen(prompt));
                            if (count_w==-1){
                                perror("write stoc inv cmd2");
                                exit(1);
                            }
                            goto nextcmd;
                        }
                        else if(mode==2){   //checking command validity for kill command
                            char *temp=strtok_r(NULL,delim, &ptr);
                            if (temp!=NULL){
                                char * prompt= "0 Invalid command: Can only kill 1 process at a time\n\n";
                                count_w= write(msgsock, prompt, strlen(prompt));
                                if (count_w==-1){
                                    perror("write stoc inv cmd");
                                    exit(1);
                                }
                                goto nextcmd;
                            }
                        }
                    }
                    
                    if(mode==0){   //task1: arithmetic
                        float result=0;
                        bool char_check;
                        char_check= contains_char(token);
                        if (char_check==false){
                            result= atof(token);
                        }
                        else{
                            char * prompt= "0 Invalid command: Characters found in operands\n\n";
                            if(write(msgsock, prompt, strlen(prompt))==-1){
                                perror("write to sock arithmetic");
                                exit(1);
                            }
                            goto nextcmd;
                        }
                        token= strtok_r(NULL, delim, &ptr);

                        while (token != NULL)
                        {
                            float num;
                            char_check= contains_char(token);
                            if(char_check==false){
                                switch (op)
                                {
                                case 0:
                                    num= atof(token);
                                    result += num;
                                    break;
                                
                                case 1:
                                    num= atof(token);
                                    result -= num;
                                    break;
                            
                                case 2:
                                    num= atof(token);
                                    result *= num;
                                    break;

                                case 3:
                                    num= atof(token);
                                    result /= num;
                                    break;
                                default:
                                    break;
                                }
                            }
                            else{
                                char * prompt= "0 Invalid command: Characters found in operands\n\n";
                                if(write(msgsock, prompt, strlen(prompt))==-1){
                                    perror("write to sock arithmetic");
                                    exit(1);
                                }
                                goto nextcmd;
                            }
                            token = strtok_r(NULL, delim, &ptr);
                        }
                        
                        int c_out= sprintf(output_buf, "0 Result= %.2f\n\n", result );
                        int cw= write(msgsock, output_buf, c_out);
                        if (cw==-1){
                            perror("Writing to sock arithmetic result");
                            exit(1);
                        }
                    }

                    else if(mode==1){       //task2: run process
                        if(list_point>=process_limit){
                            char * prompt= "0 Failed to run process. Maximum process limit reached\n\n";
                            count_w= write(msgsock, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("s to c max limit");
                                exit(0);
                            }
                            goto nextcmd;
                        }
                        char p_name[19];
                        strncpy(p_name,token,19);
                        strncat(p_name," ",19);
                        strncat(p_name,ptr,19);

                        int pipefd[2];
                        int p= pipe2(pipefd, __O_CLOEXEC);
                        if (p==-1){
                            perror("pipe in server");
                            exit(0);
                        }
                        cpid= fork();
                        if (cpid==-1){
                            perror("fork on server failed");
                            exit(0);
                        }
                        else if(cpid==0){
                            //child
                            close(pipefd[0]);
                            char *argv[10];
                            char *cmd= token;

                            char * arg;
                            argv[0]=cmd;
                            int i=1;
                            if(ptr!=NULL){
                                arg=strtok(ptr," ");
                                while(arg!=NULL){
                                    argv[i]=arg;
                                    arg= strtok(NULL," ");
                                    i++;
                                }
                            }
                            argv[i]=NULL;
                            int exec= execvp(cmd, argv);
                            if(exec==-1){
                                perror("exec failed");
                                count_w= write(pipefd[1], "1\0", 2);
                                if (count_w==-1){
                                    perror("write for exec");
                                    exit(0);
                                }
                                exit(0);
                            }
                        }
                        else {
                            //parent
                            close(pipefd[1]);
                            char in[2];
                            int count_r= read(pipefd[0], in, 2);
                            if (count_r==-1){
                                perror("exec read failed");
                                exit(0);
                            }
                            else if (count_r==0){
                                //success
                                int t= time(NULL);
                                int hours= (((t/3600)%24)+5)%24;
                                int mins= (t/60)%60;
                                int sec= t%60;
                                char timeStamp[9];
                                int c_p= sprintf(timeStamp, "%d:%d:%d", hours,mins,sec);
                                char * prompt= "0 Running process\n\n";
                                count_w= write(msgsock, prompt, strlen(prompt));
                                if (count_w==-1){
                                    perror("run success write");
                                    exit(0);
                                }
                                list[list_point].pid=cpid;
                                strncpy(list[list_point].name,p_name,19);
                                strcpy(list[list_point].status,"active");
                                list[list_point].timeStartInSec=t;
                                strncpy(list[list_point].timeStart,timeStamp,c_p);
                                strncpy(list[list_point].timeEnd,"-",1);
                                list_point++;
                            }
                            else {
                                //fail
                                char * prompt= "0 Failed to run process\n\n";
                                count_w= write(msgsock, prompt, strlen(prompt));
                                if (count_w==-1){
                                    perror("run fail write");
                                    exit(0);
                                }
                            }
                        }
                    }
                    else if(mode==2){    //task3: kill process
                        
                        int pid;
                        if(!is_num(token)){
                            char *prompt= "0 Second argument must be a process id\n\n";
                            count_w= write(msgsock,prompt,strlen(prompt));
                            if(count_w==-1){
                                perror("2ng arg is string in kill");
                                exit(1);
                            }
                            continue;
                        }
                        int count_s= sscanf(token, "%d", &pid);
                        if(count_s==-1){
                            perror("pid conversion");
                            exit(1);
                        }
                        for (int i=0; i<list_point; i++){
                            if(list[i].pid==pid){
                                int error= kill(list[i].pid, SIGTERM);
                                if (error==-1){
                                    char * prompt= "0 Process has already been terminated\n\n";
                                    count_w= write(msgsock, prompt, strlen(prompt));
                                    if(count_w==-1){
                                        perror("s to c termination");
                                        exit(0);
                                    }
                                    goto nextcmd;
                                }
                                else {
                                    int t= time(NULL);
                                    int hours= (((t/3600)%24)+5)%24;
                                    int mins= (t/60)%60;
                                    int sec= t%60;
                                    char timeStamp[9];
                                    int c_p1= sprintf(timeStamp, "%d:%d:%d", hours,mins,sec);
                                    int secondsPassed= t-list[i].timeStartInSec;
                                    hours= ((secondsPassed/3600)%24);
                                    mins= (secondsPassed/60)%60;
                                    sec= secondsPassed%60;
                                    char timeElapsed[9];
                                    int c_p2= sprintf(timeElapsed, "%d:%d:%d", hours,mins,sec);
                                    int wstatus;
                                    waitpid(list[i].pid, &wstatus, 0);
                                    strcpy(list[i].status,"terminated");
                                    strncpy(list[i].timeEnd,timeStamp,c_p1);
                                    strncpy(list[i].timeElapsed,timeElapsed,c_p2);
                                    char * prompt= "0 Process terminated successfully\n\n";
                                    count_w= write(msgsock, prompt, strlen(prompt));
                                    if(count_w==-1){
                                        perror("s to c termination");
                                        exit(0);
                                    }
                                    goto nextcmd;
                                }
                            }
                        }
                        char * prompt= "0 Process does not exist\n\n";
                        count_w= write(msgsock, prompt, strlen(prompt));
                        if(count_w==-1){
                            perror("s to c termination");
                            exit(0);
                        }
                    }

                    else if(mode==3){    //task4: list
                        if(list_point==0){
                            char *prompt= "0 List is empty\n\n";
                            count_w= write(msgsock, prompt, strlen(prompt));
                            if (count_w==-1){
                                perror("list write to child");
                                exit(1);
                            }    
                        }
                        else {
                            int i, sp;
                            char buf[500000];
                            sprintf(buf, "%-*s%-*s%-*s%-*s%-*s%-*s\n",9,"0 PID",20,"Process name",15,"Status",15,"Start time",15,"End time",15,"Time Elapsed");
                            char temp_buf[10000];

                            for(i=0; i<list_point; i++){
                                if(strcmp(list[i].status, "active")==0){
                                    int curTime= time(NULL);
                                    int secondsPassed= curTime-list[i].timeStartInSec;
                                    int hours= ((secondsPassed/3600)%24);
                                    int mins= (secondsPassed/60)%60;
                                    int sec= secondsPassed%60;
                                    char timeElapsed[9];
                                    int c_p= sprintf(timeElapsed, "%d:%d:%d", hours,mins,sec);
                                    sp= sprintf(temp_buf, "%-*d%-*s%-*s%-*s%-*s%-*s\n", 7,list[i].pid, 20,list[i].name, 15, list[i].status, 15,list[i].timeStart, 15, list[i].timeEnd, 15, timeElapsed);

                                }
                                else if(strcmp(list[i].status,"terminated")==0){
                                    sp= sprintf(temp_buf, "%-*d%-*s%-*s%-*s%-*s%-*s\n", 7,list[i].pid, 20,list[i].name, 15,list[i].status, 15,list[i].timeStart,15, list[i].timeEnd, 15,list[i].timeElapsed);
                                }
                                
                                strncat(buf,temp_buf,sp);
                            }
                            strcat(buf, "\n");
                            count_w= write(msgsock, buf, strlen(buf));
                            if (count_w==-1){
                                perror("list write to child");
                                exit(1);
                            }
                        }
                    }
                    else if(mode==4){    //task5: exit
                        jump2exit:;
                        for (int i=0; i<list_point; i++){
                            if(strcmp(list[i].status,"terminated")==0){
                                continue;
                            }
                            else {
                                int error= kill(list[i].pid, SIGTERM);
                                if(error==-1){
                                    perror("kill failed in exit");
                                    exit(1);
                                }
                            }
                        }
                        char* prompt= "1 All processes shut down. Disconnecting\n";
                        count_w= write(msgsock, prompt, strlen(prompt));
                        if (count_w==-1){
                            perror("s to c write in exit");
                            exit(1);
                        }
                        exit(0);
                    }
                    else if(mode==5){    //task6: print on server
                        strcat(print, " ");
                        strcat(print, ptr);
                        strcat(print, "\n");
                        count_w= write(STDOUT_FILENO, print, strlen(print));
                        if (count_w==-1){
                            perror("write to screen failed in Print");
                            exit(1);
                        }
                        char* prompt= "0 Print on server successful\n\n";
                        count_w= write(msgsock, prompt, strlen(prompt));
                        if (count_w==-1){
                            perror("s to c write in exit");
                            exit(1);
                        }
                    }
                    nextcmd:;
                    cmd= strtok(NULL, ";");
                }while(cmd!=NULL);
            } 
        }
        else {
            //parent: connection handler
            close(pipefdContoCl[0]);
            close(pipefdCltoCon[1]);
            if(head==NULL){
                node_t * new= (node_t *)malloc(sizeof(node_t));
                new->client_id= last_id+1;
                new->pipefdContoClw= pipefdContoCl[1];
                new->pipefdCltoConr= pipefdCltoCon[0];
                new->chPid= cpid;
                new->next= NULL;
                head=new;
                last_id++;

            }
            else{
                node_t *current =head;
                while(current->next!=NULL){
                    current= current->next;
                }
                node_t *new= (node_t*)malloc(sizeof(node_t));
                new->client_id= last_id+1;
                new->pipefdContoClw= pipefdContoCl[1];
                new->pipefdCltoConr= pipefdCltoCon[0];
                new->chPid= cpid;
                new->next= NULL;
                current->next=new;
                last_id++;

            }
        }
    }	
}
void* input_thread(void*ptr){
    char string_th[500];
    char *token_th;
    char *msg_th;
    char cmd_th[500];
    char output_buf[500000];
    char temp_buf[100];
    int count_r, count_w;

    signal(SIGCHLD,client_handler_death);
    
    while(true){
        count_r= read(STDIN_FILENO, string_th, 500);
        if(count_r==-1){
            perror("read from user input thread");
            exit(1);
        }
        if(head==NULL){
            char *prompt= "No connections\n\n";
            count_w= write(STDOUT_FILENO,prompt,strlen(prompt));
            if(count_w==-1){
                perror("no conn");
                exit(1);
            }
            continue;
        }
        string_th[count_r-1]='\0';
        token_th= strtok_r(string_th, " ", &msg_th);
        strcpy(cmd_th,token_th);
        token_th= strtok_r(NULL, " ", &msg_th);
        if(token_th==NULL){
            char *prompt= "Invalid Command: Missing arguments\n\n";
            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
            if(count_w==-1){
                perror("2nd arg null");
                exit(1);
            }
            continue;
        }
        
        if (!is_num(token_th)){
            char *prompt= "Invalid Command: 2nd argument should be clientID[0: for all clients, or positive int]\n\n";
            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
            if(count_w==-1){
                perror("2nd arg nonnumeric");
                exit(1);
            }
            continue;
        }
        else{
            long id= atol(token_th);
            if(id>last_id){
                char *prompt= "Client does not exist\n\n";
                count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                if(count_w==-1){
                    perror("client invalid");
                    exit(1);
                }
                continue;
            }
            else{
                if(strcmp(cmd_th,"print")==0){
                    char optbuf[500];
                    strcat(cmd_th," 0 ");
                    strcat(cmd_th,msg_th);
                    if(id==0){
                        //all clients
                        node_t *current=head;
                        while(current!=NULL){
                            int spf= sprintf(optbuf,"%s\n\n",cmd_th);
                            count_w= write(current->pipefdContoClw, optbuf, spf);
                            if(count_w==-1){
                                perror("input to CH");
                                exit(1);
                            }
                            current= current->next;
                        }
                        
                    }
                    else{
                        //print to specific client
                        node_t *current=head;
                        while(current!=NULL && current->client_id!=id && current->client_id<last_id){
                            current= current->next;
                        }
                        
                        if(current==NULL){
                            char*prompt= "Client has disconnected\n\n";
                            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("client invalid");
                                exit(1);
                            }
                            continue;
                        }
                        else if(current->client_id==id){
                            int spf= sprintf(optbuf,"%s\n\n",cmd_th);
                            count_w= write(current->pipefdContoClw, optbuf, spf);
                            if(count_w==-1){
                                perror("input to CH");
                                exit(1);
                            }
                        }
                        else{
                            char *prompt= "Client does not exist\n\n";
                            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("client invalid");
                                exit(1);
                            }
                            continue;
                        }
                    }
                }
                else if(strcmp(cmd_th, "list")==0){
                    if(id==0){
                        //all clients
                        token_th=strtok_r(NULL," ",&msg_th);
                        if(token_th!=NULL){
                            char *prompt= "Invalid Command: Too many arguments\n\n";
                            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("client invalid");
                                exit(1);
                            }
                            continue;
                        }
                        node_t *current=head;
                        while(current!=NULL){
                            count_w= write(current->pipefdContoClw, cmd_th, strlen(cmd_th));
                            if(count_w==-1){
                                perror("input to CH");
                                exit(1);
                            }
                            count_r= read(current->pipefdCltoConr,output_buf,500000);
                            if(count_r==-1){
                                perror("input thread read from ch");
                                exit(1);
                            }
                            if(count_r==0){
                                perror("ch to cn pipe broken print all");
                            }
                            else{
                                output_buf[count_r]='\0';
                                int spf= sprintf(temp_buf,"-----------------List from client %ld------------------\n",current->client_id);
                                count_w= write(STDOUT_FILENO, temp_buf, spf);
                                if(count_w==-1){
                                    perror("input thread write to screen print all");
                                    exit(1);
                                }
                                count_w= write(STDOUT_FILENO, output_buf, count_r);
                                if(count_w==-1){
                                    perror("input thread write to screen print all");
                                    exit(1);
                                }
                            }
                            
                            current= current->next;
                        }
                
                    }
                    else{
                        //send to specific client
                        token_th=strtok_r(NULL," ",&msg_th);
                        if(token_th!=NULL){
                            char *prompt= "Invalid Command: Too many arguments\n\n";
                            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("client invalid");
                                exit(1);
                            }
                            continue;
                        }
                        node_t *current=head;
                        
                        while(current!=NULL && current->client_id!=id && current->client_id<last_id){
                            current= current->next;
                        }

                        if(current==NULL){
                            char*prompt= "Client has disconnected\n\n";
                            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("client invalid");
                                exit(1);
                            }
                            continue;
                        }
                        else if(current->client_id==id){
                            count_w= write(current->pipefdContoClw, cmd_th, strlen(cmd_th));
                            if(count_w==-1){
                                perror("input to CH");
                                exit(1);
                            }
                            count_r= read(current->pipefdCltoConr,output_buf,500000);
                            if(count_r==-1){
                                perror("input thread read from ch");
                                exit(1);
                            }
                            if(count_r==0){
                                perror("ch to cn pipe broken print all");
                            }
                            else{
                                output_buf[count_r]='\0';
                                int spf= sprintf(temp_buf,"-----------------List from client %ld------------------\n",current->client_id);
                                count_w= write(STDOUT_FILENO, temp_buf, spf);
                                if(count_w==-1){
                                    perror("input thread write to screen print all");
                                    exit(1);
                                }
                                count_w= write(STDOUT_FILENO, output_buf, count_r);
                                if(count_w==-1){
                                    perror("input thread write to screen print all");
                                    exit(1);
                                }
                            }
                        }
                        else{
                            char *prompt= "Client does not exist\n\n";
                            count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                            if(count_w==-1){
                                perror("client invalid");
                                exit(1);
                            }
                            continue;
                        }
                    }
                }
                else{
                    char *prompt= "Invalid Command\n\n";
                    count_w= write(STDOUT_FILENO, prompt, strlen(prompt));
                    if(count_w==-1){
                        perror("inval cmd");
                        exit(1);
                    }
                }
            }
        }
    }
}

void* ch_input(void*ptr){
    char string[500];
    char *cmd;
    char *msg;
    char buf[50];
    int count_w, count_r;
    int msgsock,pipefd1r,pipefd2w;

    signal(SIGCHLD, signalHandler);

    struct fd_for_ch *mspipe= (struct fd_for_ch*)ptr;
    msgsock= mspipe->msgsock;
    pipefd1r= mspipe->pipefd1r;
    pipefd2w= mspipe->pipefd2w;
    
    while(true){ 
        count_r=read(pipefd1r,string,500);
        if(count_r==-1){
            perror("read ch input");
            exit(1);
        }
        
        string[count_r]='\0';
        cmd= strtok_r(string," ",&msg);
        
        if(strcmp(cmd,"print")==0){
            count_w= write(msgsock,msg,strlen(msg));
            if(count_w==-1){
                perror("ch to client in thread");
                exit(1);
            }
        }
        else if(strcmp(cmd,"list")==0){
            if(list_point==0){
                char *prompt= "List is empty\n\n";
                count_w= write(pipefd2w, prompt, strlen(prompt));
                if (count_w==-1){
                    perror("list write to child");
                    exit(1);
                }
            }
            else {
                int i, sp;
                char buf[500000];
                sprintf(buf, "%-*s%-*s%-*s%-*s%-*s%-*s\n",7,"PID",20,"Process name",15,"Status",15,"Start time",15,"End time",15,"Time Elapsed");
                char temp_buf[10000];

                for(i=0; i<list_point; i++){
                    if(strcmp(list[i].status, "active")==0){
                        int curTime= time(NULL);
                        int secondsPassed= curTime-list[i].timeStartInSec;
                        int hours= ((secondsPassed/3600)%24);
                        int mins= (secondsPassed/60)%60;
                        int sec= secondsPassed%60;
                        char timeElapsed[9];
                        int c_p= sprintf(timeElapsed, "%d:%d:%d", hours,mins,sec);
                        sp= sprintf(temp_buf, "%-*d%-*s%-*s%-*s%-*s%-*s\n", 7,list[i].pid, 20,list[i].name, 15, list[i].status, 15,list[i].timeStart, 15, list[i].timeEnd, 15, timeElapsed);

                    }
                    else if(strcmp(list[i].status, "terminated")==0){
                        sp= sprintf(temp_buf, "%-*d%-*s%-*s%-*s%-*s%-*s\n", 7,list[i].pid, 20,list[i].name, 15, list[i].status, 15,list[i].timeStart, 15, list[i].timeEnd, 15,list[i].timeElapsed);
                    }
                    
                    strncat(buf,temp_buf,sp);
                }
                strcat(buf, "\n");
                count_w= write(pipefd2w, buf, strlen(buf));
                if (count_w==-1){
                    perror("list write to child");
                    exit(1);
                }
            }
        }
    } 
}


void signalHandler(int signo){
    int status;
    pid_t cpid;
    while(true){
        cpid= waitpid(0, &status, WNOHANG);
        if (cpid>0){
            for (int i=0; i<list_point; i++){
                if(list[i].pid==cpid){
                    int t= time(NULL);
                    int hours= (((t/3600)%24)+5)%24;
                    int mins= (t/60)%60;
                    int sec= t%60;
                    char timeStamp[9];
                    int c_p1= sprintf(timeStamp, "%d:%d:%d", hours,mins,sec);
                    int secondsPassed= t-list[i].timeStartInSec;
                    hours= ((secondsPassed/3600)%24);
                    mins= (secondsPassed/60)%60;
                    sec= secondsPassed%60;
                    char timeElapsed[9];
                    int c_p2= sprintf(timeElapsed, "%d:%d:%d", hours,mins,sec);
                    strcpy(list[i].status,"terminated");
                    strncpy(list[i].timeEnd,timeStamp,c_p1);
                    strncpy(list[i].timeElapsed,timeElapsed,c_p2);
                    char *prompt= "Child has been terminated\n";
                    write(STDOUT_FILENO, prompt, strlen(prompt));
                }
            }
        }
        else if(cpid==0){
            break;
        }
        else{
            if(errno==ECHILD){
                break;
            }
            else{
                perror("signalHandler wait");
                exit(1);
            }
        }
    }
}

void client_handler_death(int signo){
    int status;
    pid_t cpid;

    while(true){
        cpid=waitpid(0,&status, WNOHANG);
        if(cpid==0){
            break;
        }
        else if(cpid>0){
            node_t *current= head;
            node_t *temp;
            if(head->chPid==cpid){
                temp=head->next;
                free(head);
                head=temp;
            }
            else{
                while(current->next->chPid!=cpid){
                    current=current->next;
                }
                temp=current->next;
                current->next=temp->next;
                free(temp);
            }
        }
        else{
            if(errno==ECHILD){
                break;
            }
            else{
                perror("ch hanlder death wait");
                exit(1);
            }
        }
    }
}

bool contains_char(char token[]){
    for (int i = 0; i < strlen(token); i++)
    {
        if(i==0){
            if(token[i]>=33 && token[i]<=42 || token[i]==44 || token[i]==47 ||token[i]>=58 && token[i]<=126){  //exclude characters except '.' '-' '+'
                return true;
            }    
        }
        else{
            if(token[i]>=33 && token[i]<=45||token[i]==47||token[i]>=58 && token[i]<=126){ //exclude characters in the middle and at the end
                return true;
            }
        }

    }
    return false;
}

bool is_num(char token[]){
    for (int i=0; i<strlen(token);i++){
        if(isdigit(token[i])==0){
            return false;
        }
    }
    return true;
}