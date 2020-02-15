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
#include 	<netdb.h>
#include 	<resolv.h>
#include 	<unistd.h>
#include <netinet/sctp.h>

#define MAXLINE 2048
#define SA      struct sockaddr
#define SERV_PORT 25564
#define MAX_SIZE 24

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

struct data
{
  uint16_t server_id;
  uint16_t client_id;
};

struct sockaddr_in    localSock;
struct in_addr localInterface;
struct sockaddr_in groupSock;

int sd;
char databuf[MAX_SIZE] = "";
int datalen = sizeof(databuf);

/* Send Multicast Datagram code example. */
int main (int argc, char *argv[ ])
{
  if (argc < 3) {
    perror("Usage: cli <IP> <name>");
    exit(1);
  }
  if (strlen(argv[2]) > 16) {
    perror("Max <name> length = 16");
    exit(1);
  }
  /* Create a datagram socket on which to send. */
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0)
  {
    perror("Opening datagram socket error");
    exit(1);
  }
  else
    printf("Opening the datagram socket...OK.\n");
  
  memset((char *) &groupSock, 0, sizeof(groupSock));
  groupSock.sin_family = AF_INET;
  groupSock.sin_addr.s_addr = inet_addr("225.1.1.1");
  groupSock.sin_port = htons(SERV_PORT);

  memset((char *) &localSock, 0, sizeof(localSock)); //to recv
  localSock.sin_family = AF_INET;
  localSock.sin_port = 0;
  localSock.sin_addr.s_addr  = INADDR_ANY;

  if (bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) { //to recv
    perror("binding datagram socket");
    close(sd);
    exit(1);
  }

  char loopch = 0;
  if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
    perror("Setting IP_MULTICAST_LOOP error");
    close(sd);
    exit(1);
  }
  else
  printf("Disabling the loopback...OK.\n");
    
  /* Set local interface for outbound multicast datagrams. */
  /* The IP address specified must be associated with a local, */
  /* multicast capable interface. */
  localInterface.s_addr = inet_addr(argv[1]);
  if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
    perror("Setting local interface error");
    exit(1);
  }
  else
    printf("Setting the local interface...OK\n");
  
  struct init_frame init_frame;
  init_frame.client_id = 0;
  init_frame.server_id = 0;
  init_frame.waiting = 1;

  memcpy(databuf, &init_frame, sizeof(init_frame));
  memcpy(databuf + sizeof(init_frame), argv[2], strlen(argv[2]));

  if(sendto(sd, databuf, sizeof(databuf), 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0){ //Looking for server
    perror("Sending datagram message error");
  } else
    printf("Looking for server...OK\n");
  
  //establishing communication
		struct sockaddr srv_addr;
    socklen_t srv_addrlen = sizeof(srv_addr);
		uint16_t srv_id = 0;
    uint16_t cli_id = 0; 
    char enemy_name[18];
  if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&srv_addr, &srv_addrlen) < 0)
    {
      perror("Reading datagram message error\n");
      close(sd);
      exit(1);
    }
    else
    {
      printf("Reading datagram message from server...OK\n");
      struct init_frame * init_frame;
      init_frame = (struct init_frame *)databuf;
      cli_id = init_frame->client_id;
      srv_id = init_frame->server_id;
    }
  while(1){
    if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&srv_addr, &srv_addrlen) < 0)
			{
				perror("Reading datagram message error\n");
				close(sd);
				exit(1);
			}
			else
			{
				printf("Reading datagram message from server...OK\n");
				struct init_frame * init_frame;
				init_frame = (struct init_frame *)databuf;
				if(cli_id == init_frame->client_id && srv_id == init_frame->server_id && init_frame->waiting == 0) {
            memcpy(enemy_name, databuf + sizeof(struct init_frame), 16); 
            break;
          }
			}
      
  }
  
  
  return 0;
  }

