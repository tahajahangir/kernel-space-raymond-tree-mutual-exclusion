#ifndef	_MY_KERN_H
#define	_MY_KERN_H	1

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
// kld


int my_kern_accept(struct socket *so, struct socket **retrun_sock);
int my_kern_connect(struct socket *so, struct sockaddr *sa, struct thread *td);
int my_kern_recv(struct socket *so, void* buf, int len, int* read,
		struct thread *td);
int my_kern_send(struct socket *so, void* buf, int len, int* wrote,
		struct thread *td);

#endif // my_kern.h
