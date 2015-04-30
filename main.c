#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEST_PORT 80

char IP[16];	// adresa IP de cautare server web; se va incrementa iterativ

void setIP(){
	strcpy(IP,"192.168.3.1");
}

void *rutina_fir1(void *params)
{
	int sockfd;
	struct sockaddr_in dest_addr;
	
	setIP();	

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1){
		perror("socket");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(DEST_PORT);
	dest_addr.sin_addr.s_addr = inet_addr(IP);
	memset(&(dest_addr.sin_zero), 0, 8);
	
	if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) == -1){
		perror("connect");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}
	
	// processing here ...

	
	if(shutdown(sockfd,SHUT_WR) == -1){
		perror("shutdown");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}

	return NULL;
}



int main(int argc, char *argv[])
{
	pthread_t fir1;
	
	if(pthread_create(&fir1,NULL, &rutina_fir1, NULL)){
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}

	// asteptam incheierea
	if(pthread_join(fir1, NULL))
		perror("pthread_join");


	return EXIT_SUCCESS;
}
