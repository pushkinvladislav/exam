#include <stdio.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <string.h>

#define RCVPORT 16667
#define FUNC(x) x*x*3/2



typedef struct
{
  int upper_limits;
  int number_of_tries;
} task_data_t;

typedef struct
{
  int upper_limits;
  long number_of_tries;
  long double *result_array;
} thread_args_t;

typedef struct
{
  int sock;
  pthread_t *calcthreads;
  int threadnum;
} checker_args_t;


void *execute_client_check(void *argument)
{
  checker_args_t *checker_arguments = (checker_args_t *)argument;
  char buffer[10];
  recv(checker_arguments->sock, &buffer, 10, 0);

  int status;
  for (int i = 0; i < checker_arguments->threadnum; ++i)
  {
    status = pthread_kill(checker_arguments->calcthreads[i], SIGUSR1);
  }

  return NULL;
}

void *listen_broadcast(void *arg)
{
  int *isbusy = arg;
  int sockbrcast = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockbrcast == -1)
  {
    perror("Create broadcast socket failed");
    exit(EXIT_FAILURE);
  }

  int port_rcv = RCVPORT;
  struct sockaddr_in addrbrcast_rcv;
  bzero(&addrbrcast_rcv, sizeof(addrbrcast_rcv));
  addrbrcast_rcv.sin_family = AF_INET;
  addrbrcast_rcv.sin_addr.s_addr = htonl(INADDR_ANY);
  addrbrcast_rcv.sin_port = port_rcv;
  if (bind(sockbrcast, (struct sockaddr *)&addrbrcast_rcv,
           sizeof(addrbrcast_rcv)) < 0)
  {
    perror("Bind broadcast socket failed");
    close(sockbrcast);
    exit(EXIT_FAILURE);
  }

  int msgsize = sizeof(char) * 18;
  char hellomesg[18];
  bzero(hellomesg, msgsize);

  fcntl(sockbrcast, F_SETFL, O_NONBLOCK);

  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(sockbrcast, &readset);

  struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  struct sockaddr_in client;
  ;
  bzero(&client, sizeof(client));
  socklen_t servaddrlen = sizeof(struct sockaddr_in);
  char helloanswer[18];
  bzero(helloanswer, msgsize);
  strcpy(helloanswer, "Hello Client");
  int sockst = 1;
  while (sockst > 0)
  {
    sockst = select(sockbrcast + 1, &readset, NULL, &readset, NULL);
    if (sockst == -1)
    {
      perror("Broblems on broadcast socket");
      exit(EXIT_FAILURE);
    }
    int rdbyte = recvfrom(sockbrcast, (void *)hellomesg, msgsize, MSG_TRUNC,
                          (struct sockaddr *)&client,
                          &servaddrlen);
    if (rdbyte == msgsize && strcmp(hellomesg, "Hello Integral") == 0 &&
        *isbusy == 0)
    {
      if (sendto(sockbrcast, helloanswer, msgsize, 0,
                 (struct sockaddr *)&client, sizeof(struct sockaddr_in)) < 0)
      {
        perror("Sending answer");
        close(sockbrcast);
        exit(EXIT_FAILURE);
      }
    }
    FD_ZERO(&readset);
    FD_SET(sockbrcast, &readset);
  }
  printf("fall here");
  return NULL;
}

void *perform_calculation(void *thread_data)
{
  printf("commence computation");
  thread_args_t *info = (thread_args_t *)thread_data;
  long double result_value = 0;
  int limit_value = info->upper_limits;
  int tries = info->number_of_tries;
  unsigned seed = limit_value;
  for (int i = 0; i < tries; ++i)
  {
    int dividend = rand_r(&seed);
    int divisor = rand_r(&seed);
    double x = dividend % limit_value + (divisor / (divisor * 1.0 + dividend * 1.0));
    result_value += FUNC(x); // custom_function represents the original FUNC macro
  }
  *(info->result_array) = result_value;
  return NULL;
}



void thread_cancel(int signo)
{
  pthread_exit(PTHREAD_CANCELED);
}


int main(int argc, char **argv)
{

  if (argc > 3)
  {
    fprintf(stderr, "Usage: %s [numofcpus]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int numofthread;

  if (argc == 2)
  {
    numofthread = atoi(argv[1]);
    if (numofthread < 1)
    {
      fprintf(stderr, "Incorrect num of threads!\n");
      exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Num of threads forced to %d\n", numofthread);
  }
  else
  {
    numofthread = sysconf(_SC_NPROCESSORS_ONLN);
    if (numofthread < 1)
    {
      fprintf(stderr, "Can't detect num of processors\n"
                      "Continue in two threads\n");
      numofthread = 2;
    }
    fprintf(stdout, "Num of threads detected automatically it's %d\n\n",
            numofthread);
  }
  struct sigaction cancel_act;
  memset(&cancel_act, 0, sizeof(cancel_act));
  cancel_act.sa_handler = thread_cancel;
  sigfillset(&cancel_act.sa_mask);
  sigaction(SIGUSR1, &cancel_act, NULL);

  pthread_t broadcast_thread;
  int isbusy = 1;

  isbusy = 1;

  if (pthread_create(&broadcast_thread, NULL, listen_broadcast, &isbusy))
  {
    fprintf(stderr, "Can't create broadcast listen thread");
    perror("Detail:");
    exit(EXIT_FAILURE);
  }

  int listener;
  struct sockaddr_in addr;
  listener = socket(PF_INET, SOCK_STREAM, 0);
  if (listener < 0)
  {
    perror("Can't create listen socket");
    exit(EXIT_FAILURE);
  }
  addr.sin_family = AF_INET;
  addr.sin_port = RCVPORT;
  addr.sin_addr.s_addr = INADDR_ANY;
  int a = 1;

  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(a)) < 0)
  {
    perror("Set listener socket options");
    exit(EXIT_FAILURE);
  }

  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("Can't bind listen socket");
    exit(EXIT_FAILURE);
  }

  if (listen(listener, 1) < 0)
  {
    perror("Problem with listen socket");
    exit(EXIT_FAILURE);
  }

  int needexit = 0;
  while (needexit == 0)
  {
    fprintf(stdout, "\nWait new connection...\n\n");
    printf("start accepting");
    int client;
    isbusy = 0;
    struct sockaddr_in addrclient;
    socklen_t addrclientsize = sizeof(addrclient);
    printf("start accepting");
    client = accept(listener, (struct sockaddr *)&addrclient, &addrclientsize);
    if (client < 0)
    {
      perror("Client accepting");
    }
    printf("something was accepted");
    isbusy = 1;
    task_data_t data;
    printf("start recv");
    int read_bytes = recv(client, &data, sizeof(data), 0);
    if (read_bytes != sizeof(data) || data.upper_limits < 1 || data.number_of_tries < 1)
    {
      fprintf(stderr, "Invalid data from %s on port %d, reset peer\n", inet_ntoa(addrclient.sin_addr), ntohs(addrclient.sin_port));
      close(client);
      isbusy = 0;
    }
    else
    {
      fprintf(stdout, "New task from %s on port %d\nupper_limits: %d\n number_of_triess: %d\n", inet_ntoa(addrclient.sin_addr),
              ntohs(addrclient.sin_port), data.upper_limits, data.number_of_tries);
      thread_args_t *tinfo;
      pthread_t *calc_threads =
          (pthread_t *)malloc(sizeof(pthread_t) * numofthread);
      int threads_trys = data.number_of_tries % numofthread;
      long double *result_array =
          (long double *)malloc(sizeof(long double) * numofthread);
      tinfo = (thread_args_t *)malloc(sizeof(thread_args_t) *
                                      numofthread);
      int numofthreadtry = data.number_of_tries / numofthread + 1;
      for (int i = 0; i < numofthread; ++i)
      {
        tinfo[i].upper_limits = data.upper_limits;
        tinfo[i].number_of_tries = numofthreadtry;
        tinfo[i].result_array = &result_array[i];
        if (pthread_create(&calc_threads[i], NULL, perform_calculation, &tinfo[i]) != 0)
        {
          fprintf(stderr, "Can't create thread by num %d", i);
          perror("Detail:");
          exit(EXIT_FAILURE);
        }
      }

      checker_args_t checker_arg;
      checker_arg.calcthreads = calc_threads;
      checker_arg.threadnum = numofthread;
      checker_arg.sock = client;
      pthread_t checker_thread;
      if (pthread_create(&checker_thread, NULL, execute_client_check,
                         &checker_arg) != 0)
      {
        fprintf(stderr, "Can't create checker thread");
        perror("Detail:");
        exit(EXIT_FAILURE);
      }
      int iscanceled = 0;
      int *exitstat;
      for (int i = 0; i < numofthread; ++i)
      {
        pthread_join(calc_threads[i], (void *)&exitstat);
        if (exitstat == PTHREAD_CANCELED)
          iscanceled = 1;
      }
      if (iscanceled != 1)
      {
        long double *res = (long double *)malloc(sizeof(long double));
        bzero(res, sizeof(long double));
        *res = 0.0;
        for (int i = 0; i < numofthread; ++i)
          *res += result_array[i];
        pthread_kill(checker_thread, SIGUSR1);
        if (send(client, res, sizeof(long double), 0) < 0)
        {
          perror("Sending error");
        }
        close(client);
        free(res);
        free(result_array);
        free(calc_threads);
        free(tinfo);
        isbusy = 0;
        fprintf(stdout, "perform_calculation and send finish!\n");
      }
      else
      {
        fprintf(stderr, "Client die!\n");
        close(client);
        ;
        free(result_array);
        free(calc_threads);
        free(tinfo);
      }
    }
  }

  return (EXIT_SUCCESS);
}