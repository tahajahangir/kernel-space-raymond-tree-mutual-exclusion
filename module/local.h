#ifndef	_MY_LOCAL_H
#define	_MY_LOCAL_H	1

#include <sys/param.h>
#include <sys/proc.h> // struct thread

int kern_enter_critical(struct thread *td);
int kern_exit_critical(struct thread *td);

#endif /* my/local.h */
