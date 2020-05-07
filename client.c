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
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#define PORT_NO			20001
#define SERVER_IP		"127.0.0.1"
#define FILE_UP			0
#define FILE_DOWN		1
#define FILE_NEWEST		2
#define FILE_LIST		3

void error(const char *msg, char end){
	perror(msg);
	if(end)
		exit(-1);
}

int print_help(){
	printf(
		"Usage: jef [option] [file]\n"
		"options:\n"
		"-d <file_name>\t-> download file from server\n"
		"-n [number]\t-> download n latest files from server, default 1\n"
		"-l\t\t-> list files on the server uploade in less than x hours (specified by server configuration)\n"
		"-r <file_name>\t-> remove file from server\n"
	);
	return 0;
}

void invalid_args(){
	printf("Invalid arguments provided\n\n");
	print_help();
	exit(-1);
}

int get_conn(){
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket<0)
		error("socket", 1);
	
	struct sockaddr_in remote_addr;
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(PORT_NO);
	inet_pton(AF_INET, SERVER_IP, &(remote_addr.sin_addr));

	if(connect(client_socket, (struct sockaddr *) &remote_addr, sizeof(struct sockaddr))<0)
		error("connect", 1);

	return client_socket;
}

void handshake(int s_sock, unsigned char f_type){
	short header = 0x1ef0 | (f_type & 0xf);
	ssize_t len = send(s_sock, &header, sizeof(header), 0);
	if(len<0)
		error("handshake", 1);
}

int main(int argc, char *argv[]){
	if(argc==1)
		return print_help();
	else if(argc>3)
		invalid_args();

	int c = getopt(argc, argv, "dnl");
	if(c=='d'){
		if(argc!=3)
			invalid_args();

		int client_socket = get_conn();
		handshake(client_socket, FILE_DOWN);

		char *file_name = argv[optind];
		ssize_t len = send(client_socket, file_name, 256, 0);
		if(len<0)
			error("file name", 1);
		printf("file name: %s\n", file_name);

		char f_len_buff[32];
		recv(client_socket, f_len_buff, 32, 0);
		int file_size = atoi(f_len_buff);
		if(file_size<0)
			error("no file", 1);
		printf("file size: %d\n", file_size);

		char buffer[256];
		int remain_data = file_size;
		FILE *rec_file;
		rec_file = fopen(file_name, "w");
		if(rec_file == NULL)
			error("file", 1);

		while((remain_data>0) && (len = recv(client_socket, buffer, 256, 0))>0){
			fwrite(buffer, sizeof(char), len, rec_file);
			remain_data -= len;
			printf("\rreceived: %d bytes, %d bytes remaining", len, remain_data);
		}
		printf("\n");
		close(client_socket);
	}else if(c=='n'){

	}else if(c=='l'){
		printf("tutaj\n");
	}else{
		int client_socket = get_conn();

		handshake(client_socket, FILE_UP);

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
			fprintf(stdout, "\rsent %d bytes, %d bytes remaining", sent_bytes, remain_data);
		}
		printf("\n");
		close(client_socket);
	}
	return 0;
}
