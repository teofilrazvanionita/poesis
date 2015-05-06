#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define DEST_PORT 80

char IP[16];	// adresa IP de cautare server web; se va incrementa iterativ

unsigned char fillRand(){
	int fd_devurand, readcount;
	char read_buf;

	fd_devurand = open("/dev/urandom", O_RDONLY);
	if(fd_devurand == -1){
		perror("open /dev/urandom");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}	
	readcount = read(fd_devurand, &read_buf, 1);
	if(readcount != 1){
		perror("read /dev/urandom");;
		exit(EXIT_FAILURE);	// v. pthread_exit
	}
	if(close(fd_devurand) == -1){
		perror("close /dev/urandom");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}

	return (unsigned char) read_buf;
}

void setIP(){
	unsigned char a,b,c,d;
	char sa[4], sb[4], sc[4], sd[4];

	memset(IP, 0, 16);

	bzero((void *)sa, 4);
	bzero((void *)sb, 4);
	bzero((void *)sc, 4);
	bzero((void *)sd, 4);

	// fill it randomly, excluding Privare IP ranges
	a = fillRand();
	b = fillRand();
	c = fillRand();
	d = fillRand();
	while (a == 10 || (a == 172 && (b >= 16 && b <= 31)) || (a == 192 && b == 168) || a == 127){
		bzero((void *)sa, 4);
		bzero((void *)sb, 4);
		bzero((void *)sc, 4);
		bzero((void *)sd, 4);
	
		a = fillRand();
		b = fillRand();
		c = fillRand();
		d = fillRand();
	}
	
	sprintf(sa, "%d", (int)a);	// possible reentrancy issues
	sprintf(sb, "%d", (int)b);
	sprintf(sc, "%d", (int)c);
	sprintf(sd, "%d", (int)d);

	strcat(IP, sa);
	strcat(IP, ".");
	strcat(IP, sb);
	strcat(IP, ".");
	strcat(IP, sc);
	strcat(IP, ".");
	strcat(IP, sd);


	//strcpy(IP,"178.157.84.142");
	//strcpy(IP,"192.168.1.1");	// used for testing
}

void *rutina_fir1(void *params)
{
	int sockfd, retcon, flags;
	struct sockaddr_in dest_addr;
	


	while(1){
		setIP();	

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd == -1){
			perror("socket");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}
	
		flags = fcntl(sockfd, F_GETFL);
		if(flags == -1){
			perror("fcntl");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}
		flags |= O_NONBLOCK;;
		if(fcntl(sockfd, F_SETFL, flags) == -1){
			perror("fcntl F_SETFL");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}

		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(DEST_PORT);
		dest_addr.sin_addr.s_addr = inet_addr(IP);
		memset(&(dest_addr.sin_zero), 0, 8);
	
		retcon = connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
		if (retcon == -1){
			if(errno == ECONNREFUSED){
				write(1, "Connection refused\n", 19);
				continue;
			}

		}
	
	// processing here ...

		if(retcon == -1 && errno == EINPROGRESS){
			char writebuf[29];
			int sockoptval = 0;
			fd_set wfds, rfds;
			int retval;
			struct timeval tv;
			char buf_read[1024];

			FD_ZERO(&wfds);
			FD_SET(sockfd, &wfds);

			tv.tv_sec = 1;
			tv.tv_usec = 0;
			
			while(1){
				retval = select(sockfd + 1, NULL, &wfds,  NULL, &tv);
				if(FD_ISSET(sockfd, &wfds)){
					getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockoptval, sizeof(int));
					if(!sockoptval){
						memset(writebuf, 0, 29);
						strcat(writebuf, "Connected to ");	// possible reentrancy issues!
						strcat(writebuf, IP);
						strcat(writebuf, "\n");
						write(1, writebuf, 14 + strlen(IP));
						//sleep(2);
						retval = 0;
						
						// writting on the socket
						retval = write(sockfd, "GET / HTTP/1.0\n\n", 16);
						if(retval == -1){
							printf("Error write\n");	// TODO: change to write
							break;
						}/*else {
							printf("Writen to socket\n");
						}*/
						
						FD_ZERO(&rfds);
						FD_SET(sockfd, &rfds);
						tv.tv_sec = 5;
						tv.tv_usec = 0;
						retval = select(sockfd+1, &rfds, NULL, NULL, &tv);
						if(FD_ISSET(sockfd, &rfds)){
							memset(buf_read, 0, 1024);
							retval = read(sockfd, buf_read, 1024);
							write(1, buf_read, retval);
						}
					}
					break;
				}else if(retval == -1){
					perror("select");
				}
				break;
			}
		}

		if(!retcon && (shutdown(sockfd,SHUT_RDWR) == -1)){
			
			perror("shutdown");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}
		
		if(close(sockfd) == -1){
			perror("close");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}

		//sleep((unsigned int)1);
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
