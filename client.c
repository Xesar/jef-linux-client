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
#include <time.h>

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

short get_conn(){
	short client_socket = socket(AF_INET, SOCK_STREAM, 0);
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

void handshake(short socket_id, unsigned char h_type){
	short header = 0x1ef0 | (h_type & 0xf);
	ssize_t len = send(socket_id, &header, sizeof(header), 0);
	if(len<0)
		error("handshake", 1);
}

void send_file_amount(short socket_id, short file_amount){
	ssize_t len = send(socket_id, &file_amount, sizeof(short), 0);
	if(len<0)
		error("file amount: ",1);
}

void send_file_name(short socket_id, char * file_name){
	char * ptr;
	ptr = strrchr(file_name, '/');
	if(ptr!=NULL)
		strcpy(file_name, ptr+1);
	ssize_t len = send(socket_id, file_name, 256, 0);
	if(len<0)
		error("file name", 1);
}

void send_file_size(short socket_id, int file_size){
	ssize_t len = send(socket_id, &file_size, sizeof(file_size), 0);
	if(len<0)
		error("file name", 1);
}

void send_file(short socket_id, short fd, int file_size){
	long offset = 0;
	int remain_data = file_size;
	int sent_bytes = 0;
	while(((sent_bytes = sendfile(socket_id, fd, &offset, 256))>0) && (remain_data>0)){
		remain_data -= sent_bytes;
		printf("sent: %d\tdone: %.2f%%\n", sent_bytes, 100.0-(remain_data*1.0)/(file_size*1.0)*100.0);
	}
}

const char * recv_file_name(short socket_id){
	static char file_name[256];
	ssize_t len = recv(socket_id, file_name, 256, 0);
	if(len<0)
		error("file name", 1);
	char * ptr;
	ptr = strrchr(file_name, '/');
	if(ptr!=NULL)
		strcpy(file_name, ptr+1);
	return file_name;
}

int recv_file_size(short socket_id){
	int file_size;
	ssize_t len = recv(socket_id, &file_size, sizeof(file_size), 0);
	if(len<0)
		error("no file", 1);
	return file_size;
}

long recv_file_time(short socket_id){
	long file_time;
	ssize_t len = recv(socket_id, &file_time, sizeof(file_time), 0);
	if(len<0)
		error("no time", 1);
	return file_time;
}

short recv_file_amount(short socket_id){
	short file_amount;
	ssize_t len = recv(socket_id, &file_amount, sizeof(file_amount), 0);
	if(len<0)
		error("no amount", 1);
	return file_amount;
}

void recv_file(short socket_id, char * file_name, int file_size){
	char buffer[256];
	ssize_t len;
	int remain_data = file_size;
	FILE *rec_file;
	rec_file = fopen(file_name, "w");
	if(rec_file == NULL)
		error("file", 1);

	printf("%s: %d bytes\n", file_name, file_size);

	int full_count = file_size/sizeof(buffer);
	for(int i=0; i<full_count; i++){
		len = recv(socket_id, buffer, sizeof(buffer), 0);
		if(len<0)
			error("len", 1);
		fwrite(buffer, sizeof(char), len, rec_file);
		remain_data -= len;
		printf("received: %d bytes, %d bytes remaining\n", len, remain_data);
	}
	if(remain_data){
		len = recv(socket_id, buffer, file_size%256, 0);
		if(len<0)
			error("len", 1);
		fwrite(buffer, sizeof(char), len, rec_file);
		remain_data -= len;
		printf("received: %d bytes, %d bytes remaining\n", len, remain_data);
	}

	// this is not working for multiple files
	// recv sucks in too much data, including more sends from server than one
	// while((remain_data>0) && (len = recv(socket_id, buffer, sizeof(buffer), 0))>0){
	// 	fwrite(buffer, sizeof(char), len, rec_file);
	// 	remain_data -= len;
	// 	printf("received: %d bytes, %d bytes remaining\n", len, remain_data);
	// }

	printf("\n");
	fclose(rec_file);
}

int main(int argc, char *argv[]){
	if(argc==1)
		return print_help();
	
	if(argv[1][0]=='-'){
		if(argv[1][1]=='d'){
			if(argc<3)
				invalid_args();

			short client_socket = get_conn();
			handshake(client_socket, FILE_DOWN);

			short file_count = argc-2;
			send_file_amount(client_socket, file_count);

			int file_size;
			char *file_name;
			for(int i=0; i<file_count; i++){
				file_name = argv[i+2];
				send_file_name(client_socket, file_name);
				file_size = recv_file_size(client_socket);
				recv_file(client_socket, file_name, file_size);
			}

			close(client_socket);

		}else if(argv[1][1]=='n'){
			short file_count;
			if(argc==2)
				file_count = 1;
			else if(argc==3){
				file_count = atoi(argv[2]);
				if(file_count<1)
					error("file amount nan", 1);
			}else
				invalid_args();
			
			short client_socket = get_conn();
			handshake(client_socket, FILE_NEWEST);

			send_file_amount(client_socket, file_count);

			for(int i=0; i<file_count; i++){
				char file_name[256];
				int file_size;
				printf("%s: %d\n", recv_file_name(client_socket), recv_file_size(client_socket));
				// file_size = recv_file_size(client_socket);
				// recv_file(client_socket, file_name, file_size);
			}

			close(client_socket);

		}else if(argv[1][1]=='l'){
			if(argc!=2)
				invalid_args();
			
			short client_socket = get_conn();
			handshake(client_socket, FILE_LIST);

			int n = recv_file_amount(client_socket);

			for(int i=0; i<n; i++){
				char file_name[256], date[26];
				int file_size;
				long file_time;
				struct tm* file_time_struct;

				strcpy(file_name, recv_file_name(client_socket));
				file_size = recv_file_size(client_socket);
				file_time = recv_file_time(client_socket);

				file_time_struct = localtime(&file_time);
				strftime(date, 26, "%d-%m-%Y %H:%M:%S", file_time_struct);

				printf("%s -> %d bytes, %s\n", file_name, file_size, date);
			}

			close(client_socket);
			
		}else{
			invalid_args();
		}
	}else{
		short client_socket = get_conn();
		handshake(client_socket, FILE_UP);

		short file_amount = argc-1;
		send_file_amount(client_socket, file_amount);

		for(short i=0; i<file_amount; i++){
			char * file_name = argv[i+1];

			int fd = open(file_name, O_RDONLY);
			if(fd<0)
				error("opening file", 1);
			struct stat file_stat;
			if(fstat(fd, &file_stat)<0)
				error("fstat", 1);

			int file_size = file_stat.st_size;
			
			send_file_name(client_socket, file_name);
			send_file_size(client_socket, file_size);
			send_file(client_socket, fd, file_size);
			close(fd);
		}

		close(client_socket);

	}
	return 0;
}