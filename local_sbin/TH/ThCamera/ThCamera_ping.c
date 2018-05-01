#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <stdio.h>
#include <string.h>

#include <time.h>
#include <signal.h>

#define BUF_LEN (1024)
#define ECHO_PORT (3220)
#define MSG_HEAD "ThCamera_ping: "

static void sig_handler(int signum) {
    fprintf(stderr, MSG_HEAD"readform() waited time out %d", signum);
}

int main( int argc, char **argv ) {
    int ret,
        sockfd;
    char buf[BUF_LEN];
    struct sockaddr_in servaddr;
    timer_t timerid;
    struct itimerspec it;
    socklen_t socklen;
    char servip[16]; // 255.255.255.255, 15 chars + '\0'

    if(argc != 2) {
        fprintf(stderr, MSG_HEAD"Please specify a ip\n");
        return -1;
    }

    ret = sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(ret < 0) {
        fprintf(stderr, MSG_HEAD"failed to socket\n");
        return -1;
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //servaddr.sin_port = htons(13); // daytime, well-known port
    servaddr.sin_port = htons(ECHO_PORT);
    ret = inet_pton(AF_INET, argv[1], (struct sockaddr *)&servaddr.sin_addr);
    if(ret <= 0) {
        fprintf(stderr, MSG_HEAD"invalid ip\n");
        return -1;
    }

    sendto(sockfd, buf, BUF_LEN, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

    /* timer structure init */
    memset(&it, 0, sizeof(it));
    it.it_value.tv_sec = 5;

    struct sigevent evp;
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGUSR1, &act, NULL);
    memset(&evp, 0, sizeof(evp));
    evp.sigev_signo = SIGUSR1;
    evp.sigev_notify = SIGEV_SIGNAL;

    timer_create(CLOCK_REALTIME, &evp, &timerid);
    timer_settime(timerid, 0, &it, NULL);
    /* blocked function */
    ret = -1;
    ret = recvfrom(sockfd, buf, BUF_LEN, 0, (struct sockaddr *)&servaddr, &socklen);
    if((ret < 0) || (socklen != sizeof(servaddr))) {
        timer_delete(timerid);
        return -1;
    }

    timer_delete(timerid);

    buf[ret] = '\0';
    //printf("%s: receive %d bytes\n", __func__, ret);
    printf(MSG_HEAD"%s: %s\n", argv[1], buf);
    close(sockfd);
    return 0;
}
