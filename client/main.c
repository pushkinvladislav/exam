#include <stdio.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <stdlib.h>  
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>


typedef struct
{
  int upper_limits;
  int number_of_tries;
} task_data_t;

typedef struct {
  int upper_limits;
  long number_of_tries;
  struct sockaddr_in *server; 
  long double *results; 
} thread_args_t;


void *send_thread(void *arg) {
  thread_args_t *task_data = (thread_args_t*) arg;
  int servsock = socket(PF_INET, SOCK_STREAM, 0);
  if(servsock < 0) {
    perror("Create new socket to server");
    exit(EXIT_FAILURE);
  }
  struct sockaddr_in listenaddr;
  listenaddr.sin_family = AF_INET;
  listenaddr.sin_addr.s_addr = INADDR_ANY;
  listenaddr.sin_port = 0;
  
  if(bind(servsock, (struct sockaddr*) &listenaddr, sizeof(listenaddr)) < 0) {
    perror("Can't create listen socket");
    exit(EXIT_FAILURE);
  }
  socklen_t servaddrlen = sizeof(struct sockaddr_in);
  if(connect(servsock, (struct sockaddr*)task_data->server,
    servaddrlen) < 0) {
    perror("Connect to server failed!");
    exit(EXIT_FAILURE);
  }
  task_data_t senddata;
  senddata.upper_limits = task_data->upper_limits;
  senddata.number_of_tries = task_data->number_of_tries;
  
  if(send(servsock, &senddata, sizeof(senddata), 0) < 0) {
    perror("Sending data to server failed");
    exit(EXIT_FAILURE);
  }
  
  int recv_byte = recv(servsock, task_data->results, sizeof(long double), 0);
  if(recv_byte == 0) {
    fprintf(stderr, "Server %s on port %d die!\nCancel calculate, on all",
        inet_ntoa(task_data->server->sin_addr),
        ntohs(task_data->server->sin_port));
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "Server %s on port %d finish!\n",
        inet_ntoa(task_data->server->sin_addr),
        ntohs(task_data->server->sin_port));
  return NULL;
}


int main(int argc, char** argv) {
  if(argc < 3) {
    fprintf(stderr, "Usage: %s upper_limits number_of_tries [maxserv]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  
  int number_of_tries = atoi(argv[2]);
  if(number_of_tries == 0) {
    fprintf(stderr, "Num of try is invalid\n");
    exit(EXIT_FAILURE);
  }
  int maxservu = 1000000;
  if(argc == 4) {
   maxservu = atoi(argv[3]);
   if (maxservu < 1) {
    fprintf(stderr, "Error number of max servers\n");
    exit(EXIT_FAILURE);
   }
  }
  int upper_limits = atoi(argv[1]);
  if(upper_limits == 0) {
    fprintf(stderr, "upper_limits is invalid\n");
    exit(EXIT_FAILURE);
  }
  
  int sockbrcast = socket(PF_INET, SOCK_DGRAM, 0);
  if(sockbrcast == -1) {
    perror("Create broadcast socket failed");
    exit(EXIT_FAILURE);
  }
  
  
  int port_rcv = 0;
  struct sockaddr_in addrbrcast_rcv;
  bzero(&addrbrcast_rcv, sizeof(addrbrcast_rcv));
  addrbrcast_rcv.sin_family = AF_INET;
  addrbrcast_rcv.sin_addr.s_addr = htonl(INADDR_ANY);
  addrbrcast_rcv.sin_port = 0;
  
  if(bind(sockbrcast, (struct sockaddr *) &addrbrcast_rcv,
      sizeof(addrbrcast_rcv)) < 0) {
    perror("Bind broadcast socket failed");
    close(sockbrcast);
    exit(EXIT_FAILURE);
  }
  
  int port_snd = 16667;
  struct sockaddr_in addrbrcast_snd;
  bzero(&addrbrcast_snd, sizeof(addrbrcast_snd));
  addrbrcast_snd.sin_family = AF_INET;
  addrbrcast_snd.sin_port = port_snd;
  addrbrcast_snd.sin_addr.s_addr = htonl(0xffffffff);

  int access = 1;
  
  if(setsockopt(sockbrcast, SOL_SOCKET, SO_BROADCAST,
       (const void*) &access, sizeof(access)) < 0) {
    perror("Can't accept broadcast option at socket to send");
    close(sockbrcast);
    exit(EXIT_FAILURE);
  }
  int msgsize = sizeof(char) * 18;
  void *hellomesg = malloc(msgsize);
  bzero(hellomesg, msgsize);
  strcpy(hellomesg, "Hello Integral");

  if(sendto(sockbrcast, hellomesg, msgsize, 0,
      (struct sockaddr*) &addrbrcast_snd, sizeof(addrbrcast_snd)) < 0) {
    perror("Sending broadcast");
    close(sockbrcast);
    exit(EXIT_FAILURE);
  }
  
  fcntl(sockbrcast, F_SETFL, O_NONBLOCK);
  
  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(sockbrcast, &readset);
  
  struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;
  
  struct sockaddr_in *servers = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
  bzero(servers, sizeof(struct sockaddr_in));
  int servcount = 0;
  int maxserv = 1;
  socklen_t servaddrlen = sizeof(struct sockaddr_in);
  while(select(sockbrcast + 1, &readset, NULL, &readset, &timeout) > 0) {
    printf("\nheeere");
    int rdbyte = recvfrom(sockbrcast, (void*) hellomesg, msgsize,MSG_TRUNC, (struct sockaddr*) &servers[servcount], &servaddrlen);
    printf("%s, %d\n", inet_ntoa(servers[servcount].sin_addr),servers[servcount].sin_port);

    if(rdbyte == msgsize && strcmp(hellomesg, "Hello Client") == 0) {
      servcount++;

      if(servcount >= maxserv) {
        printf("inc serv count %d   %d",servcount, maxserv);
        servers = realloc(servers, sizeof(struct sockaddr_in) * (maxserv + 1));
        if(servers == NULL) {
          perror("Realloc failed");
          close(sockbrcast);
          exit(EXIT_FAILURE);
        }
        printf("\nheeere");
        bzero(&servers[servcount], servaddrlen);
        maxserv++;
      }
      printf("\nheeere 1");
      FD_ZERO(&readset);
      printf("\nheeere 2");
      FD_SET(sockbrcast, &readset);
    }
  }
  int i;

  if(servcount < 1) {
    fprintf(stderr, "No servers found!\n");
    exit(EXIT_FAILURE);
  }
  if(argc > 3 && maxservu <= servcount)
   servcount = maxservu;
  for(i = 0; i < servcount; ++i) {
    printf("Server answer from %s on port %d\n",
        inet_ntoa(servers[i].sin_addr), ntohs(servers[i].sin_port));
  }
  printf("\n");
  free(hellomesg);
  
  long double *results =
    (long double*) malloc(sizeof(long double) * servcount);
  pthread_t *tid = (pthread_t*) malloc(sizeof(pthread_t) * servcount);
  for(i = 0; i < servcount; ++i) {
    thread_args_t *args = (thread_args_t*) malloc (sizeof(thread_args_t));
    args->upper_limits = upper_limits;
    args->number_of_tries = number_of_tries / servcount + 1;
    args->results = &results[i];
    args->server = &servers[i];
    
    if(pthread_create(&tid[i], NULL, send_thread, args) != 0) {
      perror("Create send thread failed");
      exit(EXIT_FAILURE);
    }
  }
  long double res = 0;
  for(i = 0; i < servcount; ++i)
    pthread_join(tid[i], NULL);
  
  for(i = 0; i < servcount; ++i)
    res += results[i];
  res /= number_of_tries;
  res *= upper_limits;

  
  free(servers);
  printf("\nResult: %Lf\n", res);
  return (EXIT_SUCCESS);
}