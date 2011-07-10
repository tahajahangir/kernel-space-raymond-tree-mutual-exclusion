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

#include "test.h"

int sendMessages (struct sockaddr_in *serv_addr, int round, int messagesCount);
int kill_server (struct sockaddr_in *serv_addr);

int siteid;

/*
 * find syscall id by it's name, return -1 on error
 */
int syscall_num (char *name) {
  struct module_stat stat;
  stat.version = sizeof(stat);
  if (modstat (modfind (name), &stat))
    return -1;
  return stat.data.intval;

}

void print_error () {
  fprintf (stderr, "syntax: \n"
    "./client.out <site-id> <server-address> <server-port> <rounds#> <each-round-msgs#>\n"
    "./client.out kill <server-address> <server-port>\n");
  exit (1);
}

int main (int argc, char *argv[]) {
  int error = 0;

  if (argc != 6 && argc != 4)
    print_error ();

  struct sockaddr_in serv_addr;

  bzero ((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  if (inet_aton (argv[2], &serv_addr.sin_addr) == 0)
    print_error ();
  serv_addr.sin_port = htons (atoi (argv[3]));

  if (argc == 4) {
    if (strcmp ("kill", argv[1])) {
      print_error ();
    }
    kill_server (&serv_addr);
    return 0;
  }

  siteid = atoi (argv[1]);

  int roundsCount = atoi (argv[4]);
  int messagesCount = atoi (argv[5]);

  int enterSyscalln = syscall_num ("enter_critical");
  int exitSyscalln = syscall_num ("exit_critical");

  int i;

  for (i = 0; i < roundsCount; ++i) {
    error = enterSyscalln == -1 ? -1 : syscall (enterSyscalln);
    if (error) {
      printf ("ERROR in enter_critical syscall returned:%d [%d: %s]\n", error, errno,
          strerror (errno));
      goto bad;
    }
    printf ("Round %d:\tSending %d messages to %s:%d\n", i, messagesCount, inet_ntoa (
        serv_addr.sin_addr), ntohs (serv_addr.sin_port));

    sendMessages (&serv_addr, i, messagesCount);

    error = exitSyscalln == -1 ? -1 : syscall (exitSyscalln);
    if (error) {
      printf ("ERROR in exit_critical syscall returned:%d [%d: %s]\n", error, errno,
          strerror (errno));
      goto bad;
    }
  }

  bad:

  return error;
}

int sendMessages (struct sockaddr_in *serv_addr, int round, int messagesCount) {
  int error = 0, i = 0;
  int sfd = socket (serv_addr->sin_family, SOCK_STREAM, 0);
  if (sfd == -1) {
    printf ("Error in creating socket in client code\n");
    goto bad;
  }
  error = connect (sfd, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in));
  if (error == -1) {
    printf ("Error in connecting to server\n");
    goto bad;
  }

  char message[100];
  for (i = 0; i < messagesCount; ++i) {
    sprintf (message, "Site %d, Round %d, message %d\n", siteid, round, i);
    int size = strlen (message);
    if (size != send (sfd, message, size, 0)) {
      printf ("Error in sending messages\n");
      error = -1;
      goto bad;
    }
  }

  char *end = END_MESSAGE;
  int size = strlen (end);
  if (size != send (sfd, end, size, 0)) {
    printf ("Error in sending messages\n");
    error = -1;
    goto bad;
  }
  int len = strlen (end);
  char buf[10];
  int index = 0;
  while (len > 0) {
    size = recv (sfd, buf + index, len, 0);
    if (size == -1) {
      printf ("Error in receiving messages acknowledgment\n");
      error = -1;
      goto bad;
    }
    index += size;
    len -= size;
  }
  buf[index] = '\0';
  if (strcmp (end, buf) != 0) {
    printf ("Error in receiving messages acknowledgment\n");
    error = -1;
    goto bad;
  }
  bad: if (sfd)
    close (sfd);
  return error;
}

int kill_server (struct sockaddr_in *serv_addr) {
  int error = 0;
  int sfd = socket (serv_addr->sin_family, SOCK_STREAM, 0);
  if (sfd == -1) {
    printf ("Error in creating socket in client code\n");
    goto bad;
  }
  error = connect (sfd, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in));
  if (error == -1) {
    printf ("Error in connecting to server\n");
    goto bad;
  }
  char *kill_msg = KILL_MESSAGE;
  int size = strlen (kill_msg);
  if (size != send (sfd, kill_msg, size, 0)) {
    printf ("Error in sending kill message\n");
    error = -1;
    goto bad;
  }
  bad: if (sfd)
    close (sfd);
  if (error == 0) {
    sleep (1);
    int sfd = socket (serv_addr->sin_family, SOCK_STREAM, 0);
    if (sfd != -1) {
      error = connect (sfd, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in));
      close (sfd);
    }
  }
  return error;
}
