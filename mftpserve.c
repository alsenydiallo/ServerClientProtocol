#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#define MY_PORT_NUM 49999
#define BUFFER_SZ 1024

int isDirectory(char *path);
void bindSocketToPort(int listenfd);
int makeSocket();
int listen_accept_connect(int listenfd);
char* getHostName(const struct sockaddr_in clientAddr);
void readCmd(int connectfd, char *buffer);
int newConnection(int connectfd);
void cdCmd(int connectfd, char* path);
void getCmd(int connectfd, char* path, int newConnectfd);
char* extractFileName(char* path);
void sendAcknowledgement(int connectfd);
void putCmd(int connectfd, char* path, int newConnectfd);
void rlsCmd(int connectfd, char* path, int newConnectfd);
void senderror(int connectfd, char *error);


int main(int argc, char** argv){
	int socketfd = makeSocket();
	bindSocketToPort(socketfd);

	pid_t pid;
	int connectfd;
	if(listen(socketfd, 4) == -1) perror("listen");
	
	while((connectfd = listen_accept_connect(socketfd)) > 0){
		
		pid = fork();
		if(pid < 0){ printf("fork failed\n"); exit(1);}
		
		if(pid){ // parent process
			close(connectfd);
		}
		else{// child process
			while(1){
				int newConnectfd;
				char cmd = '\0';
				char path[BUFFER_SZ] = {'\0'};
				char buffer[BUFFER_SZ] = {'\0'};
				printf("\nWaiting for a command ...\n");
				
				readCmd(connectfd, buffer);
				if(buffer == NULL) break;
				cmd = buffer[0]; 
				strncpy(path, &buffer[1], (strlen(buffer)-1));
				printf("Pid %d: executing command %c %s\n", getpid(), cmd, path);
				switch(cmd){
					case 'Q': // working
						printf("process %d exiting ...\n", getpid());
						exit(0);
						break;
					case 'C': // working
						cdCmd(connectfd, path);
						break;
					case 'L':
						getcwd(path, BUFFER_SZ);
						rlsCmd(connectfd, path, newConnectfd); 
						break;
					case 'G': // working
						getCmd(connectfd, path, newConnectfd);
						break;
					case 'P': // working
						putCmd(connectfd, path, newConnectfd);
						break;
					case 'D': // working
						newConnectfd = newConnection(connectfd);
					break;
					default:
						senderror(connectfd, "Unknown command recieved !!\n");
				}
		  }
		}
	}
	return 0;
}

void rlsCmd(int connectfd, char* path, int newConnectfd){
	int pid;

	pid = fork();
  if (pid < 0) {perror("fork"); return;}
  
  if(pid){// code that runs in the parent process
		close(newConnectfd);
		wait(NULL);
  }
  else{// code that runs in the child process
  	sendAcknowledgement(connectfd);
		close(1); dup2(newConnectfd, 1);
		if(execlp("ls", "ls","-l", path, NULL) == -1){
			senderror(connectfd, strerror(1)); exit(1);
		}
	}		
}

void putCmd(int connectfd, char* path, int newConnectfd){
	int fd;	
	char* temp = extractFileName(path);
	char* fileName = malloc(sizeof(char) * (strlen(temp)+1));
	strcpy(fileName, temp);
	char buffer[BUFFER_SZ] = {'\0'};
	int n =0;
	
	if((fd = open(path, O_WRONLY | O_CREAT, S_IRWXU)) < 0){
			printf("%s\n", strerror(1));
			senderror(connectfd, strerror(1)); close(newConnectfd); return;
	}	
	else{
		sendAcknowledgement(connectfd);	
		printf("receiving file <%s> from client ...\n", fileName);
		while((n = read(newConnectfd, &buffer, BUFFER_SZ)) > 0){
				if(write(fd, &buffer, n) < 0) {
					senderror(connectfd, strerror(1));
				}
			}
		close(newConnectfd);
	}
	free(fileName);
	close(fd);
}

void getCmd(int connectfd, char* path, int newConnectfd){
	int fd;
	char buffer[BUFFER_SZ] = {'\0'};
	int n = 0;
	
	if(((fd = open(path, O_RDONLY)) > 0) && (isDirectory(path) != 0)){
		sendAcknowledgement(connectfd);
		printf("Transfering file <%s> to client ...\n", path);
		while((n = read(fd, &buffer, BUFFER_SZ)) > 0)
			write(newConnectfd, &buffer, n);
		close(fd);
	}
	else{
		senderror(connectfd, strerror(1));
	}
	close(newConnectfd);
}

void cdCmd(int connectfd, char* path){	
	if(path == NULL) return;

	if(chdir(path) == -1){
		char str[] = {"directory could not be open"};
		senderror(connectfd, str);
		printf("cd to [%s] -> %s\n", path, strerror(1));
		return;
	}		
	printf("Server working directory changed to -> <%s>\n", path);
	sendAcknowledgement(connectfd);
}

void readCmd(int connectfd, char *buffer){
	int r = 0, i =0;
	char ch = '\0';
	while(1){
		if((r = read(connectfd, &ch, 1)) > 0){
				if(ch == '\n') break;
				 buffer[i] = ch;
				 i++;
		}
		else {
			senderror(connectfd, strerror(1)); return;
		}
	}
	buffer[i] = '\0';
}

int makeSocket(){
	return socket(AF_INET, SOCK_STREAM, 0);
}

void bindSocketToPort(int listenfd){
	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(MY_PORT_NUM);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(listenfd, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0) {
		perror("bind"); exit(1);
	}
}

int listen_accept_connect(int listenfd){
	socklen_t length = sizeof(struct sockaddr_in);
	struct sockaddr_in clientAddr;
	int connectfd = accept(listenfd, (struct sockaddr*) &clientAddr, &length);

	if(connectfd < 0) {
		perror("connect"); exit(1);
	}
	printf("Server connected to host -> %s\n", getHostName(clientAddr));
	return connectfd;	
}

char* getHostName(const struct sockaddr_in clientAddr){
	struct hostent* hostEntry;
	char* hostname;

	hostEntry = gethostbyaddr(&(clientAddr.sin_addr), sizeof(struct in_addr), AF_INET);
	hostname = hostEntry->h_name;
	if(hostname == NULL) {
		herror("host name"); exit(1);
	}
	return hostname;
}


int newConnection(int connectfd){
	int newSocketfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(0);
	dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	socklen_t length = sizeof(struct sockaddr_in);
		
	if(bind(newSocketfd, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0){
		senderror(connectfd, strerror(1));
	}
		
	if(getsockname(newSocketfd, (struct sockaddr*) &dest_addr, &length) < 0){
		senderror(connectfd, strerror(1));
	}
	
	int w=0;
	int portNbr = ntohs(dest_addr.sin_port);
	char temp[25] = {'\0'};
	temp[0] = 'A';
	printf("Establishing a data connection to port #%d ...\n", portNbr);
	sprintf(&temp[1], "%i\n", portNbr);
	
	if(listen(newSocketfd, 1) == -1) {
		senderror(connectfd, strerror(1));
	}
		
	if((w = write(connectfd, temp, strlen(temp))) < 0) {
		senderror(connectfd, strerror(1)); return -1;
	}
	
	int newConnectfd = accept(newSocketfd, (struct sockaddr*) &dest_addr, &length);
	if(newConnectfd < 0) {
		senderror(connectfd, strerror(1));
	}	
	return newConnectfd;
}

char* extractFileName(char* path){
		const char ch = '/';
		char* ret = strrchr(path,ch);
		if(ret != NULL){
			char* temp = malloc(sizeof(char) * strlen(ret));
			strcpy(temp, &ret[1]);
			temp[strlen(temp)] = '\0';
			return temp;
		} 
	return path;
}

void sendAcknowledgement(int connectfd){
	int w=0;
	char msg[2] ={'A', '\n'};
	if((w = write(connectfd, msg, 2)) < 0) perror("write");
}

void senderror(int connectfd, char *error){
	char msg[BUFFER_SZ] = {'\0'};
	strcpy(msg, "E");
	strcat(msg, error);
	strcat(msg, "\n");
	write(connectfd, msg, strlen(msg));
}

int isDirectory(char *path){
	struct stat area;
	struct stat *s = &area;
	if((lstat(path, s) == 0) && (S_ISDIR(s->st_mode)))
		return 0;
	else
		return 1;
}


