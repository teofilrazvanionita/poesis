#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <mysql.h>
#include <errmsg.h>

//#define _POSIX_SOURCE

#define DEST_PORT 80

// macro used on system calls errors; on final version suppress memset() call here and use it only once at beginning; adjust temp[] dimension as needed 
#define ERROR(msg)	memset(temp, 0, 64); sprintf(temp, "[%s]:%d " msg ":\n" "errnum %d: %s\n", __FILE__, __LINE__, errno, strerror(errno)); write(STDERR_FILENO, temp, strlen(temp));
#define PTHREAD_ERROR(msg,err)	memset(temp, 0, 64); sprintf(temp, "[%s]:%d " msg ":\n" "errnum %d: %s\n", __FILE__, __LINE__, err, strerror(err)); write(STDERR_FILENO, temp, strlen(temp));


typedef struct {
	long long ID;
	long long ID_pagini_html;
	char Link[500];
}TABLE_ROW;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

TABLE_ROW current_line_ref_html;	// used sincron by threads 2 and 3

char IP1[16];	// adresa IP de cautare server web pe threadul 1
char IP2[16];	// IP address use by thread 2
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
		exit(EXIT_FAILURE);
	}	
	readcount = read(fd_devurand, &read_buf, 1);
	if(readcount != 1){
		ERROR("read");
		exit(EXIT_FAILURE);
	}
	if(close(fd_devurand) == -1){
		ERROR("close");
		exit(EXIT_FAILURE);
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
	
	sprintf(sa, "%d", (int)a);
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

int getOpenedSocket(int *sfd, char *IP_ADDRESS, int thread_no)
{
	int flags, retcon;
	struct sockaddr_in dest_addr;

	*sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(*sfd == -1){
		ERROR("socket");
		exit(EXIT_FAILURE);
	}

	flags = fcntl(*sfd, F_GETFL);
	if(flags == -1){
		ERROR("fcntl");
		exit(EXIT_FAILURE);
	}
	flags |= O_NONBLOCK;;
	if(fcntl(*sfd, F_SETFL, flags) == -1){
		ERROR("fcntl");
		exit(EXIT_FAILURE);
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
				exit(EXIT_FAILURE);
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
				exit(EXIT_FAILURE);
			}
			if(FD_ISSET(*sfd, &wfds)){
				if(getsockopt(*sfd, SOL_SOCKET, SO_ERROR, &sockoptval, &sockoptsize) == -1){
					ERROR("getsockopt");
					exit(EXIT_FAILURE);
				}
				if(!sockoptval){
					memset(writebuf, 0, 40);
					if(thread_no == 1)
						strcat(writebuf, "THREAD 1: Connected to ");
					else if(thread_no == 2)
						strcat(writebuf, "THREAD 2: Connected to ");
					strcat(writebuf, IP_ADDRESS);
					strcat(writebuf, "\n");
				
					if(write(STDOUT_FILENO, writebuf, 24 + strlen(IP_ADDRESS)) == -1){
						ERROR("write");
						exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}

	return -1;
}

int exchangeMessage(int *sfd, int thread_no, char *IP_ADDRESS, const char *Server, const char *URI_REQ)
{
	fd_set rfds;
	struct timeval tv;
	char buf_read[8192];
	ssize_t read_count;
        
        char http1_1req[800];
        char *p;

	switch(thread_no){
	case 1:
		// writting on the socket
		if(write(*sfd, "GET / HTTP/1.0\nUser-Agent: poesis/1.1\n\n", 39) == -1){
			if(write(STDERR_FILENO, "Error writing on socket\n", 24) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			return -1;	// ERROR
		}
		FD_ZERO(&rfds);
		FD_SET(*sfd, &rfds);
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		if(select((*sfd)+1, &rfds, NULL, NULL, &tv) == -1){
			ERROR("select");
			exit(EXIT_FAILURE);
		}
		if(FD_ISSET(*sfd, &rfds)){
			int ipIndexHtml_fd;
			int k = 0;
			//int savedErrno;

			// open and create file "ip-index-html"
			if((ipIndexHtml_fd = open("ip-index-html", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1){
				ERROR("open");
				exit(EXIT_FAILURE);
			}
						
			// write IP address on first line of the file
			if(write(ipIndexHtml_fd, IP_ADDRESS, strlen(IP_ADDRESS)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			if(write(ipIndexHtml_fd, "\n", 1) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
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
							exit(EXIT_FAILURE);
						}
						sleep(2);
						continue;	
					}
					if(errno == ECONNRESET){
						continue;
					}
					ERROR("read");
					exit(EXIT_FAILURE);
				}
								
				// write into the file
				if(write(ipIndexHtml_fd, buf_read, read_count) != read_count){
					ERROR("write");
					exit(EXIT_FAILURE);
				}
			}
							
			if(write(STDOUT_FILENO, "THREAD 1: Written to ip-index-html\n", 35) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}	
			if(close(ipIndexHtml_fd) == -1){
				ERROR("close");
				exit(EXIT_FAILURE);
			}
			return 0;	// Normal return
		}

		break;
	case 2:
                memset(http1_1req, 0, 800);
                //extract the host name part from server address containg the protocol specification (e.g. http, https))
        	p = strchr(Server, '/');
                if(p)	// 1st '/'
                        p++;
        	else{
                	ERROR("Not an web address format");
                        exit(EXIT_FAILURE);
        	}
        	p = strchr(p, '/');
                if(p)	// 2nd '/'
                        p++;
        	else{
                	ERROR("Not an web address format");
                        exit(EXIT_FAILURE);
        	}               
                // construct the request string
                strcpy(http1_1req, "GET ");
                strcat(http1_1req, URI_REQ);
                strcat(http1_1req, " HTTP/1.1\nUser-Agent: poesis/1.1\nHost: ");
                strcat(http1_1req, p);
                strcat(http1_1req, ":80\n\n");
                
                //send the request
 		if(write(*sfd, http1_1req, sizeof(http1_1req)) == -1){
			if(write(STDERR_FILENO, "Error writing on socket\n", 24) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			return -1;	// ERROR
		}
                
  		FD_ZERO(&rfds);
		FD_SET(*sfd, &rfds);
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		if(select((*sfd)+1, &rfds, NULL, NULL, &tv) == -1){
			ERROR("select");
			exit(EXIT_FAILURE);
		}             
  		if(FD_ISSET(*sfd, &rfds)){
			int refGlobPage_fd;
			int k = 0;
			//int savedErrno;

			// open and create file "ip-index-html"
			if((refGlobPage_fd = open("ref-glob-page", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1){
				ERROR("open");
				exit(EXIT_FAILURE);
			}
						
			// write IP address on first line of the file
			if(write(refGlobPage_fd, IP_ADDRESS, strlen(IP_ADDRESS)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			if(write(refGlobPage_fd, "\n", 1) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}

			// write URLLink on second line
			if(write(refGlobPage_fd, Server, strlen(Server)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			if(write(refGlobPage_fd, URI_REQ, strlen(URI_REQ)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			if(write(refGlobPage_fd, "\n", 1) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
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

						if(write(STDOUT_FILENO, "THREAD 2: Resource temporary anavailable, continuing...\n", 56) == -1){
							ERROR("write");
							exit(EXIT_FAILURE);
						}
						sleep(2);
						continue;	
					}
					if(errno == ECONNRESET){
						continue;
					}
					ERROR("read");
					exit(EXIT_FAILURE);
				}
								
				// write into the file
				if(write(refGlobPage_fd, buf_read, read_count) != read_count){
					ERROR("write");
					exit(EXIT_FAILURE);
				}
				//this should be redundant under normal conditions (e.g. a recent kernel version) as I've already specified the O_DSYNC flag at open()
				/*if(fdatasync(refGlobPage_fd) == -1){
					ERROR("fdatasync");
					exit(EXIT_FAILURE);
				}*/
			}
							
			if(write(STDOUT_FILENO, "THREAD 2: Written to ref-glob-page\n", 35) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}	
			if(close(refGlobPage_fd) == -1){
				ERROR("close");
				exit(EXIT_FAILURE);
			}
			return 0;	// Normal return
		}
              
                
		break;
	default:
		break;
	}

	return -1;	// Error return
}


void splitLink(const char *Link, char *WS, char *URI)
{
	char *p, *q;
 	int i;


	p = strchr(Link, '/');
	if(p)	// 1st '/' in html address
		p++;
	else{
		ERROR("Not an web address format");
		exit(EXIT_FAILURE);
	}
	
	p = strchr(p, '/');
	if(p)	// 2nd '/' in html address
		p++;
	else{
		ERROR("Not an web address format");
		exit(EXIT_FAILURE);
	}

	p = strchr(p, '/');
	if(p){
		for(q = (char *)Link, i = 0; q < p; q++, i++)
			WS[i] = Link[i];
		strcpy(URI, p);
	}
	else{
		strcpy(WS, Link);
		*URI = '\0';
	}
}

int verifyExistance(char *hiperlink)
{
        char interogare[560];
    	
        MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;
        
        char *server = "localhost";
	char *user = "razvan";
	char *password = "password";
	char *database = "poesis";
        
        memset(interogare, 0, 560);
        
        conn = mysql_init(NULL);

	/* Connect to database */
	if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)){
		if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1){
			ERROR("write");
		}
                if(write(STDERR_FILENO, "\n", 1) == -1){
			ERROR("write");
		}
		exit(EXIT_FAILURE);
	}
        
        sprintf(interogare, "select * from referinte_globale where URL = \"%s\"", hiperlink);
        
	if(mysql_query(conn, interogare)){
		if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1){
			ERROR("write");
		}
                if(write(STDERR_FILENO, "\n", 1) == -1){
			ERROR("write");
		}		
		exit(EXIT_FAILURE);
	}

	res = mysql_use_result(conn);
	if(res == NULL){
		if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1){
			ERROR("write");
		}
                if(write(STDERR_FILENO, "\n", 1) == -1){
			ERROR("write");
		}
		exit(EXIT_FAILURE);
	}
        
	while((row = mysql_fetch_row(res))){
            	mysql_free_result(res);
		mysql_close(conn);
                
                return 1;
        }
        
        mysql_free_result(res);
	mysql_close(conn);
        
        return 0;
}

void getIpAddressFromHostName(char *IP, const char *WS)
{
	const char *service = "80";
	struct addrinfo hints; 
	struct addrinfo *result, *rp;;
	int s;
	int k = 0;
	char *p;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	

	p = strchr(WS, '/');
	if(p)	// 1st '/'
		p++;
	else{
		ERROR("Not an web address format");
		exit(EXIT_FAILURE);
	}
	
	p = strchr(p, '/');
	if(p)	// 2nd '/'
		p++;
	else{
		ERROR("Not an web address format");
		exit(EXIT_FAILURE);
	}

AGAIN:	if((s = getaddrinfo(p, service, &hints, &result)) != 0 && s != EAI_SYSTEM && s != EAI_NONAME){
		if(s == EAI_AGAIN){
			k++;
			if(k >= 5){
				*IP = '\0';
				return;
			}
			goto AGAIN;
		}
		if(write(STDERR_FILENO, "ERROR from getaddrinfo(): ", 26) == -1){
			ERROR("write");
			exit(EXIT_FAILURE);
		}
		if(write(STDERR_FILENO, gai_strerror(s), strlen(gai_strerror(s))) == -1){
			ERROR("write");
			exit(EXIT_FAILURE);
		}
		
		exit(EXIT_FAILURE);
	}

	for(rp = result; rp != NULL; rp = rp->ai_next){
		strcpy(IP, inet_ntoa(((struct sockaddr_in *)rp->ai_addr)->sin_addr));	// laso this part might necesitate some mutex along time
		if(IP)
			break;
	}

	if(rp == NULL)
		*IP = '\0';

	freeaddrinfo(result);
}

void *rutina_fir1(void *params)
{
	int sockfd;

	while(1){
		setIP();	

		if(getOpenedSocket(&sockfd, IP1, 1) == -1)
			continue;
		else{
			if(!exchangeMessage(&sockfd, 1, IP1, NULL, NULL)){
				if(system("./parseIPIndexHtml.pl") == -1){
					ERROR("system");
					exit(EXIT_FAILURE);
				}
			
				if((shutdown(sockfd, SHUT_RDWR) == -1) && (errno != ECONNRESET) && (errno != ENOTCONN)){
						ERROR("shutdown");
						exit(EXIT_FAILURE);
				}
			}
			if((close(sockfd) == -1)){
				ERROR("close");
				exit(EXIT_FAILURE);
			}
		}

		
	}

	return NULL;
}

void *rutina_fir2(void *params)
{
	int sockfd;
	int s;	// return value from pthread function calls
		
	long long last_id = 0L;
		
	TABLE_ROW line;
	
	char Server[255];	// web server address
	char URI[500];		// URI request on web server (using HTTP/1.1)

	char interogare[60];

	char errorMySQLAPI[255];

	MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;

	my_bool reconnect = 1;	// enable auto-reconnect

	char *server = "localhost";
	char *user = "razvan";
	char *password = "password";
	char *database = "poesis";

	memset(errorMySQLAPI, 0 ,255);
	memset(&line, 0, sizeof(line));
	memset(Server, 0 , 255);
	memset(URI, 0, 500);


	conn = mysql_init(NULL);

	/* enable auto reconnection */
	if(mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect)){
		if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1){
			ERROR("write");
		}
		if(write(STDERR_FILENO, "\n", 1) == -1){
			ERROR("write");
		}
		exit(EXIT_FAILURE);
	}

	/* Connect to database */
	if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)){
		//fprintf(stderr, "%s\n", mysql_error(conn));
		
		strcpy(errorMySQLAPI, mysql_error(conn));
		strcat(errorMySQLAPI, "\n");
		
		if(write(STDERR_FILENO, errorMySQLAPI, strlen(errorMySQLAPI)) == -1){
			ERROR("write");
		}

		exit(EXIT_FAILURE);
	}

	while(1){
		sprintf(interogare, "select * from referinte_html where id > %lld", last_id);
		
		/* send SQL query */
		if(mysql_query(conn, interogare)){
			//fprintf(stderr, "%s\n", mysql_error(conn));

			strcpy(errorMySQLAPI, mysql_error(conn));
			strcat(errorMySQLAPI,"\n");

			if(write(STDERR_FILENO, errorMySQLAPI, strlen(errorMySQLAPI)) == -1){
				ERROR("write");
			}
			/*
			if((mysql_errno(conn)) == CR_SERVER_GONE_ERROR){
				//trying to re-establish the connection
				if(!mysql_real_connect(conn, server, user, password, database, 0 , NULL, 0)){
					if(write(STDERR_FILENO, mysql_error(conn), strlen(mysql_error(conn))) == -1){
						ERROR("write");
					}
					if(write(STDERR_FILENO, "\nCouldn't re-establish connection\n", 34) == -1){
						ERROR("write");
						exit(EXIT_FAILURE);	
					}
				}
				continue;
			} */
			exit(EXIT_FAILURE);
		}

		res = mysql_use_result(conn);
		if(res == NULL){
			//fprintf(stderr, "%s\n", mysql_error(conn));
			
			strcpy(errorMySQLAPI, mysql_error(conn));
			strcat(errorMySQLAPI,"\n");
			
			if(write(STDERR_FILENO, errorMySQLAPI, strlen(errorMySQLAPI)) == -1){
				ERROR("write");
			}

			exit(EXIT_FAILURE);
		}
		
		while((row = mysql_fetch_row(res))){
			line.ID = atoll(row[0]);
			line.ID_pagini_html = atoll(row[1]);
			strncpy(line.Link, row[2], 499);
			
			last_id = line.ID;
			
			write(STDOUT_FILENO, "THREAD 2: Link No: ", 19);	
			if(write(STDOUT_FILENO, row[0], strlen(row[0])) == -1){
				ERROR("write");
			}
			write(STDOUT_FILENO, ": ", 2);
			if(write(STDOUT_FILENO, line.Link, strlen(line.Link)) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}

			if(write(STDOUT_FILENO, "\n", 1) == -1){
				ERROR("write");
				exit(EXIT_FAILURE);
			}
			
			memset(Server, 0 , 255);
			memset(URI, 0, 500);
		
			splitLink(line.Link, Server, URI);
			write(STDOUT_FILENO, "THREAD 2: SERVER ADDRESS: ", 26);
			write(STDOUT_FILENO, Server, strlen(Server));
			write(STDOUT_FILENO, "\n", 1);
			write(STDOUT_FILENO, "THREAD 2: URI REQUEST: ", 23);
			write(STDOUT_FILENO, URI, strlen(URI));
			write(STDOUT_FILENO, "\n", 1);

                        if(!verifyExistance(line.Link)){ // check if Link already exist in referinte_globale
                                // Link doesn't exist - go further processing it here
				memset(IP2, 0, 16);
				getIpAddressFromHostName(IP2, Server);
                                
				if(write(STDOUT_FILENO, "Thread 2: IP from Host Name: ", 29) == -1){
					ERROR("write");
					exit(EXIT_FAILURE);
				}
				if(write(STDOUT_FILENO, IP2, strlen(IP2)) == -1){
					ERROR("write");
					exit(EXIT_FAILURE);
				}
				if(write(STDOUT_FILENO, "\n", 1) == -1){
					ERROR("write");
					exit(EXIT_FAILURE);
				}

				if(*IP2){
					if(!getOpenedSocket(&sockfd, IP2, 2)){
						if(!exchangeMessage(&sockfd, 2, IP2, Server, URI)){
                        				if(system("./parseRefGlobv2.pl") == -1){
                                        			ERROR("system");
                                                                exit(EXIT_FAILURE);
                                                        }
                                                        if((shutdown(sockfd, SHUT_RDWR) == -1) && (errno != ECONNRESET) && (errno != ENOTCONN)){
                                                                ERROR("shutdown");
                                                                 exit(EXIT_FAILURE);
                                                        }
						}
						if(close(sockfd) == -1){
							ERROR("close");
							exit(EXIT_FAILURE);
						}
					}
				}
                        }
                        
			s = pthread_mutex_lock(&mtx);
			if(s != 0){
				PTHREAD_ERROR("pthread_mutex_lock", s);
				exit(EXIT_FAILURE);
			}
			
			current_line_ref_html = line;

			s = pthread_mutex_unlock(&mtx);
			if(s != 0){
				PTHREAD_ERROR("pthread_mutex_unlock", s);
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
