#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

void signalHandler(int signo);
void *read_from_sock(void *ptr);
int sock;

int main(int argc, char *argv[])
	{
	struct sockaddr_in server;
	struct hostent *hp;
	int size=1025;
	char string[size];
	int count_r, count_w;

	/* Create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	/* Connect socket using name specified by command line. */
	server.sin_family = AF_INET;
	hp = gethostbyname(argv[1]);
	if (hp == 0) {
		fprintf(stderr, "%s: unknown host\n", argv[1]);
		exit(2);
	}
	bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
	server.sin_port = htons(atoi(argv[2]));

	if (connect(sock,(struct sockaddr *) &server,sizeof(server)) < 0) {
		perror("connecting stream socket");
		exit(1);
	}

	signal(SIGINT,signalHandler);
	signal(SIGTERM,signalHandler);
	signal(SIGHUP,signalHandler);
	
	char * header= "\n1.Arithmetic\n\nList of operators:\n  add  -  Addition\n  sub  -  Subtraction\n  mul  -  Multiplication\n  div  -  Division\n\nCommand format: (operator) (operand) (operand)...(operand)\n\nNote: It will be an invalid command if operands contain characters\n\n2.Processes\n\nList of commands:\n run <process name>  -  running a process\n kill <process id>  -  killing an active process\n list  -  viewing list of created processes\n print <statement> - prints the statement on server\n exit - closes all running processes on the server and shuts down client\n\nPress Enter to exit\n---------------------------\n\n";
	count_w= write(STDOUT_FILENO, header, strlen(header));
	if (count_w==-1){
		perror("header");
		exit(1);
	}

	//threading
	pthread_t thread1;
	int ret= pthread_create(&thread1,NULL,read_from_sock, NULL);
	if(ret!=0){
		perror("thread create");
		exit(1);
	}

	while (true){
		cnt:;
		count_r = read(STDIN_FILENO, string, size);
		if (count_r == -1){
			perror("input client");
			exit(1);
		}
		else if (count_r==1){
			goto cnt;
		}
		else{
			count_w= write(sock, string, count_r);
			if (count_w==-1){
				perror("client writing to pipe");
				exit(1);
			}
		}
	}
}

void *read_from_sock(void *ptr){
	int count_r, count_w;
	int output_size=500000;
	char output_buf[output_size];

	signal(SIGINT,signalHandler);
	signal(SIGTERM,signalHandler);

	while(true){
		
		count_r= read(sock, output_buf, output_size-1);
		if (count_r==-1){
			perror("parent reading pipe line183");
			exit(1);
		}
		output_buf[count_r]= '\0';
		
		char* exitFlag;
		char* msg;
		char* token=strtok_r(output_buf," ",&msg);
		if(strcmp(token,"1")==0){
			count_w= write(STDOUT_FILENO,msg, count_r-2);
				if (count_w==-1){
				perror("client printing to screen pt2");
				exit(1);
			}
			char*prompt= "Exiting...\n";
			count_w=write(STDOUT_FILENO, prompt, strlen(prompt));
			if (count_w==-1){
				perror("exit print");
				exit(1);
			}
			close(sock);
			exit(0);
		}
		else{
			count_w= write(STDOUT_FILENO, msg, count_r-2);
			if (count_w==-1){
				perror("client printing to screen pt2");
				exit(1);
			}
			
		}
	}
}

void signalHandler(int signo){
	
	if(signo==SIGTERM){
		char*prompt= "sigterm\n";
		write(STDOUT_FILENO, prompt, strlen(prompt));
	}
	else if(signo==SIGINT){
		char*prompt= "sigint\n";
		write(STDOUT_FILENO, prompt, strlen(prompt));
	}
	int count_w= write(sock, "exit\n", 5);
	if (count_w==-1){
		perror("client writing to pipe");
		exit(1);
	}
}