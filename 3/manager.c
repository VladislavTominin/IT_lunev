/*
** broadcaster.c -- a datagram "client" like talker.c, except
**                  this one can broadcast
./broadcaster 255.255.255.25
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
#include <netinet/tcp.h>
#include "read_int.h"

#define SERVERPORT 4000    // the port users will be connecting to
#define MYPORT "4000"
#define BACKLOG 10
#define N 2000000000
#define  INTEGRTO 1000000000

typedef struct Integral{
  double a, b, h;
  int ncpus;
} Integral, *pIntegral;

int main(int argc, char *argv[])
{
    char s[INET6_ADDRSTRLEN];
    socklen_t sin_size;
    int numWorkers = read_int(argc, argv);
    int sockfd, rv, optval, optlen;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in their_addr; // connector's address information
    int numbytes, tcp_sockfd;
    int broadcast = 1, i = 0;
    int all_sock_fd[10];
    int bytes = 0, recieved_bytes, send_bytes;
    //char broadcast = '1'; // if that doesn't work, try this


    //bind start
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((tcp_sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Server: socket");
            continue;
        }
        if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
          perror("setsockopt(SO_REUSEADDR) failed");
        if (bind(tcp_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Server: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    //bind end
    printf("Bind success\n");
    //listening...
    if (listen(tcp_sockfd, BACKLOG) == -1)
    {
      perror("listen");
      close(sockfd);
      exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
      perror("setsockopt(SO_REUSEADDR) failed");
    // this call is what allows broadcast packets to be sent:
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast,
        sizeof broadcast) == -1) {
        perror("setsockopt (SO_BROADCAST)");
        close(sockfd);
        exit(1);
    }

    their_addr.sin_family = AF_INET;     // host byte order
    their_addr.sin_port = htons(SERVERPORT); // short, network byte order
    their_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); //use INADDR_BROADCAST 255.255.255.25
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);

    if ((numbytes=sendto(sockfd, "Let's work!", sizeof("Let's work!"), 0,
             (struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
        perror("sendto");
        close(sockfd);
        exit(1);
    }

    printf("sent %d bytes to %s\n", numbytes,
        inet_ntoa(their_addr.sin_addr));


    close(sockfd);


    //set timeout
    struct timeval tv = {
      .tv_sec = 0,
      .tv_usec = 50000
    };

    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }
    while(1)
    {
      sin_size = sizeof their_addr;
      all_sock_fd[i] = accept(tcp_sockfd, (struct sockaddr *)&their_addr, &sin_size);
      if (all_sock_fd[i] == -1)
      {
        printf("Number of connections %d\n", i);
        break;
      }
      inet_ntop(their_addr.sin_family,
                &(their_addr.sin_addr),s, sizeof s);
      printf("Server: got connection from %s\n", s);
      ++i;
    }
    //i = numWorkers
    if(i == 0)
    {
      printf("There are no workes :(\n");
      close(tcp_sockfd);
      exit(1);
    }
    int core_sum = 0, cur_core = 0, j;
    int cores[i];
    tv.tv_sec = 1;      //check this!!!
    tv.tv_usec = 0;
    for(j = 0; j < i; ++j)
    {
      if (setsockopt(all_sock_fd[j], SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
      {
          perror("Error");
      }
      optval = 1;
      optlen = sizeof(optval);
      if(setsockopt(all_sock_fd[j], SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
      {
          perror("Error");
      }
      if(setsockopt(all_sock_fd[j], SOL_TCP, TCP_KEEPIDLE, &optval, optlen) < 0)
      {
          perror("Error");
      }
      if(setsockopt(all_sock_fd[j], SOL_TCP, TCP_KEEPINTVL, &optval, optlen) < 0)
      {
          perror("Error");
      }
      if(setsockopt(all_sock_fd[j], SOL_TCP, TCP_KEEPCNT, &optval, optlen) < 0)
      {
          perror("Error");
      }
    }
    //get cores from workers
    for(j = 0; j < i; ++j)
    {
        bytes = 0;
        cur_core = 0;
        void *ptr = (void *) &cur_core;
        while(bytes < sizeof(int))
        {
            if((recieved_bytes = recv(all_sock_fd[j], ptr + bytes, sizeof(int) - bytes, 0)) <= 0)
            {
              printf("Err while recieveing num cores\n");
              goto END;
            }
            bytes += recieved_bytes;
        }
        if(bytes != sizeof(int))
        {
          printf("Problems with recieve number of cores = %d,instead of %ld\n", bytes, sizeof(int));
          goto END;
        }
        core_sum += cur_core;
        cores[j] = cur_core;
    }
    printf("Number of cores : %d \n", core_sum);
    if(core_sum < numWorkers)
    {
		goto END;
	}	
    //split data for this number of workers
    double a = 0, b = (INTEGRTO), h;
    h = b/N;
    int N_per = N/(numWorkers);
    Integral integral;
    cur_core = 0;
    int k;
    for(j = 0; j < i; ++j)
    {
      bytes = 0;
      integral.h = h;
      integral.ncpus = 0;
      for(k = 0; k < cores[j]; ++k)
      {
		  if(numWorkers == 0)
		  {
			//numWorkers
			i = j+1;
			break;
		}
		  integral.ncpus += 1;
			numWorkers -= 1;
			
		}
		if(k = 0)
		{
			//don't send msg
			i -= 1;
			break;
		}	  
		integral.a = a + h*(cur_core)*N_per;
		integral.b = a + h*(cur_core + integral.ncpus)*N_per;
		cur_core += integral.ncpus;
		printf("Num cores %d \n", integral.ncpus);
      void *ptr = (void *) &integral;
      while(bytes < sizeof(Integral))
      {
          if((send_bytes = send(all_sock_fd[j], ptr + bytes, sizeof(Integral) - bytes, 0)) <= 0)
          {
            printf("Err while sending j = %d\n", j);
            break;
          }
          bytes += send_bytes;
      }
      
    }
    printf("Send data : a = %f, b = %f\n", integral.a, integral.b);
    //recieve data
    //sleep(5);
    double answer = 0, S;
    for(j = 0; j < i; ++j)
    {
      //set timeout for block reading
      tv.tv_sec = 0;
      tv.tv_usec = 0;
      if (setsockopt(all_sock_fd[j], SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
          perror("Error");
      }
      printf("Recieve data number worker j = %d\n", j);
      bytes = 0;
      S = 0;
      void *ptr = (void *) &S;
      while(bytes < sizeof(S))
      {
          if((recieved_bytes = recv(all_sock_fd[j], ptr + bytes, sizeof(S) - bytes, 0)) <= 0)
          {
            printf("Err while recieving j = %d\n", j);
            break;
          }
          bytes += recieved_bytes;
      }
      S = *((double *) ptr);
      answer += S;
    }
    printf("Answer %f\n", answer);

    //close file descriptors

    END: do{
      for(j = 0; j < i; ++j)
      {
        close(all_sock_fd[j]);
      }
      close(tcp_sockfd);
      return 0;
    } while(0);
}
