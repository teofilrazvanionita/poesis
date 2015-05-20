#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>
//#include <signal.h>
#include <errno.h>

#define DEST_PORT 80

// macro used on system calls errors; on final version suppress memset() call here and use it only once at beginning; adjust temp[] dimension as needed 
#define ERROR(msg)	memset(temp, 0, 32); sprintf(temp, "[%s]:%d " msg "\n", __FILE__, __LINE__); write(STDOUT_FILENO, temp, strlen(temp)); perror(msg);


char IP[16];	// adresa IP de cautare server web; se va incrementa iterativ
char temp[32];	// used for printing eror messages; adjust dimension as needed
/*
static volatile sig_atomic_t gotAlarm = 0;	// Set nonzero on receipt of SIGALRM

static void sigAlrmHandler(int sig){	// SIGALRM Handler
	write(STDOUT_FILENO, "Got SIGALRM\n", 12);
	gotAlarm = 1;
}
*/
unsigned char fillRand(){
	int fd_devurand, readcount;
	char read_buf;

	fd_devurand = open("/dev/urandom", O_RDONLY);
	if(fd_devurand == -1){
		ERROR("open");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}	
	readcount = read(fd_devurand, &read_buf, 1);
	if(readcount != 1){
		ERROR("read");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}
	if(close(fd_devurand) == -1){
		ERROR("close");
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


	//strcpy(IP,"188.164.254.142");
	//strcpy(IP,"192.168.1.1");	// used for testing
}

void *rutina_fir1(void *params)
{
	int sockfd, retcon, flags;
	struct sockaddr_in dest_addr;
/*
	struct sigaction sa;	// establish a handler fot SIGALRM
										
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigAlrmHandler;
									
	if(sigaction(SIGALRM, &sa, NULL) == -1){
		ERROR("sigaction");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}
*/
	while(1){
		setIP();	

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd == -1){
			ERROR("socket");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}
	
		flags = fcntl(sockfd, F_GETFL);
		if(flags == -1){
			ERROR("fcntl");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}
		flags |= O_NONBLOCK;;
		if(fcntl(sockfd, F_SETFL, flags) == -1){
			ERROR("fcntl");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}

		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(DEST_PORT);
		dest_addr.sin_addr.s_addr = inet_addr(IP);
		memset(&(dest_addr.sin_zero), 0, 8);
	
		retcon = connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
		if (retcon == -1){
			if(errno == ECONNREFUSED){
				write(STDOUT_FILENO, "Connection refused\n", 19);
				if(close(sockfd) == -1){
					ERROR("close");
					exit(EXIT_FAILURE);	// v. pthread_exit
				}
				continue;
			}
		}
	
		if(retcon == -1 && errno == EINPROGRESS){
			char writebuf[29];
			int sockoptval = 0;
			socklen_t sockoptsize;
			fd_set wfds, rfds;
			ssize_t read_count;
			struct timeval tv;
			char buf_read[8192];

			FD_ZERO(&wfds);
			FD_SET(sockfd, &wfds);

			tv.tv_sec = 1;
			tv.tv_usec = 0;

			sockoptsize = sizeof(sockoptval);
			
			while(1){
				if(select(sockfd + 1, NULL, &wfds,  NULL, &tv) == -1){
					ERROR("select");
					exit(EXIT_FAILURE);	// v. pthread_exit
				}
				if(FD_ISSET(sockfd, &wfds)){
					if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockoptval, &sockoptsize) == -1){
						ERROR("getsockopt");
						exit(EXIT_FAILURE);	// v. pthread_exit
					}
					if(!sockoptval){
						memset(writebuf, 0, 29);
						strcat(writebuf, "Connected to ");	// possible reentrancy issues!
						strcat(writebuf, IP);
						strcat(writebuf, "\n");
						if(write(STDOUT_FILENO, writebuf, 14 + strlen(IP)) == -1){
							ERROR("write");
							exit(EXIT_FAILURE);	// v. pthread_exit or _exit()
						}
						
						// writting on the socket
						if(write(sockfd, "GET / HTTP/1.0\nUser-Agent: poesis/1.0\n\n", 39) == -1){
							if(write(STDOUT_FILENO, "Error writing on socket\n", 24) == -1){
								ERROR("write");
								exit(EXIT_FAILURE);		// v. pthread_exit
							}
							break;
						}
						
						FD_ZERO(&rfds);
						FD_SET(sockfd, &rfds);
						tv.tv_sec = 5;
						tv.tv_usec = 0;
						if(select(sockfd+1, &rfds, NULL, NULL, &tv) == -1){
							ERROR("select");
							exit(EXIT_FAILURE);	// v. pthread_exit
						}
						if(FD_ISSET(sockfd, &rfds)){
							int ipIndexHtml_fd;
							int k = 0;
							//int savedErrno;

							// open and create file "ip-index-html"
							if((ipIndexHtml_fd = open("ip-index-html", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1){
								ERROR("open");
								exit(EXIT_FAILURE);	// v. pthread_exit
							}
							
							// write IP address on first line of the file
							if(write(ipIndexHtml_fd, IP, strlen(IP)) == -1){
								ERROR("write");
								exit(EXIT_FAILURE);	// v. pthread_exit
							}
							if(write(ipIndexHtml_fd, "\n", 1) == -1){
								ERROR("write");
								exit(EXIT_FAILURE);	// v. pthred_exit or _exit
							}
							
							// clear de buffer for used reading from the socket
							memset(buf_read, 0,8192);
							
							while((read_count = read(sockfd, buf_read, 8192)) != 0){
								if(read_count == -1){
									if(errno == EAGAIN){
										k++;	// loop EAGAIN counter
										
										/*
										alarm(5);	// trimite SIGALRM dupa 20 sec pentru a asigura iesirea
												//   din ciclu chiar si cand acesta devine infinit 
											
										if(gotAlarm){
											gotAlarm = 0;
											break;
										}
										*/

										if(k >= 3){
											break;
										}
										if(write(STDOUT_FILENO, "Resource temporary anavailable, continuing...\n", 46) == -1){
											ERROR("write");
											exit(EXIT_FAILURE);	// v. pthread_exit
										}
										sleep(2);
										continue;	
									}
									if(errno == ECONNRESET){
										continue;
									}
									ERROR("read");
									exit(EXIT_FAILURE);	// v. pthread_exit
								}
								
								// write into the file
								if(write(ipIndexHtml_fd, buf_read, read_count) != read_count){
									ERROR("write");
									exit(EXIT_FAILURE);	// v. pthread_exit
								}
							}
							

							if(write(STDOUT_FILENO, "Written to ip-index-html\n", 25) == -1){
								ERROR("write");
								exit(EXIT_FAILURE);		// v. pthread_exit, _exit...
							}	
							if(close(ipIndexHtml_fd) == -1){
								ERROR("close");
								exit(EXIT_FAILURE);		// v. pthread_exit or _exit
							}

							if(system("./parseIPIndexHtml.pl") == -1){
								ERROR("system");
								exit(EXIT_FAILURE);		// v. pthread_exit
							}
							
							if((shutdown(sockfd, SHUT_RDWR) == -1) && (errno != ECONNRESET) && (errno != ENOTCONN)){
								ERROR("shutdown");
								exit(EXIT_FAILURE);		// v. phread_exit
							}
							//sleep(20);
						}
						break;
					}
					break;
				}
				break;
			}
		}

		if(close(sockfd) == -1){
			ERROR("close");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}

	}
	return NULL;
}



int main(int argc, char *argv[])
{
	pthread_t fir1;
	
	if(pthread_create(&fir1,NULL, &rutina_fir1, NULL)){
		ERROR("pthread_create");
		exit(EXIT_FAILURE);
	}

	// asteptam incheierea
	if(pthread_join(fir1, NULL)){
		ERROR("pthread_join");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
