#include        <sys/types.h>   /* basic system data types */
#include        <sys/socket.h>  /* basic socket definitions */
#include        <sys/time.h>    /* timeval{} for select() */
#include        <time.h>                /* timespec{} for pselect() */
#include        <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include        <arpa/inet.h>   /* inet(3) functions */
#include        <errno.h>
#include        <fcntl.h>               /* for nonblocking */
#include        <netdb.h>
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include	<syslog.h>
#include	<unistd.h>
#include <netinet/sctp.h>

 //#include <sys/types.h>
 //#include <sys/socket.h>
 //#include <arpa/inet.h>
 //#include <netinet/in.h>
 //#include <stdio.h>
 //#include <stdlib.h>


#define MAXLINE 1024

#define SA struct sockaddr //6

#define LISTENQ 2
#define SERV_PORT 25564 //6
#define BUFFSIZE 2048 //6
//--------------------


#define	MAXFD	64

struct init_frame
{
  uint16_t server_id;
  uint16_t client_id;
  uint8_t waiting;
};

struct output_frame
{
  uint16_t server_id;
  uint16_t client_id;
  struct timespec time;
  uint16_t paddle_x;
};

struct input_frame
{
  uint16_t server_id;
  uint16_t client_id;
  struct timespec time;
  uint16_t paddle_x;
  uint16_t ball_x;
  uint16_t ball_y;
};

struct sockaddr_in    localSock;
struct in_addr localInterface;
struct sockaddr_in groupSock;
 
int sd;
char databuf[24] = "";
int datalen = sizeof(databuf);

int daemon_init(const char *pname, int facility, uid_t uid, int socket)
{
	int		i, p;
	pid_t	pid;

	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		exit(0);			/* parent terminates */

	/* child 1 continues... */

	if (setsid() < 0)			/* become session leader */
		return (-1);

	signal(SIGHUP, SIG_IGN);
	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		exit(0);			/* child 1 terminates */

	/* child 2 continues... */

	chdir("/tmp");				/* change working directory  or chroot()*/
//	chroot("/tmp");

	/* close off file descriptors */
	for (i = 0; i < MAXFD; i++){
		if(socket != i )
			close(i);
	}

	/* redirect stdin, stdout, and stderr to /dev/null */
	p= open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	openlog(pname, LOG_PID, facility);
	
        syslog(LOG_ERR," STDIN =   %i\n", p);
	setuid(uid); /* change user */
	
	return (0);				/* success */
}
//----------------------

    /* Send Multicast Datagram code example. */
    int main (int argc, char *argv[ ])
    {
	srand(time(NULL));
    /* Create a datagram socket on which to send. */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0)
    {
      perror("Opening datagram socket error");
      exit(1);
    }
    else
      printf("Opening the datagram socket...OK.\n");
	
    memset((char *) &groupSock, 0, sizeof(groupSock)); //to send multicast
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr("225.1.1.1");
    groupSock.sin_port = htons(SERV_PORT);

	memset((char *) &localSock, 0, sizeof(localSock)); //to recv
  	localSock.sin_family = AF_INET;
  	localSock.sin_port = htons(SERV_PORT);;
  	localSock.sin_addr.s_addr  = INADDR_ANY;

  	if (bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) { //to recv
   	 perror("binding datagram socket");
   	 close(sd);
   	 exit(1);
  	}

	struct ip_mreq group;
	group.imr_multiaddr.s_addr = inet_addr("225.1.1.1");
  	group.imr_interface.s_addr = inet_addr("127.0.0.1");
  	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) { //to recv multicast
    	perror("adding multicast group");
    	close(sd);
    	exit(1);
  	} else
      printf("Subbing to multicast group...OK\n");
       
    localInterface.s_addr = inet_addr("127.0.0.1");
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
    {
      	perror("Setting local interface error");
		exit(1);
    }
    else
		printf("Setting the local interface...OK\n");

	while(1){ //MAIN LOOP
		short looking_for_clients = 1;
		uint16_t cli_id[2] = {0, 0};
		struct sockaddr cli_addr[2];
		uint16_t srv_id = (uint16_t)rand();
		char cli_name[2][18]; 
		while (looking_for_clients){ //<------
			int i = 0;

			struct sockaddr src_addr;
			socklen_t src_addrlen = sizeof(src_addr);
			bzero(&src_addr, sizeof(src_addr));

			if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&src_addr, &src_addrlen) < 0)
			{
				perror("Reading datagram message error\n");
				close(sd);
				exit(1);
			}
			else
			{
				printf("Reading datagram message from client...OK\n");
				struct init_frame * init_frame;
				init_frame = (struct init_frame *)databuf;
				
				if (cli_id[i] != 0) i++;
				uint16_t id;
				do {
					id = (uint16_t)rand();
				} while (id == cli_id[i == 0 ? 1 : 0]);
				cli_id[i] = id;
				memcpy(cli_name[i], databuf + sizeof(struct init_frame), 16); 
				printf("Client name is: %s\n", cli_name[i]);
				struct init_frame return_frame;
				bzero(&return_frame, sizeof(return_frame));
				return_frame.client_id = cli_id[i];
				return_frame.server_id = srv_id;
				return_frame.waiting = 1;
				memcpy(&cli_addr[i], &src_addr, sizeof(src_addr));
				bzero(&databuf, sizeof(databuf));
				memcpy(databuf, &return_frame, sizeof(return_frame));
				memcpy(databuf + sizeof(return_frame), cli_name[i], strlen(cli_name[i]));
				if(sendto(sd, databuf, datalen, 0, (struct sockaddr *)&src_addr, src_addrlen) < 0){
					perror("Sending datagram message error");
				}
				else
					printf("Sending datagram message...OK\n");
					if (i == 1) looking_for_clients = 0;
			}
			
		}
		//introduction
		for (int i = 0; i < 2; i++) {
			struct init_frame intr_frame;
			bzero(&intr_frame, sizeof(intr_frame));
			intr_frame.client_id = cli_id[i];
			intr_frame.server_id = srv_id;
			intr_frame.waiting = 0;
			bzero(&databuf, sizeof(databuf));
			memcpy(databuf, &intr_frame, sizeof(intr_frame));
			memcpy(databuf + sizeof(intr_frame), cli_name[i == 0 ? 1 : 0], strlen(cli_name[i == 0 ? 1 : 0]));
			socklen_t cli_addrlen = sizeof(cli_addr[i]);
			if(sendto(sd, databuf, datalen, 0, (struct sockaddr *)&cli_addr[i], cli_addrlen) < 0){
				perror("Sending introduction message error");
			}
			else
				printf("Sending introduction message...OK\n");
		}
		//game
	}
    
    return 0;
    }
