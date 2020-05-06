#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

void error(const char *msg, int end){
	perror(msg);
	if(end)
		exit(-1);
}

#define PORT_NO		20001
#define SERVER_IP	"127.0.0.1"

int main(int argc, char *argv[]){
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket<0)
		error("socket", 1);
	
	struct sockaddr_in remote_addr;
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(PORT_NO);
	inet_pton(AF_INET, SERVER_IP, &(remote_addr.sin_addr));

	if(connect(client_socket, (struct sockaddr *) &remote_addr, sizeof(struct sockaddr))<0)
		error("connect", 1);

	char *file_name = argv[1];
	fprintf(stdout, "file name: %s\n", file_name);
	int fd = open(file_name, O_RDONLY);
	if(fd<0)
		error("opening file", 1);
	struct stat file_stat;
	if(fstat(fd, &file_stat)<0)
		error("fstat", 1);

	char file_size[256];
	sprintf(file_size, "%d", file_stat.st_size);
	fprintf(stdout, "file size: %s bytes\n", file_size);

	ssize_t len = send(client_socket, file_name, 256, 0);
	if(len<0)
		error("file name", 1);

	len = send(client_socket, file_size, 256, 0);
	if(len<0)
		error("file size", 1);

	long offset = 0;
	int remain_data = file_stat.st_size;
	int sent_bytes = 0;
	while(((sent_bytes = sendfile(client_socket, fd, &offset, 256))>0) && (remain_data>0)){
		remain_data -= sent_bytes;
		fprintf(stdout, "sent %d bytes, %d bytes remaining\n", sent_bytes, remain_data);
	}

	close(client_socket);

	return 0;
}
