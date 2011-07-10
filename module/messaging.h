#ifndef	_MY_MESSAGING_H
#define	_MY_MESSAGING_H	1

#include <sys/param.h>
#include <netinet/in.h> // many socket related defs (like sockaddr_in)
#include <sys/proc.h> // struct thread

#define MESSAGE_TOKEN "t"
#define MESSAGE_REQUEST "r"
#define MESSAGE_TOKEN_AND_REQUEST "T"
#define MESSAGE_EXIT "x"

/*
 *  messaging submodule, this submodule highly depends on user/kernel mode
 */
struct thread *current_kthread(void);
int sendMessageToSite(struct sockaddr *site, char *message,
		struct sockaddr *sender, struct thread *td);
int waitForMessages(struct sockaddr_in *site, struct thread *td);

#endif /* my/messaging.h */
