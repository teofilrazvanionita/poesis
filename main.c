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
#include <mysql.h>

#define DEST_PORT 80

// macro used on system calls errors; on final version suppress memset() call here and use it only once at beginning; adjust temp[] dimension as needed 
#define ERROR(msg)	memset(temp, 0, 64); sprintf(temp, "[%s]:%d " msg "\n" "%s\n", __FILE__, __LINE__, strerror(errno)); write(STDERR_FILENO, temp, strlen(temp));


char IP1[16];	// adresa IP de cautare server web pe threadul 1
char temp[64];	// used for printing eror messages; adjust dimension as needed
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

	memset(IP1, 0, 16);

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

	strcat(IP1, sa);
	strcat(IP1, ".");
	strcat(IP1, sb);
	strcat(IP1, ".");
	strcat(IP1, sc);
	strcat(IP1, ".");
	strcat(IP1, sd);


	//strcpy(IP1, "153.120.37.225");
	//strcpy(IP1, "192.168.1.1");	// used for testing
}

int getOpenedSocket(int *sfd, char *IP_ADDRESS)
{
	int flags, retcon;
	struct sockaddr_in dest_addr;

	*sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(*sfd == -1){
		ERROR("socket");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}

	flags = fcntl(*sfd, F_GETFL);
	if(flags == -1){
		ERROR("fcntl");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}
	flags |= O_NONBLOCK;;
	if(fcntl(*sfd, F_SETFL, flags) == -1){
		ERROR("fcntl");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(DEST_PORT);
	dest_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
	memset(&(dest_addr.sin_zero), 0, 8);

	retcon = connect(*sfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
	if (retcon == -1){
		if(errno == ECONNREFUSED){
			write(STDOUT_FILENO, "Connection refused\n", 19);
			if(close(*sfd) == -1){
				ERROR("close");
				exit(EXIT_FAILURE);	// v. pthread_exit
			}
			return -1;
		}
	}
	if(retcon == -1 && errno == EINPROGRESS){
		char writebuf[40];
		int sockoptval = 0;
		socklen_t sockoptsize;
		fd_set wfds;
		struct timeval tv;

		FD_ZERO(&wfds);
		FD_SET(*sfd, &wfds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		sockoptsize = sizeof(sockoptval);

		while(1){
			if(select(*sfd + 1, NULL, &wfds,  NULL, &tv) == -1){
				ERROR("select");
				exit(EXIT_FAILURE);	// v. pthread_exit
			}
			if(FD_ISSET(*sfd, &wfds)){
				if(getsockopt(*sfd, SOL_SOCKET, SO_ERROR, &sockoptval, &sockoptsize) == -1){
					ERROR("getsockopt");
					exit(EXIT_FAILURE);	// v. pthread_exit
				}
				if(!sockoptval){
					memset(writebuf, 0, 40);
					strcat(writebuf, "THREAD 1: Connected to ");	// possible reentrancy issues!
					strcat(writebuf, IP_ADDRESS);
					strcat(writebuf, "\n");
				
					if(write(STDOUT_FILENO, writebuf, 24 + strlen(IP_ADDRESS)) == -1){
						ERROR("write");
						exit(EXIT_FAILURE);	// v. pthread_exit or _exit()
					}
					return 0;	// normal return
				}
				break;
			}
			break;
		}
	}
	if(close(*sfd) == -1){
		ERROR("close");
		exit(EXIT_FAILURE);	// v. pthread_exit
	}

	return -1;
}

int exchangeMessage(int *sfd, int thread_no, char *IP_ADDRESS)
{
	fd_set rfds;
	struct timeval tv;
	char buf_read[8192];
	ssize_t read_count;

	switch(thread_no){
	case 1:
		// writting on the socket
		if(write(*sfd, "GET / HTTP/1.0\nUser-Agent: poesis/1.1\n\n", 39) == -1){
			if(write(STDERR_FILENO, "Error writing on socket\n", 24) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);		// v. pthread_exit
			}
			return -1;	// ERROR
		}
		FD_ZERO(&rfds);
		FD_SET(*sfd, &rfds);
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		if(select((*sfd)+1, &rfds, NULL, NULL, &tv) == -1){
			ERROR("select");
			exit(EXIT_FAILURE);	// v. pthread_exit
		}
		if(FD_ISSET(*sfd, &rfds)){
			int ipIndexHtml_fd;
			int k = 0;
			//int savedErrno;

			// open and create file "ip-index-html"
			if((ipIndexHtml_fd = open("ip-index-html", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1){
				ERROR("open");
				exit(EXIT_FAILURE);	// v. pthread_exit
			}
						
			// write IP address on first line of the file
			if(write(ipIndexHtml_fd, IP_ADDRESS, strlen(IP_ADDRESS)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);	// v. pthread_exit
			}
			if(write(ipIndexHtml_fd, "\n", 1) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);	// v. pthred_exit or _exit
			}
							
			// clear de buffer for used reading from the socket
			memset(buf_read, 0,8192);
							
			while((read_count = read(*sfd, buf_read, 8192)) != 0){
				if(read_count == -1){
					if(errno == EAGAIN){
						k++;	// loop EAGAIN counter
									
						if(k >= 3){
							break;
						}

						if(write(STDOUT_FILENO, "THREAD 1: Resource temporary anavailable, continuing...\n", 56) == -1){
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
							
			if(write(STDOUT_FILENO, "THREAD 1: Written to ip-index-html\n", 35) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);		// v. pthread_exit, _exit...
			}	
			if(close(ipIndexHtml_fd) == -1){
				ERROR("close");
				exit(EXIT_FAILURE);		// v. pthread_exit or _exit
			}
			return 0;	// Normal return
		}

		break;
	case 2:

		break;
	default:
		break;
	}


	return -1;	// Error return
}

void *rutina_fir1(void *params)
{
	int sockfd;

	while(1){
		setIP();	

		if(getOpenedSocket(&sockfd, IP1) == -1)
			continue;
		else{
			if(!exchangeMessage(&sockfd, 1, IP1)){
				if(system("./parseIPIndexHtml.pl") == -1){
					ERROR("system");
					exit(EXIT_FAILURE);		// v. pthread_exit
				}
			
				if((shutdown(sockfd, SHUT_RDWR) == -1) && (errno != ECONNRESET) && (errno != ENOTCONN)){
						ERROR("shutdown");
						exit(EXIT_FAILURE);		// v. phread_exit
				}
			}
			if((close(sockfd) == -1)){
				ERROR("close");
				exit(EXIT_FAILURE);		// v. phread_exit
			}
		}

		
	}

	return NULL;
}

void *rutina_fir2(void *params)
{
	int sockfd;
       	
	long long last_id = 1L;
	char hiperlink[500];

	char interogare[60];

	MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;

	char *server = "localhost";
	char *user = "razvan";
	char *password = "password";
	char *database = "poesis";

	memset(hiperlink, 0 , 500);

	conn = mysql_init(NULL);
	/* Connect to database */
	if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)){
		//fprintf(stderr, "%s\n", mysql_error(conn));
		if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1)
			ERROR("write");
		if(write(STDERR_FILENO, "\n", 1) == -1)
			ERROR("write");
		exit(EXIT_FAILURE);
	}

	while(1){
		sprintf(interogare, "select * from referinte_html where id > %lld", last_id);
		
		/* send SQL query */
		if(mysql_query(conn, interogare)){
			//fprintf(stderr, "%s\n", mysql_error(conn));
			if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1)
				ERROR("write");
			if(write(STDERR_FILENO, "\n", 1) == -1)
				ERROR("write");
			exit(EXIT_FAILURE);
		}

		res = mysql_use_result(conn);
		if(res == NULL){
			//fprintf(stderr, "%s\n", mysql_error(conn));
			if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1)
				ERROR("write");
			if(write(STDERR_FILENO, "\n", 1) == -1)
				ERROR("write");
			exit(EXIT_FAILURE);
		}
		
		while((row = mysql_fetch_row(res))){
			last_id = atoll(row[0]);
			strncpy(hiperlink, row[2], 499);
			
			if(write(STDOUT_FILENO, hiperlink, strlen(hiperlink)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}

			if(write(STDOUT_FILENO, "\n", 1) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}

			sleep(1);
		}

		sleep(3600);	// ii dam timp sa adauge in baza de date - 1h
		
		mysql_free_result(res);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t fir1, fir2;
	
	if(pthread_create(&fir1,NULL, &rutina_fir1, NULL)){
		ERROR("pthread_create");
		exit(EXIT_FAILURE);
	}
	
	if(pthread_create(&fir2, NULL, &rutina_fir2, NULL)){
		ERROR("pthread_create");
		exit(EXIT_FAILURE);
	}

	// asteptam incheierea
	if(pthread_join(fir1, NULL)){
		ERROR("pthread_join");
		exit(EXIT_FAILURE);
	}

	// asteptam incheierea
	if(pthread_join(fir2, NULL)){
		ERROR("pthread_join");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
