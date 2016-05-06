#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#define MY_PORT_NUM 49999
struct sockaddr_in SERVER_ADDR;
#define BUFFER_SZ 1024

int isDirectory(char *path);
int makeConnection();
int setUpServerAddr(int socketfd, char* host);
int connectToServer(int socketfd, struct sockaddr_in* servAddr);
void sendCmdToServer(char *cmd, char *path);
int isCmdValid(char *cmd, char* path);
void lsCmd(int socketfd, char * path);
char* cdCmd(int socketfd, char* path);
void getCmd(int socketfd, char* path, int newSocketfd);
void putCmd(int socketfd, char* path, int newSocketfd);
void exitCmd(int socketfd);
int makeDataConnection(int socketfd);
void rcdCmd(int socketfd, char *path);
int readAcknowledgement(int socketfd);
char* extractFileName(char* path);
void rlsCmd(int socketfd, int newSocketfd);
void showCmd(int socketfd, char *path, int newSocketfd);

int main(int argc, char** argv){
	char buffer[512];
	int buffer_sz = 512;
	char cmd[5] = {'\0'};
	char path[256] = {'\0'};
	int flag = 0;
	
	if(argc < 2){
		printf("Too few argument !!\n");
		exit(1);
	}	
	
	int socketfd = makeConnection();
	setUpServerAddr(socketfd, argv[1]);
	
	while(1){
		
		do{
				// reset variable containt
				cmd[0] = '\0';
				path[0] = '\0';
				
				printf("mftp: >> ");
				fgets(buffer, buffer_sz, stdin);
				sscanf(buffer, "%s %s", cmd, path);
				if((flag = isCmdValid(cmd, path)) != 1)
						printf("Invalid command '%s' !! - command ignored\n", cmd);
	
		}while(flag != 1);
		
		if(strcmp(cmd, "ls") == 0) { // working
			getcwd(&path[0], 255);
			lsCmd(socketfd, path);
		}
		else if(strcmp(cmd, "exit") == 0) { // working 
			exitCmd(socketfd);
		}
		else if(strcmp(cmd, "cd") == 0) { // working
			char *temp = cdCmd(socketfd, path);
			printf("working directory changed to: %s\n", temp);
			free(temp);
		}
		else if(strcmp(cmd, "get") == 0) { // working
			int newSocketfd = makeDataConnection(socketfd); 
			if(newSocketfd == -1){printf("Data connection failled ...\n");break;}
			getCmd(socketfd, path, newSocketfd);
		}
		else if(strcmp(cmd, "put") == 0) { // working
			int newSocketfd = makeDataConnection(socketfd);
			if(newSocketfd == -1){printf("Data connection failled ...\n");break;} 
			putCmd(socketfd, path, newSocketfd);
		}
		else if(strcmp(cmd, "rcd") == 0){ // working
			rcdCmd(socketfd, path);
		}
		else if(strcmp(cmd, "rls") == 0){
			int newSocketfd = makeDataConnection(socketfd);
			if(newSocketfd == -1){printf("Data connection failled ...\n");break;}
			rlsCmd(socketfd,  newSocketfd);
		}
		else if(strcmp(cmd, "show") == 0){
			int newSocketfd = makeDataConnection(socketfd);
			if(newSocketfd == -1){printf("Data connection failled ...\n");break;}
			showCmd(socketfd, path, newSocketfd);
		}
		
		if(strcmp(cmd, "exit") == 0) {
			close(socketfd);
			exit(0);
		}
	}

	return 0;
} 

int makeDataConnection(int socketfd){
	int r = 0;
	char portNbr[25] ={'\0'};
	// requesting a data connection
	if((r = write(socketfd, "D\n", 2)) < 0) perror("read in data connection");
	// waiting for port number
	if((r = read(socketfd, portNbr, 25)) < 0) perror("read in data connection");
	
	int my_port;
	sscanf(&portNbr[1], "%i\n", &my_port);	
	if(my_port == 0) return -1;

	// establising new connection
	SERVER_ADDR.sin_port = htons(my_port);
	int newSocketfd = socket(AF_INET, SOCK_STREAM, 0);
	int newConnectfd = connect(newSocketfd, (struct sockaddr *) &SERVER_ADDR, sizeof(SERVER_ADDR));
	
	if(newConnectfd < 0) {perror("connect in data new connection"); exit(1);}	
		
	return newSocketfd;
}

void showCmd(int socketfd, char* path, int newSocketfd){
	char cmd[512] = {'\0'};
	strcpy(cmd, "G");
	strcat(cmd, path);
	strcat(cmd, "\n");
	if(write(socketfd, cmd, strlen(cmd)) < 0) perror("write");
	
	if(readAcknowledgement(socketfd)){
		int pid;
		int fd[2];
		
		pipe(fd);
		pid = fork();
		if(pid < 0) {perror("fork in showCmd"); return;}
		
		if(pid){ // code that runs in parent process
			wait(NULL);
			close(newSocketfd);
		}
		else{ // code that runs in child process
			close(0); dup2(newSocketfd, 0);
			if(execlp("more", "more", "-20", NULL) == -1){
				printf("%s\n", strerror(1)); exit(0);
			}
		}  
  }
}

void rlsCmd(int socketfd, int newSocketfd){
	char cmd[5] = {'\0'};
	strcpy(cmd, "L");
	strcat(cmd, "\n");
	if(write(socketfd, cmd, strlen(cmd)) < 0) perror("write");
	
	if(readAcknowledgement(socketfd)){
		int pid;
		pid = fork();
		if (pid < 0) {perror("fork"); return;}
	
		if(pid){ // code that runs in parent process
			wait(NULL);
			close(newSocketfd);
		}
		else{ // code that runs in child process
			close(0); dup2(newSocketfd, 0);
			if(execlp("more", "more", "-20", NULL) == -1) printf("%s\n", strerror(1));
		}  
	}
}

void rcdCmd(int socketfd, char *path){
	char cmd[512] = {'\0'};
	strcpy(cmd, "C");
	strcat(cmd, path);
	strcat(cmd, "\n");
	if(write(socketfd, cmd, strlen(cmd)) < 0) perror("write");
	readAcknowledgement(socketfd);
}

void exitCmd(int socketfd){
	char cmd[5] = {'\0'};
	strcpy(cmd, "Q");
	strcat(cmd, "\n");
	if(write(socketfd, cmd, strlen(cmd)) < 0) perror("write");
	readAcknowledgement(socketfd);
}

void lsCmd(int socketfd, char *path){
	int rdr, wtr; 
	int fd[2];
	int pid;
	
	if(pipe(fd) < 0){ fprintf(stderr, " Piping failled\n"); return;};
	rdr = fd[0];
	wtr = fd[1];
	
	pid = fork();
  if (pid < 0) {perror("fork"); return;}
  
  if(pid){// code that runs in the parent process
		close(wtr);
			
		if(fork()){
			close(rdr);
			wait(NULL);
			wait(NULL);
		}
		else{
			close(0); dup2(rdr, 0); close(rdr);
			if(execlp("more", "more", "-20",  NULL) == -1) printf("%s\n", strerror(1));
		}
  }
  else{// code that runs in the child process
  	close(1); dup2(wtr, 1); close(wtr); 	
		if(execlp("ls", "ls","-l", path, NULL) == -1) printf("%s\n", strerror(1));
  }		
}

char* cdCmd(int socketfd, char* path){	
		char* temp = NULL;
		if(chdir(path) == -1){ printf("%s\n", strerror(1)); return NULL;}
		return getcwd(temp, 512);
}

void getCmd(int socketfd, char* path, int newSocketfd){
	int fd;
	char cmd[512] = {'\0'};
	strcpy(cmd, "G");
	strcat(cmd, path);
	strcat(cmd, "\n");
	if(write(socketfd, cmd, strlen(cmd)) < 0) perror("write in getCmd");
	char buffer[BUFFER_SZ] = {'\0'};
	int n = 0;
	
	if(readAcknowledgement(socketfd)){
		if((fd = open(path, O_WRONLY | O_CREAT, S_IRWXU)) < 0){
			printf("%s\n", strerror(1)); close(newSocketfd); return;
		}	
		else{
			while((n = read(newSocketfd, &buffer, BUFFER_SZ)) > 0){
				if(write(fd, &buffer, n) < 0) perror("write in getCmd");
				}
			close(fd);
			close(newSocketfd);
		}
	}
}

void putCmd(int socketfd, char* path, int newSocketfd){
	int fd;
	char cmd[512] = {'\0'};
	char buffer[BUFFER_SZ] = {'\0'};
	strcpy(cmd, "P");
	strcat(cmd, path);
	strcat(cmd, "\n");
	int n = 0;
	
	if(((fd = open(path, O_RDONLY)) > 0) && (isDirectory(path) != 0)){
		if(write(socketfd, cmd, strlen(cmd)) < 0) perror("write in putCmd");
		if(readAcknowledgement(socketfd)){
					while((n = read(fd, &buffer, BUFFER_SZ)) > 0){
				if(write(newSocketfd, &buffer, n) < 0) perror("write in putCmd");
			}
		}	
	}	
	else{
		printf("Cannot open file <%s> %s\n",path, strerror(1)); 
		close(newSocketfd); return;
	}
	close(newSocketfd);
	close(fd);  
}

int makeConnection(){
	return socket(AF_INET, SOCK_STREAM, 0);
}

int setUpServerAddr(int socketfd, char* host){
	struct hostent* hostEntry;
	struct in_addr **pptr;

	memset(&SERVER_ADDR, 0, sizeof(SERVER_ADDR));
	SERVER_ADDR.sin_family = AF_INET;
	SERVER_ADDR.sin_port = htons(MY_PORT_NUM);

	hostEntry = gethostbyname(host);
	if(hostEntry == NULL){herror("host name"); exit(1);}

	pptr = (struct in_addr **) hostEntry->h_addr_list;
	memcpy(&SERVER_ADDR.sin_addr, *pptr, sizeof(struct in_addr));

	int connectfd =  connect(socketfd, (struct sockaddr *) &SERVER_ADDR, sizeof(SERVER_ADDR));
	if(connectfd < 0) {perror("connect"); exit(1);}	
	printf("Client connected to server -> %s\n", hostEntry->h_name);
	
	return connectfd;
}

int isCmdValid(char *cmd, char* path){
	// check validity of the argument
	if(cmd[0] == '\0') return 1;
	if(strcmp(cmd, "cd") == 0 && path[0] != '\0') return 1;
	else if(strcmp(cmd, "rcd") == 0 && path[0] != '\0') return 1;
	else if(strcmp(cmd, "get") == 0 && path[0] != '\0') return 1;
	else if(strcmp(cmd, "put") == 0 && path[0] != '\0') return 1;
	else if(strcmp(cmd, "show") == 0 && path[0] != '\0') return 1;
	else if(strcmp(cmd, "ls") == 0 && path[0] == '\0') return 1;
	else if(strcmp(cmd, "rls") == 0 && path[0] == '\0') return 1;
	else if(strcmp(cmd, "exit") == 0 && path[0] == '\0') return 1;
	
	return 0;
}

int readAcknowledgement(int socketfd){	
	char msg[512] = {'\0'};
	int msg_sz = 512;
	
	if(read(socketfd, msg, msg_sz) < 0) perror("read rcd");
	
	if(msg[0] == 'E') {printf("An error occured: %s", &msg[1]); return 0;}
	
	return 1; // should never reach this statement
}

char* extractFileName(char* path){
		const char ch = '/';
		char *ret = strrchr(path,ch);
		if(ret != NULL){
			char* temp = malloc(sizeof(char) * strlen(ret));
			strcpy(temp, &ret[1]);
			temp[strlen(temp)] = '\0';
			return temp;
		} 
	return NULL;
}

int isDirectory(char *path){
	struct stat area;
	struct stat *s = &area;
	if((lstat(path, s) == 0) && (S_ISDIR(s->st_mode)))
		return 0;
	else
		return 1;
}







