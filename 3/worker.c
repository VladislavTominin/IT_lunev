/*
** listener.c -- a datagram sockets "server" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include "worker_proc.h"

#define MYPORT "4000"    // the port users will be connecting to
#define MAXBUFLEN 100
#define SERVERPORT 4000
typedef struct Integral{
  double a, b, h;
  int ncpus;
} Integral, *pIntegral;

int exit_variable = 0;

typedef struct Thread_Information{
	pthread_t thread_id;
	int sockfd;
} ThreadInfo, *pThreadInfo;


void *socket_checking(void *arg)
{
  pThreadInfo thread = (pThreadInfo) arg;
  int recieved_bytes, send_bytes, x = 0;
  if((recieved_bytes = recv(thread->sockfd, &x, sizeof(int), 0)) <= 0)
  {
    printf("Thread Err, Exit! %d\n", recieved_bytes);
    exit_variable = 1;
  }
	return (void *) thread;
}
int main(void)
{
    int sockfd, tcpfd;
    struct addrinfo hints, *workinfo, *p;
    int rv;
    int numbytes, i;
    struct sockaddr_in their_addr;
    char s[INET6_ADDRSTRLEN];
    socklen_t addr_len;
    char buf[MAXBUFLEN];
    int ncpus = Ncpus();
    printf("Ncpus = %d\n", ncpus);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &workinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    printf("Listener : try binding \n");
    // loop through all the results and bind to the first we can
    for(p = workinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(workinfo);

    printf("listener: waiting to recvfrom...\n");

    //recieve broadcast msg; server info in their_addr
    addr_len = sizeof(their_addr);
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n",
        inet_ntop(their_addr.sin_family,
            &(their_addr.sin_addr),
            s, sizeof s));
    //change port namber to 4000
    their_addr.sin_port = htons(SERVERPORT);
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);
    close(sockfd);
    //create tcp connection
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror("client: socket");
    }
    if (connect(sockfd, (struct sockaddr *) &their_addr, sizeof(struct sockaddr)) == -1)
    {
      close(sockfd);
      perror("client: connect");
      exit(1);
    }
    printf("Connected!\n");
    //send cores
    int bytes = 0, recieved_bytes, send_bytes;
    void *ptr = (void *) &ncpus;
    while(bytes < sizeof(int))
    {
        if((send_bytes = send(sockfd, ptr + bytes, sizeof(int) - bytes, 0)) <= 0)
        {
          printf("Err while sending\n");
          break;
        }
        bytes += send_bytes;
    }


    int optval = 1;
    int optlen = sizeof(optval);
    if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        perror("Error");
    }
    if(setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, &optval, optlen) < 0)
    {
        perror("Error");
    }
    if(setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, &optval, optlen) < 0)
    {
        perror("Error");
    }
    if(setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, &optval, optlen) < 0)
    {
        perror("Error");
    }
    //recieve work
    bytes = 0;
    Integral integral;
    ptr = (void *) &integral;
    while(bytes < sizeof(integral))
    {
        if((recieved_bytes = recv(sockfd, ptr + bytes, sizeof(integral) - bytes, 0)) <= 0)
        {
          printf("Err while recieveing\n");
          break;
        }
        bytes += recieved_bytes;
        printf("recieved bytes %d\n", recieved_bytes);
    }
    if(bytes != sizeof(integral))
    {
      printf("Problems with recieve work bytes = %d,instead of %ld\n", bytes, sizeof(integral));
      close(sockfd);
      exit(1);
    }
    //for diing
    ThreadInfo thread;
    thread.sockfd = sockfd;
    int e = pthread_create(&(thread.thread_id), NULL, socket_checking, &thread);
    if (e == -1)
    {
      printf("Can't create thread :(\n");
      return 0;
    }

    printf("a = %f, b = %f\n", integral.a, integral.b);
    double S;
    if ((S = ParallelIntegral(integral.a, integral.b, integral.h, &exit_variable, integral.ncpus)) < 0)
    {
      goto END;
    }
    bytes = 0;
    ptr = (void *) &S;
    while(bytes < sizeof(S))
    {
      //printf("Sending answer bytes = %d, sizeof(S) = %ld\n", bytes, sizeof(S));
        if((send_bytes = send(sockfd, ptr + bytes, sizeof(S) - bytes, 0)) <= 0)
        {
          printf("Err while sending\n");
          break;
        }
        bytes += send_bytes;
    }
    END: do{ close(sockfd);
    return 0;
  } while(0);
}
