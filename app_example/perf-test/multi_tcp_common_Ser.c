/*
*
* Copyright (c) 2018 Huawei Technologies Co.,Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at:
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <fcntl.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <time.h>
#include <linux/tcp.h>
#define __USE_GNU
#include <sched.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

#define _BIND bind
#define _LISTEN listen
#define _SOCKET socket
#define _ACCEPT accept
#define _SEND send
#define _RECV recv
#define _CLOSE close
#define _CONNECT connect
#define _PROTOCOL IPPROTO_TCP

#define MAX_TEST_TIME 1000
#define MSG_LENGTH 256
#define CORE_NUM 8
#define START_CPU_ID 2
#define MAX_CONN_LIMIT 256
#define MAX_PORT_NUM 65535
//1:A-B ;0:A-B-A
#define SEND_RECV_MODE 1
#define MAX_EVENTS 1000
#define Default_PortID 12345
#define Default_SleepCnt 10000
#define Default_SleepTime 1000000
#define  Flag_Print 1
#define  Fd_Number 1

static struct sockaddr_in g_dest;
static struct sockaddr_in g_src;
static struct sockaddr_in g_recv;

int times = MAX_TEST_TIME;
int msg_length = MSG_LENGTH;
int coreNum = CORE_NUM;
int startCPUId = START_CPU_ID;
int connectNum = MAX_CONN_LIMIT;
int msgMode = SEND_RECV_MODE;
int sleepCnt = Default_SleepCnt;
int sleepTime = Default_SleepTime;      //us
int fdNumber = Fd_Number;
int flagPrint = Flag_Print;
int unitPrint = 0;
int waitTime = 0;

int srcPort = Default_PortID;
int destPort = 0;
int recvPort = 0;
char sendarr[256] = "";
char recvarr[256] = "";
char destarr[256] = "";

static void
setArgsDefault ()
{

  memset (&g_dest, 0, sizeof (g_dest));
  g_dest.sin_family = AF_INET;
  g_dest.sin_addr.s_addr = inet_addr ("127.0.0.1");
  g_dest.sin_port = htons (12345);
  bzero (&(g_dest.sin_zero), 8);

  memset (&g_src, 0, sizeof (g_src));
  g_src.sin_family = AF_INET;
  g_src.sin_addr.s_addr = inet_addr ("0.0.0.0");
  g_src.sin_port = htons (7895);
  bzero (&(g_src.sin_zero), 8);

  memset (&g_recv, 0, sizeof (g_recv));
  g_recv.sin_family = AF_INET;
  g_recv.sin_addr.s_addr = inet_addr ("0.0.0.0");
  g_recv.sin_port = htons (7895);
  bzero (&(g_recv.sin_zero), 8);

}

static int
process_arg (int argc, char *argv[])
{
  int opt = 0;
  int error = 0;
  const char *optstring = "p:d:s:a:l:t:e:i:f:r:n:w:u:x:";
  int rw_mark = 0;

  if (argc < 4)
    {
      printf
        ("Param info :-p dest_portid; -d dest_serverIP; -a src_portid; -s src_clientIP; -l msg_length; \n");
      printf
        ("\t-t MAX_TEST_TIME; -i  msg_interval ; -f  client fd number ; -r receive port; -n connectNum(one server vs n client)\n");
      return 0;
    }
  while ((opt = getopt (argc, argv, optstring)) != -1)
    {
      switch (opt)
        {
        case 'p':
          destPort = atoi (optarg);
          g_dest.sin_port = htons (atoi (optarg));
          break;
        case 'd':
          stpcpy (destarr, optarg);
          g_dest.sin_addr.s_addr = inet_addr (optarg);
          break;
        case 's':
          stpcpy (sendarr, optarg);
          g_src.sin_addr.s_addr = inet_addr (optarg);
          g_recv.sin_addr.s_addr = inet_addr (optarg);
          break;
        case 'a':
          //g_src.sin_port = htons(atoi(optarg));
          srcPort = atoi (optarg);
          break;
        case 'l':
          msg_length = atoi (optarg);
          break;
        case 't':
          times = atoi (optarg);
          break;
        case 'e':
          sleepCnt = atoi (optarg);
          break;
        case 'i':
          sleepTime = atoi (optarg);
          break;
        case 'f':
          fdNumber = atoi (optarg);
          break;
        case 'r':
          recvPort = atoi (optarg);
          g_recv.sin_port = htons (atoi (optarg));
          break;
        case 'n':
          connectNum = atoi (optarg);
          break;
        case 'w':
          waitTime = atoi (optarg);
          break;
        case 'u':
          unitPrint = atoi (optarg);
          break;
        case 'x':
          flagPrint = atoi (optarg);
          break;

        }
    }
  return 1;
}

void
process_client (void)
{
  int sendLen = 0;
  int i = 0, t = 0, p = 0, optval = 0, ret = 0;
  char send_buf[1000] = "";
  int c_socketfd[100] = { 0 };
  int errbind[1000] = { 0 };
  int errconn[1000] = { 0 };

  int send_count[1000] = { 0 };
  int pps = 0;
  long pps_time = 0;

  struct timespec startTime, endTime;
  struct timespec countStart;
  struct timespec countEnd;

  memset (&countStart, 0, sizeof (countStart));
  memset (&countEnd, 0, sizeof (countEnd));

  for (i = 0; i < fdNumber; i++)
    {
      c_socketfd[i] = _SOCKET (PF_INET, SOCK_STREAM, _PROTOCOL);
      if (0 > c_socketfd[i])
        {
          printf ("client %d failed,err %d\n", i, errno);
        }
      else
        {
          printf ("client %d created success\n", i);
        }
    }

  for (i = 0; i < fdNumber; i++)
    {
      optval = 1;
      ret =
        setsockopt (c_socketfd[i], SOL_SOCKET, SO_REUSEADDR, (void *) &optval,
                    sizeof (optval));
      if (ret == -1)
        {
          printf ("Couldn't setsockopt(SO_REUSEADDR)\n");
          break;
        }
    }

  for (i = 0; i < fdNumber; i++)
    {
      g_src.sin_port = htons (srcPort + i);
      errbind[i] =
        _BIND (c_socketfd[i], (struct sockaddr *) &g_src, sizeof (g_src));
      if (errbind[i] < 0)
        {
          printf ("client %d bind Failed %d\n", i, errno);
          _CLOSE (c_socketfd[i]);
          c_socketfd[i] = -1;
          continue;
        }
      else
        {
          printf ("client %d bind Success port:%d IP:%s\n", i,
                  ntohs (g_src.sin_port),
                  inet_ntoa (*((struct in_addr *) &(g_src.sin_addr.s_addr))));
        }
    }
  for (i = 0; i < fdNumber; i++)
    {
      if (errbind[i] >= 0)
        {
          errconn[i] =
            _CONNECT (c_socketfd[i], (struct sockaddr *) &g_dest,
                      sizeof (g_dest));
          if (errconn[i] < 0)
            {
              printf ("client %d Connect Failed %d\n", i, errno);
              _CLOSE (c_socketfd[i]);
              c_socketfd[i] = -1;
              continue;
            }
          else
            {
              printf ("client %d Connect Success port:%d, IP:%s\n", i,
                      ntohs (g_dest.sin_port),
                      inet_ntoa (*
                                 ((struct in_addr *)
                                  &(g_dest.sin_addr.s_addr))));
            }
        }
    }

  sleep (1);

  clock_gettime (CLOCK_MONOTONIC, &startTime);
  clock_gettime (CLOCK_MONOTONIC, &countStart);

  for (t = 0; t < times; t++)
    {
      for (i = 0; i < fdNumber; i++)
        {
          if (c_socketfd[i] < 0)
            {
              continue;
            }
          do
            {
              sendLen = _SEND (c_socketfd[i], send_buf, msg_length, 0);
            }
          while (sendLen <= 0);
          send_count[i]++;
        }

      if (0 != sleepTime)
        {
          if ((t % sleepCnt) == 0)
            {
              usleep (sleepTime);
            }
        }

      if ((send_count[0] % unitPrint) == 0)
        {
          clock_gettime (CLOCK_MONOTONIC, &countEnd);

          pps_time =
            (countEnd.tv_sec - countStart.tv_sec) * 1000000000 +
            countEnd.tv_nsec - countStart.tv_nsec;
          pps = ((float) 1000000000 / pps_time) * unitPrint * fdNumber;
          if ((flagPrint != 0))
            {
              printf (" sendcount %d, time: %ld ns\n",
                      send_count[0] * fdNumber, pps_time);
            }
          printf ("sendcount %d , pps=%d\n", send_count[0] * fdNumber, pps);
          clock_gettime (CLOCK_MONOTONIC, &countStart);
        }
    }

  clock_gettime (CLOCK_MONOTONIC, &endTime);

  for (i = 0; i < fdNumber; i++)
    {
      printf ("client %d send %d , sendtime :%ld s %ld ns\n", i,
              send_count[i], endTime.tv_sec - startTime.tv_sec,
              endTime.tv_nsec - startTime.tv_nsec);
    }

  for (i = 0; i < fdNumber; i++)
    {
      printf ("client %d close!\n", i);
      if (c_socketfd[i] > 0)
        _CLOSE (c_socketfd[i]);
    }

  pthread_exit (NULL);
}

/*
   using this thread to do recv msg;
*/
void *
process_server_msg_thread (void *pArgv)
{
  int recvLen = 0, recvLen2 = 0;
  char send_buf[1000];
  char recv_buf[1000];
  int recv_count = 0;
  long recv_ppstime = 0;
  int recv_pps = 0;
  int ret;
  struct timespec recvStart;
  struct timespec recvEnd;
  memset (&recvStart, 0, sizeof (recvStart));
  memset (&recvEnd, 0, sizeof (recvEnd));

  pthread_detach (pthread_self ());

  int msgFd = *(int *) pArgv;
  clock_gettime (CLOCK_MONOTONIC, &recvStart);
  while (recv_count < times)
    {
      recvLen = 0;
      recvLen2 = 0;
      while (1)
        {
          recvLen2 =
            recv (msgFd, recv_buf + recvLen, msg_length - recvLen, 0);
          if (recvLen2 <= 0)
            {
              break;
            }
          else if (recvLen2 == msg_length - recvLen)
            {
              recvLen = 0;
              recv_count++;
              if (msg_length != send (msgFd, send_buf, msg_length, 0))
                {
                  printf ("send failed!====, need exit\n");
                }

            }
          else if (recvLen2 < msg_length - recvLen)
            {
              recvLen += recvLen2;
            }
          if ((flagPrint != 0) && ((recv_count % unitPrint) == 0))
            {
              clock_gettime (CLOCK_MONOTONIC, &recvEnd);
              recv_ppstime =
                (recvEnd.tv_sec - recvStart.tv_sec) * 1000000000 +
                recvEnd.tv_nsec - recvStart.tv_nsec;
              recv_pps = ((float) 1000000000 / recv_ppstime) * unitPrint;
              printf ("receive count:%d, receive time: %ld ns\n", recv_count,
                      recv_ppstime);
              printf ("receive pps = %d\n", recv_pps);
              clock_gettime (CLOCK_MONOTONIC, &recvStart);
            }
        }
    }
  close (msgFd);
}

/*
   using this thread to do accept connect
*/
void *
process_server_accept_thread (void *pArgv)
{
  int listenFd = 0;
  int x, optval, ret, m = 0;
  int acpt_socketfd[1000] = { 0 };

  listenFd = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (0 > listenFd)
    {
      printf ("ERROR:socket failed,errno [%d]\n", errno);
      return;
    }
  else
    {
      printf ("INFO:Create listen socket Success, listenFd[%d]\n", listenFd);
    }

  if (0 > fcntl (listenFd, F_SETFL, O_NONBLOCK))
    {
      printf ("ERROR:fcntl failed. fd[%d], errno[%d]/n", listenFd, errno);
      close (listenFd);
    }

  optval = 1;
  ret =
    setsockopt (listenFd, SOL_SOCKET, SO_REUSEADDR, (void *) &optval,
                sizeof (optval));
  if (ret == -1)
    {
      printf ("ERROR:setsockopt failed. fd[%d], errno[%d]\n", listenFd,
              errno);
      close (listenFd);
      return;
    }

  if (0 !=
      bind (listenFd, (struct sockaddr *) &g_recv, sizeof (struct sockaddr)))
    {
      printf ("ERROR:bind failed. fd[%d] errno [%d]\n", listenFd, errno);
      printf ("Info Bind Success, port %d IP:%s\n", ntohs (g_recv.sin_port),
              inet_ntoa (*((struct in_addr *) &(g_recv.sin_addr.s_addr))));
      close (listenFd);
      return;
    }
  else
    {
      printf ("Info Bind Success, port %d IP:%s\n", ntohs (g_recv.sin_port),
              inet_ntoa (*((struct in_addr *) &(g_recv.sin_addr.s_addr))));
    }

  if (0 != listen (listenFd, 100))
    {
      printf ("server socket listen failed. err %d\n", errno);
      close (listenFd);
      return;
    }
  printf ("Listen Success\n");

  int timeout = -1;
  int accpedNum = 0;
  while (accpedNum < connectNum)
    {
      while (1)
        {
          acpt_socketfd[accpedNum] =
            accept4 (listenFd, NULL, NULL, SOCK_NONBLOCK);
          if (acpt_socketfd[accpedNum] < 0)
            {
              break;
            }

          fcntl (acpt_socketfd[accpedNum], F_SETFL, 0);

          /*add new accptFd to MsgEpFD */
          pthread_t ser_rcv_thread_id;
          if (pthread_create
              (&ser_rcv_thread_id, NULL, process_server_msg_thread,
               (void *) &acpt_socketfd[accpedNum]) == -1)
            {
              printf ("create process_server_msg_thread fail\n");
              break;
            }

          printf ("accept cnt [%d], cur accept fd [%d]\n", accpedNum,
                  acpt_socketfd[accpedNum]);
          accpedNum++;
        }
    }
  close (listenFd);
  while (1)
    {
      sleep (10);
    }

  pthread_exit (NULL);
}

void
main (int argc, char *argv[])
{
  socklen_t addrlen = sizeof (struct sockaddr);
  int err = 0, result = 0, ret = 0;
  int i = 0, j = 0, optval = 0, z = 0, x, listenfd;
  cpu_set_t mask;

  setArgsDefault ();
  ret = process_arg (argc, argv);
  if (ret != 1)
    {
      printf ("The param error\n");
      return;
    }

  pthread_t server_thread_id;

  if (pthread_create
      (&server_thread_id, NULL, process_server_accept_thread, NULL) == -1)
    {
      printf ("create process_server_accept_thread fail\n");
      return;
    }

  printf ("create process_server_accept_thread success\n");

  if (server_thread_id != 0)
    {
      printf ("Server Thread join\n");
      pthread_join (server_thread_id, NULL);
    }

  while (1)
    {
      sleep (10);
    }

  return;
}
