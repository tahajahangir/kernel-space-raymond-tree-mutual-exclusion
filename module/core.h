#ifndef	_MY_CORE_H
#define	_MY_CORE_H	1

#include <sys/param.h>
#include <netinet/in.h> // many socket related defs (like sockaddr_in)
#include <sys/proc.h> // struct thread

void deliverRequest(struct sockaddr *site);
void deliverToken(void);
int kern_init_site(struct sockaddr_in *self, struct sockaddr_in *uplink,
		struct thread *td);
int kern_destroy_site(struct thread *td);

struct sockaddr_in *current_addr(void);

#endif /* my/core.h */
