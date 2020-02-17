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

#define X_AXIS_MAX 1024
#define Y_AXIS_MAX 1024
#define PADDLE_SIZE 128

#define SERV_PORT 25564 //6

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

struct countdown_frame
{
  uint16_t server_id;
  uint16_t client_id;
  struct timespec time;
  uint16_t paddle_x; //flag 0xFFFF
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

struct sockaddr_in localSock;
struct in_addr localInterface;
struct sockaddr_in groupSock;
 
int sd;
char databuf[32] = "";
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
	if (argc < 2) {
    	perror("Usage: srv <IP>");
    	exit(1);
  	}
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
  	group.imr_interface.s_addr = inet_addr(argv[1]);
  	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) { //to recv multicast
    	perror("adding multicast group");
    	close(sd);
    	exit(1);
  	} else
      printf("Subbing to multicast group...OK\n");
       
    localInterface.s_addr = inet_addr(argv[1]);
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
    {
      	perror("Setting local interface error");
		exit(1);
    }
    else
		printf("Setting the local interface...OK\n");
		printf("Demon init\n");
	daemon_init(argv[0], LOG_USER, 1000, sd);

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
				syslog(LOG_ERR, "Reading datagram message error\n");
				close(sd);
				exit(1);
			}
			else
			{
				syslog (LOG_NOTICE, "Reading datagram message from client...OK\n");
				struct init_frame * init_frame;
				init_frame = (struct init_frame *)databuf;
				if (init_frame->client_id == 0 && init_frame->server_id == 0){
					if (cli_id[i] != 0) i++;
					uint16_t id;
					do {
						id = (uint16_t)rand();
					} while (id == cli_id[i == 0 ? 1 : 0]);
					cli_id[i] = id;
					memcpy(cli_name[i], databuf + sizeof(struct init_frame), 16); 
					syslog (LOG_NOTICE, "Client name is: %s\n", cli_name[i]);
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
						syslog(LOG_ERR, "Sending datagram message error\n");
					}
					else
						syslog (LOG_NOTICE, "Sending datagram message...OK\n");
						if (i == 1) looking_for_clients = 0;
				}
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
				syslog(LOG_ERR, "Sending introduction message error\n");
			}
			else
				syslog (LOG_NOTICE, "Sending introduction message...OK\n");
		}
		//game
		short score[2] = {0, 0};
		while(1){ //match
			struct timespec prevtime;
			struct timespec ts;
			uint8_t counter = 0xFF;
			while (1){ //countdown
				timespec_get(&ts, TIME_UTC);
				for (int i = 0; i < 2; i++) {
					struct countdown_frame cd_frame;
					bzero(&cd_frame, sizeof(cd_frame));
					cd_frame.client_id = cli_id[i];
					cd_frame.server_id = srv_id;
					cd_frame.time = ts;
					cd_frame.paddle_x = 0xFFFF; //flag to countdown;
					cd_frame.countdown = counter;
					bzero(&databuf, sizeof(databuf));
					memcpy(databuf, &cd_frame, sizeof(cd_frame));
					socklen_t cli_addrlen = sizeof(cli_addr[i]);
					if(sendto(sd, databuf, datalen, 0, (struct sockaddr *)&cli_addr[i], cli_addrlen) < 0){
						syslog(LOG_ERR, "Sending countdown message error\n");
					}
				}
				counter--;
				prevtime = ts;
				if (counter == 0xFF) break;
				struct timespec req, rem;
				req.tv_sec = 0;
				req.tv_nsec = 20000000;
				nanosleep(&req, &rem);
			}
			syslog (LOG_NOTICE, "Countdown...OK\n");
			//first frame of the game
			// for (int i = 0; i < 2; i++) {
			// 	struct input_frame in_frame;
			// 	bzero(&in_frame, sizeof(in_frame));
			// 	in_frame.client_id = cli_id[i];
			// 	in_frame.server_id = srv_id;
			// 	in_frame.time = ts;
			// 	in_frame.paddle_x = (X_AXIS_MAX-PADDLE_SIZE)/2; //half-half of paddle
			// 	in_frame.ball_x = X_AXIS_MAX/2; //half
			// 	in_frame.ball_y = Y_AXIS_MAX/2; //half
			// 	bzero(&databuf, sizeof(databuf));
			// 	memcpy(databuf, &in_frame, sizeof(in_frame));
			// 	socklen_t cli_addrlen = sizeof(cli_addr[i]);
			// 	if(sendto(sd, databuf, datalen, 0, (struct sockaddr *)&cli_addr[i], cli_addrlen) < 0){
			// 		perror("Sending datagram message error");
			// 	}
			// }
			int paddle[2] = {(X_AXIS_MAX-PADDLE_SIZE)/2, (X_AXIS_MAX-PADDLE_SIZE)/2};
			double ball_x = X_AXIS_MAX/2;
			double ball_y = Y_AXIS_MAX/2;
			double v_y = 1;
			double v_x = rand() % 1 - 0.5;
			v_x =+ (v_x > 0 ? 0.5 : -1*0.5);
			timespec_get(&ts, TIME_UTC);
			while (1){ //round
				struct sockaddr src_addr;
				socklen_t src_addrlen = sizeof(src_addr);
				bzero(&src_addr, sizeof(src_addr));
				if(recvfrom(sd, databuf, datalen, 0, (struct sockaddr *)&src_addr, &src_addrlen) < 0)
				{
					syslog(LOG_ERR, "Reading datagram message error\n");
					close(sd);
					exit(1);
				}
				else
				{
					//printf("Reading datagram message from client...OK\n");
					struct output_frame * o_frame;
					o_frame = (struct output_frame *)databuf;
					if(o_frame->server_id == srv_id){
						int i = 0;
						if(o_frame->client_id == cli_id[0]) i = 0;
						else if(o_frame->client_id == cli_id[1]) i = 1; else continue;
						paddle[i] = (i == 0 ? o_frame->paddle_x : X_AXIS_MAX - o_frame->paddle_x);
						prevtime = ts;
						timespec_get(&ts, TIME_UTC);
						uint32_t t = 0; //delta_t
						if(ts.tv_sec == prevtime.tv_sec) { 
							t = ts.tv_nsec - prevtime.tv_nsec;
						} else if (ts.tv_sec < prevtime.tv_sec + 3) {
							t = (ts.tv_sec - prevtime.tv_sec) * 1000000000UL + ts.tv_nsec - prevtime.tv_nsec;
						} else exit(1);
						double dx = t*v_x/6000000;
						double dy = t*v_y/4000000;
						if(ball_x + dx > X_AXIS_MAX) {
							ball_x = 2*X_AXIS_MAX - ball_x - dx;
							v_x *= -1;
						} else if (ball_x + dx < 0) {
							ball_x = (ball_x + dx) * -1;
							v_x *= -1;
						} else ball_x += dx;
						if(ball_y + dy > Y_AXIS_MAX) {
							if (ball_x > paddle[0] + PADDLE_SIZE || ball_x < paddle[0]) {
								score[1]++;
								break;
							}
							ball_y = 2*Y_AXIS_MAX - ball_y - dy;
							v_y *= -1;
						} else if (ball_y + dy < 0) {
							if (ball_x < Y_AXIS_MAX - paddle[1] || ball_x > Y_AXIS_MAX - paddle[1] + PADDLE_SIZE) { 
								score[0]++;
								break;
							}
							ball_y = (ball_y + dy) * -1;
							v_y *= -1;
						} else ball_y += dy;
						struct input_frame return_frame;
						bzero(&return_frame, sizeof(return_frame));
						return_frame.client_id = cli_id[i];
						return_frame.server_id = srv_id;
						return_frame.paddle_x = (i == 0 ? paddle[i] : X_AXIS_MAX - paddle[i]);
						return_frame.ball_x = (uint16_t)ball_x;
						return_frame.ball_y = (i == 0 ? (uint16_t)ball_y : (uint16_t)(Y_AXIS_MAX - ball_y));
						return_frame.time = ts;
						bzero(&databuf, sizeof(databuf));
						memcpy(databuf, &return_frame, sizeof(return_frame));
						if(sendto(sd, databuf, datalen, 0, (struct sockaddr *)&src_addr, src_addrlen) < 0){
							syslog(LOG_ERR, "Sending datagram message error\n");
						}
					}
				}
			}

			for (int i = 0; i < 2; i++) {
			struct end_frame e_frame;
			bzero(&e_frame, sizeof(e_frame));
			timespec_get(&ts, TIME_UTC);
			e_frame.client_id = cli_id[i];
			e_frame.server_id = srv_id;
			e_frame.time = ts;
			e_frame.ball_x = 0xFFFF;
			e_frame.ball_y = 0xFFFF;
			e_frame.paddle_x = paddle[i == 0 ? 1 : 0];
			e_frame.score_own = score[i];
			e_frame.score_enemy = score[i == 0 ? 1 : 0];
			bzero(&databuf, sizeof(databuf));
			memcpy(databuf, &e_frame, sizeof(e_frame));
			socklen_t cli_addrlen = sizeof(cli_addr[i]);
			if(sendto(sd, databuf, datalen, 0, (struct sockaddr *)&cli_addr[i], cli_addrlen) < 0){
				syslog(LOG_ERR, "Sending final message error\n");
			}
			else
				syslog (LOG_NOTICE, "Sending final message...OK\n");
		}
			sleep(5);
			syslog (LOG_NOTICE, "New session\n");
			if (score[1] == 1) break;
			if (score[0] == 1) break;
		}
	}
    
    return 0;
    }
