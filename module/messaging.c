#include <sys/param.h> // NULL
// kld
#include <sys/socket.h> // struct sockaddr
#include <netinet/in.h> // many socket related defs (like sockaddr_in)
#include <sys/un.h> //sockaddr_un
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/systm.h> // many necessary funcs: printf, KASSERT, ...
#include "messaging.h"

#include "my_kern.h"
#include "core.h"
#include "log.h"

static struct thread *accept_kthread;

void deliverMessage(struct sockaddr *site, char* message);
void accept_loop(void* data);

struct thread *current_kthread(void) {
	return accept_kthread;
}
/**
 * Returns: 0 on success, a positive value on error
 *
 *		EACCES (from socreate)
 *		EPERM (from socreate)
 *		EADDRNOTAVAIL (from connect)
 *		ETIMEDOUT (from connect)
 *		...
 *
 *  Rare errors:
 *		EPROTONOSUPPORT (from socreate)
 *		EPROTOTYPE (from socreate)
 *		ENOBUFS (from socreate)
 *		EALREADY (from connect)
 *
 */
int sendMessageToSite(struct sockaddr *site, char *message,
		struct sockaddr *sender, struct thread *td) {
	int error, wrote;
	struct socket *so = NULL;

	if (site->sa_family == AF_INET) {
		struct sockaddr_in* site_in = (struct sockaddr_in*) site;
		log_debug("Sending message %s to %s:%d", message,
				inet_ntoa(site_in->sin_addr), ntohs(site_in->sin_port));
	} else {
		struct sockaddr_un* site_un = (struct sockaddr_un*) site;
		log_debug("Sending message %s to %s", message, site_un->sun_path);
	}

	error = socreate(site->sa_family, &so, SOCK_STREAM, 0, td->td_ucred, td);
	if (error) {
		log_warn("error in socreate in sendMessageToSite");
		goto bad;
	}
	error = my_kern_connect(so, (struct sockaddr *) site, td);
	if (error) {
		log_warn("error in my_kern_connect in sendMessageToSite");
		goto bad;
	}
	error = my_kern_send(so, message, strlen(message), &wrote, td);
	if (error || wrote != strlen(message)) {
		error = error || EPIPE;
		log_warn("error in my_kern_send in sendMessageToSite(1)");
		goto bad;
	}
	error = my_kern_send(so, sender, sizeof(struct sockaddr), &wrote, td);
	if (error || wrote != sizeof(struct sockaddr)) {
		error = error || EPIPE;
		log_warn("error in my_kern_send in sendMessageToSite(2)");
		goto bad;
	}

	bad: //on error
	if (error)
		log_error("Error in sendMessageToSite [%d]", error);
	if (so != NULL)
		soclose(so);
	return error;
}

void accept_loop(void* thread_args) {
	struct socket *so = (struct socket*) thread_args;
	struct socket* rsock;
	struct sockaddr sender_addr;
	int error, read;
	char msg[3];

	log_debug("Starting accept_loop");
	for (;;) {
		error = my_kern_accept(so, &rsock);
		if (error) {
			log_warn("error in my_kern_accept in accept_loop: %d", error);
			if (error == ECONNABORTED)
				goto completed;
			goto continue_listen;
		}
		log_debug("A connection accepted in accept_loop");
		bzero((char*) msg, sizeof(msg));

		//read the message 't' or 'T' or 'r'
		error = my_kern_recv(rsock, msg, 1, &read, NULL);
		if (error || read != 1) {
			log_warn("error in my_kern_recv in accept_loop(1): %d", error);
			goto continue_listen;
		}
		// exit if exit_message received
		if (strcmp(msg, MESSAGE_EXIT) == 0) {
			log_info("Received exit message: exiting");
			goto completed;
		}
		// read sender address
		error = my_kern_recv(rsock, &sender_addr, sizeof(struct sockaddr),
				&read, NULL);
		if (error || read != sizeof(struct sockaddr)) {
			log_warn("error in my_kern_recv in accept_loop(2): %d", error);
			goto continue_listen;
		}
		// process delivered message
		deliverMessage(&sender_addr, msg);

		continue_listen: //
		if (rsock != NULL) {
			soclose(rsock);
			rsock = NULL;
		}
	}
	completed: // on error
	if (error)
		log_error("Error in accept_loop [%d]\n", error);
	if (so != NULL)
		soclose(so);
	if (rsock != NULL)
		soclose(rsock);
	log_info("accept_loop exit");
	kthread_exit();
}

/**
 * Forks a listening thread
 * Returns 0 on success, a positive errno value on error,
 */
struct sockaddr_in waiting_sockaddr; // it seems we should make a copy!
int waitForMessages(struct sockaddr_in *site_2, struct thread *td) {
	waiting_sockaddr = *site_2;
	struct sockaddr_in *site = &waiting_sockaddr;
	log_info("waiting for messages on %s:%d", inet_ntoa(site->sin_addr), ntohs(site->sin_port));

	int error = 0;
	struct socket *so = NULL;

	error = socreate(AF_INET, &so, SOCK_STREAM, 0, td->td_ucred, td);
	if (error) {
		log_warn("error in socreate in waitForMessages");
		goto bad;
	}
	error = sobind(so, (struct sockaddr *) site, td);
	if (error) {
		log_warn("error in sobind in waitForMessages");
		goto bad;
	}
	error = solisten(so, 5, td);
	if (error) {
		log_warn("error in solisten in waitForMessages");
		goto bad;
	}

	error = kthread_add(accept_loop, so, NULL, &accept_kthread, 0, 0,
			"raymond_accept_loop");
	if (error) {
		log_warn("error creating thread: %d\n", error);
		goto bad;
	}
	return 0;

	bad: // on error
	if (so != NULL)
		soclose(so);
	return error;
}

/* This function doesn't save input pointers, input pointers can be
 * 	invalidated after function returns
 */
void deliverMessage(struct sockaddr *site, char* message) {
	if (site->sa_family == AF_INET) {
		struct sockaddr_in* site_in = (struct sockaddr_in*) site;
		log_debug("Received message '%s' from %s:%d", message, inet_ntoa(site_in->sin_addr),
				ntohs(site_in->sin_port));
	} else {
		struct sockaddr_un* site_un = (struct sockaddr_un*) site;
		log_debug("Received message '%s' from %s", message, site_un->sun_path);
	}

	if (strcmp(message, MESSAGE_TOKEN) == 0) {
		deliverToken();
	} else if (strcmp(message, MESSAGE_REQUEST) == 0) {
		deliverRequest(site);
	} else if (strcmp(message, MESSAGE_TOKEN_AND_REQUEST) == 0) {
		deliverRequest(site);
		deliverToken();
	} else {
		log_error("Bad message");
	}
}
