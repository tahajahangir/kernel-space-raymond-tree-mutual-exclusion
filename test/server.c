#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/module.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <pthread.h>

#include "test.h"

void print_error () {
  fprintf (stderr, "syntax: \n"
    "./server.out <port-number> <output-file-address>\n");
  exit (1);
}

struct socket_descriptor {
  int sfd;
};

int kill_server;

int dump_data (char *buf, int size);

void *receive_data (void *arg) {
  struct socket_descriptor *sd = (struct socket_descriptor *) arg;
  free (sd);
  int sfd = sd->sfd;
  int size;
  char *ptr;

  char buf[100];
  int len = 100;
  size = recv (sfd, buf, len, 0);
  ptr = strstr (buf, KILL_MESSAGE);
  if (ptr) {
    close (sfd);
    kill_server = 1;
    return NULL;
  }
  for (;;) {
    ptr = strstr (buf, END_MESSAGE);
    if (ptr) {
      size -= strlen (END_MESSAGE);
    }
    if (size > 0) {
      dump_data (buf, size);
    }
    if (ptr) {
      char *end = END_MESSAGE;
      int size = strlen (end);
      if (size != send (sfd, end, size, 0)) {
        printf ("Error in sending messages\n");
      }
      break;
    }
    size = recv (sfd, buf, len, 0);
  }
  close (sfd);
  return NULL;
}

FILE *file;

int main (int argc, char *argv[]) {
  if (argc != 3)
    print_error ();

  struct sockaddr_in addr, peer;

  kill_server = 0;

  file = fopen (argv[2], "w");
  if (file == NULL) {
    print_error ();
  }

  bzero ((char *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons (atoi (argv[1]));

  int sfd = socket (addr.sin_family, SOCK_STREAM, 0);
  if (sfd == -1) {
    printf ("Error in creating socket in server code\n");
    goto bad;
  }
  if (bind (sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1) {
    printf ("Error in binding socket in server code\n");
    goto bad;
  }
  if (listen (sfd, 5) == -1) {
    printf ("Error in listening on socket in server code\n");
    goto bad;
  }
  while (kill_server == 0) {
    socklen_t peer_size = sizeof(struct sockaddr_in);
    int cfd = accept (sfd, (struct sockaddr *) &peer, &peer_size);
    if (kill_server) {
      close (cfd);
      break;
    }
    if (cfd == -1) {
      printf ("Error in accepting connection\n");
      goto bad;
    }
    struct socket_descriptor *sd = malloc (sizeof(struct socket_descriptor));
    sd->sfd = cfd;
    pthread_t tid;
    pthread_create (&tid, NULL, receive_data, sd);
  }
  bad: if (sfd != -1)
    close (sfd);

  fclose (file);

  return 0;
}

int dump_data (char *buf, int size) {
  int len = fwrite (buf, sizeof(char), size, file);
  if (len != size) {
    return -1;
  }
  return 0;
}

