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
//#include "core.h"

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
    "./a.out [enter|exit|destroy]\n"
    "./a.out init <address> <port> [<uplink_addr> <uplink_port>]\n"
    "    NOTE: address should not be 0.0.0.0\n");
  exit (1);
}

int main (int argc, char *argv[]) {
  int error = 0, syscalln;

  if (argc < 2)
    print_error ();

  if (strcmp ("enter", argv[1]) == 0) {
    syscalln = syscall_num ("enter_critical");
    error = syscalln == -1 ? -1 : syscall (syscalln);
  } else if (strcmp ("exit", argv[1]) == 0) {
    syscalln = syscall_num ("exit_critical");
    error = syscalln == -1 ? -1 : syscall (syscalln);
  } else if (strcmp ("destroy", argv[1]) == 0) {
    syscalln = syscall_num ("destroy_site");
    error = syscalln == -1 ? -1 : syscall (syscalln);
  } else if (strcmp ("init", argv[1]) == 0) {
    syscalln = syscall_num ("init_site");
    if (argc != 4 && argc != 6)
      print_error ();
    else if (syscalln > 0) {
      struct sockaddr_in serv_addr, uplink_addr;

      bzero ((char *) &serv_addr, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      if (inet_aton (argv[2], &serv_addr.sin_addr) == 0)
        print_error ();
      serv_addr.sin_len = sizeof(struct sockaddr_in);
      serv_addr.sin_port = htons (atoi (argv[3]));
      if (argc == 4) {
        error = syscall (syscalln, &serv_addr, NULL);
      } else {
        bzero ((char *) &uplink_addr, sizeof(uplink_addr));
        uplink_addr.sin_family = AF_INET;
        uplink_addr.sin_len = sizeof(struct sockaddr_in);
        if (inet_aton (argv[4], &uplink_addr.sin_addr) == 0)
          print_error ();
        uplink_addr.sin_port = htons (atoi (argv[5]));

        error = syscall (syscalln, &serv_addr, &uplink_addr);
      }
    }
  } else {
    print_error ();
  }

  if (error)
    printf ("ERROR in syscall returned:%d [%d: %s]\n", error, errno, strerror (errno));
  return error;
}
