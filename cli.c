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
#include        <poll.h>
#include 	<netdb.h>
#include 	<resolv.h>
#include 	<unistd.h>
#include <netinet/sctp.h>
#include <linux/input.h>

#define SERV_PORT 25564
#define MAX_SIZE 32

#define X_AXIS_MAX 1024
#define Y_AXIS_MAX 1024
#define PADDLE_SIZE 128

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

struct countdown_frame
{
  uint16_t server_id;
  uint16_t client_id;
  struct timespec time;
  uint16_t paddle_x; //flag ~0
  uint8_t countdown;
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

struct end_frame
{
  uint16_t server_id;
  uint16_t client_id;
  struct timespec time;
  uint16_t paddle_x;
  uint16_t ball_x;
  uint16_t ball_y;
  uint8_t score_own;
  uint8_t score_enemy;
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

void setCursorPosition(int XPos, int YPos) { //from 1 to n
  printf("\033[%d;%dH", XPos, YPos); 
}

void clearScreen(){
  printf("\033[2J");
}

void drawBall(int x, int y){
  setCursorPosition(y + 2, x + 1);
  printf("O\n");
}

void clearBall(int x, int y){
  setCursorPosition(y + 2, x + 1);
  printf(" \n");
}

int xScale(int x){
  return (x*72)/X_AXIS_MAX+1;
}

int yScale(int y){
  return (y*36)/Y_AXIS_MAX+1;
}

void drawPaddle(int x, int own){
  setCursorPosition(own == 1 ? 39 : 2, x+1);
  printf("=========\n");
}

void clearPaddle(int x, int own){
  setCursorPosition(own == 1 ? 39 : 2, x+1);
  printf("         \n");
}

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
  /* Setup mouse capture */
  // int fid = open("/dev/input/mice", O_RDONLY);
	// if (fid == -1){
	// 	perror("Could not open mice device!");
	// 	exit(1);
	// }
	// fprintf(stdout, "Opened mice device!\n");

  // int flags = fcntl(fid, F_GETFL, 0);
  // fcntl(fid, F_SETFL, flags | O_NONBLOCK);

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
    if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&srv_addr, &srv_addrlen) < 0) {
				perror("Reading datagram message error\n");
				close(sd);
				exit(1);
			} else {
				printf("Reading datagram message from server...OK\n");
				struct init_frame * init_frame;
				init_frame = (struct init_frame *)databuf;
				if(cli_id == init_frame->client_id && srv_id == init_frame->server_id && init_frame->waiting == 0) {
          memcpy(enemy_name, databuf + sizeof(struct init_frame), 16); 
          printf("Playing against: %s\n", enemy_name);
          break;
        }
			}
  }
  struct timespec prevtime, ts, netstamp;
  sizeof(struct countdown_frame);
  while (1) {//main loop
        //draw frame w/ dashboard
      setCursorPosition(1, 1);
      printf("+------------------------------------------------------------------------+\n"); //72+2
      for (int i = 2; i < 40; i++) {
        printf("|                                                                        |\n");
      }
      printf("+------------------------------------------------------------------------+\n");
    //first frame
      if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&srv_addr, &srv_addrlen) < 0) {
				perror("Reading datagram message error\n");
				close(sd);
				exit(1);
			} else {
				struct countdown_frame * cd_frame;
				cd_frame = (struct countdown_frame *)databuf;
				if(cli_id == cd_frame->client_id && srv_id == cd_frame->server_id) {
          if(cd_frame->paddle_x == 0xFFFF){
            netstamp = cd_frame->time;
            setCursorPosition(7, 25);
            printf("Game starts in: %f sec\n", cd_frame->countdown/50.0);
          } else break;
        }
			}
      int fid = open("/dev/input/mice", O_RDONLY);
      if (fid == -1){
        perror("Could not open mice device!");
        exit(1);
      }

      //int flags = fcntl(fid, F_GETFL, 0);
      //fcntl(fid, F_SETFL, flags | O_NONBLOCK);
      while(1){ //countdown loop
        if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&srv_addr, &srv_addrlen) < 0) {
          perror("Reading datagram message error\n");
          close(sd);
          exit(1);
        } else {
          struct countdown_frame * cd_frame;
          cd_frame = (struct countdown_frame *)databuf;
          if(cli_id == cd_frame->client_id && srv_id == cd_frame->server_id) {
            if(cd_frame->paddle_x == 0xFFFF){
              if((cd_frame->time.tv_sec > netstamp.tv_sec) || (cd_frame->time.tv_sec == netstamp.tv_sec && cd_frame->time.tv_nsec > netstamp.tv_nsec)) { //discard old packets
                netstamp = cd_frame->time;
                setCursorPosition(7, 25);
                printf("Game starts in: %f sec\n", cd_frame->countdown/50.0);
                if (cd_frame->countdown == 0) break;
              }
            } else break;
          }
        }
      }
      setCursorPosition(7, 25);
      printf("                            \n");
      timespec_get(&ts, TIME_UTC);
      int ball_x = X_AXIS_MAX/2;
      int ball_y = Y_AXIS_MAX/2;
      int paddle_own = (X_AXIS_MAX-PADDLE_SIZE)/2;
      int paddle_enemy = (X_AXIS_MAX-PADDLE_SIZE)/2;
      drawBall(xScale(ball_x), yScale(ball_y));
      drawPaddle(xScale(paddle_own), 1);
      while(1){ //round loop
        int delta_x = 0;
        signed char x;
        // prevtime = ts;
        // struct timespec prevtime2;
        // bzero(&prevtime2, sizeof(prevtime2));
        // while (1){ //capture mouse for 10ms
        //   timespec_get(&ts, TIME_UTC);
        //   if (ts.tv_sec == prevtime.tv_sec){
        //     if (ts.tv_nsec > prevtime.tv_nsec + 10000000UL) break;
        //   }
        //   if (ts.tv_sec > prevtime.tv_sec) {
        //     if (ts.tv_nsec > prevtime.tv_nsec + 10000000UL - 1000000000UL) break;
        //   }
          char event[4];
          struct pollfd fds;
          int nfds = 1;
          fds.fd = fid;
          fds.events = POLLIN;
          int timeout = 2;
          if(poll(&fds, nfds, timeout) > 0){
            int bytes = read(fid, &event, 4);
            if(bytes > 0) {
              x = event[1];
              delta_x = x;
            }
          }
          // prevtime2 = ts;
          struct timespec req, rem;
          req.tv_sec = 0;
          req.tv_nsec = 10000000;
          nanosleep(&req, &rem);
        // }
        if (delta_x != 0) { //did pos change?
          clearPaddle(xScale(paddle_own), 1);
          paddle_own += delta_x;
          if (paddle_own < 0) paddle_own = 0;
          if (paddle_own > X_AXIS_MAX-PADDLE_SIZE) paddle_own = X_AXIS_MAX-PADDLE_SIZE;
          drawPaddle(xScale(paddle_own), 1);
        }
        struct output_frame o_frame;
        timespec_get(&ts, TIME_UTC);
        o_frame.client_id = cli_id;
        o_frame.server_id = srv_id;
        o_frame.time = ts;
        o_frame.paddle_x = paddle_own;
        memcpy(databuf, &o_frame, sizeof(o_frame));
        if(sendto(sd, databuf, sizeof(databuf), 0, (struct sockaddr*)&srv_addr, srv_addrlen) < 0){ //sharing paddle position
          perror("Sending datagram message error");
        }
        if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&srv_addr, &srv_addrlen) < 0) {
          perror("Reading datagram message error\n");
        } else {
          struct input_frame * i_frame;
          i_frame = (struct input_frame *)databuf;
          if(cli_id == i_frame->client_id && srv_id == i_frame->server_id) {
            if(i_frame->ball_x == 0xFFFF && i_frame->ball_y == 0xFFFF) {
              struct end_frame * e_frame;
              e_frame = (struct end_frame *)databuf;
              setCursorPosition(18, 35);
              printf("%d:%d", e_frame->score_own, e_frame->score_enemy);
              setCursorPosition(41, 1);
              exit(1);
            } else
            if((i_frame->time.tv_sec > netstamp.tv_sec) || (i_frame->time.tv_sec == netstamp.tv_sec && i_frame->time.tv_nsec > netstamp.tv_nsec)) { //discard old packets
              netstamp = i_frame->time;
              clearPaddle(xScale(paddle_enemy), 0);
              clearBall(xScale(ball_x), yScale(ball_y));
              ball_x = i_frame->ball_x;
              ball_y = i_frame->ball_y;
              paddle_enemy = i_frame->paddle_x;
              drawPaddle(xScale(i_frame->paddle_x), 0);
              drawBall(xScale(ball_x), yScale(ball_y));
            }
          }
        }
      }
    
  }
  return 0;
}

