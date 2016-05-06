CC = gcc 
FLAGS =  -g -std=c99 -Wall -pedantic


all: mftpserve mftp

mftpserve: mftpserve.c
		$(CC) $(FLAGS) mftpserve.c -o mftpserve
		
mftp: mftp.c 
		$(CC) $(FLAGS) mftp.c -o mftp



# Used to clean up the solution
# it will remove all .o file and the execution file
clean:
	rm mftp mftpserve
