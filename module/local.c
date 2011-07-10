#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/systm.h>
#include <sys/kernel.h> // kernel mode
#include <sys/syscallsubr.h> // for kern_unlink

#include "my_kern.h"
#include "core.h"
#include "local.h"
#include "messaging.h"
#include "log.h"

/**
 * Returns 0 on success (lock acquired)
 * EPROTO - not an initialized site
 */
int kern_enter_critical(struct thread *td) {
	if (current_addr() == NULL) {
		log_warn("Site is not initialized");
		return EPROTO;
	}
	log_debug("enter critical, sending request to %s:%d", inet_ntoa(current_addr()->sin_addr), ntohs(current_addr()->sin_port));

	int error;
	struct sockaddr_un serv_addr;
	struct socket *so = NULL, *rsock = NULL;

	error = socreate(AF_UNIX, &so, SOCK_STREAM, 0, td->td_ucred, td);
	if (error) {
		log_warn("error in socreate in enter_critical");
		goto error;
	}

	// we will use only sizeof(serv_addr) bytes of sockaddr_un
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	serv_addr.sun_len = sizeof(serv_addr);

	// finding an unused socket name!
	int retries = 0;
	do {
		// maximum length of following str will be 7+6 = 13
		sprintf((char *) serv_addr.sun_path, "/tmp/ry%d", (int) (random() % 1000000));
		//assert(strlen(serv_addr.sun_path) < sizeof(struct sockaddr) - sizeof(serv_addr.sun_family));
		error = sobind(so, (struct sockaddr *) &serv_addr, td);
		if (error)
			log_debug("sobind in enter_critical: error for: %s", serv_addr.sun_path);
	} while (error && retries++ < 10);
	if (error) {
		log_warn("error in sobind in enter_critical");
		goto error;
	}

	error = solisten(so, 5, td);
	if (error) {
		log_warn("error in solisten in enter_critical");
		goto error;
	}

	error = sendMessageToSite((struct sockaddr *) current_addr(),
			MESSAGE_REQUEST, (struct sockaddr *) &serv_addr, td);
	if (error) {
		log_warn("error in sendMessageToSite in enter_critical");
		goto error;
	}

	error = my_kern_accept(so, &rsock);
	if (error) {
		log_warn("error in my_kern_accept in enter_critical");
		goto error;
	}

	// We sure that any incoming connection in our unix socket is token!
	// So there is nothing to do

	error: // on error
	if (so != NULL) {
		soclose(so);
		kern_unlink(td, (char *) serv_addr.sun_path, UIO_SYSSPACE);
	}
	if (rsock != NULL)
		soclose(rsock);

	return error;
}
/**
 * returns 0 on success, or -errno
 */
int kern_exit_critical(struct thread *td) {
	if (current_addr() == NULL) {
		log_warn("Site is not initialized");
		return EPROTO;
	}
	log_debug("exit_critical, sending release message");
	return sendMessageToSite((struct sockaddr *) current_addr(), MESSAGE_TOKEN,
			(struct sockaddr *) current_addr(), td);
}

