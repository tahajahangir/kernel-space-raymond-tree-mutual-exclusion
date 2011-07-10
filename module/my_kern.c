#include <sys/param.h>
#include <sys/un.h> //sockaddr_un
#include <sys/unistd.h> // included by others
#include <sys/socketvar.h> // many socket related defs
#include <sys/syscallsubr.h> // for kern_unlink and ACCEPT_LOCK
// kld
#include <sys/sysproto.h> // sysent struct
#include <sys/sysent.h> // sysent struct
#include <sys/kernel.h> // kernel mode
#include <sys/systm.h> // many necessary funcs: printf, bzero, ...
#include <sys/mutex.h> // ACCEPT_LOCK and ...
#include "my_kern.h"
#include "log.h"

/**
 * Returns 0 and fill return_sock pointer on success
 * Returns a non-zero if fails (return_sock may be filled!):
 * 	ECONNABORTED
 */
int my_kern_accept(struct socket *so, struct socket **retrun_sock) {
	int error;
	struct socket* rsock;
	struct sockaddr *client_addr;

	KASSERT((so->so_state & SS_NBIO) != 0, ("don't support non-blocking"));

	/* Accepting the connection */
	ACCEPT_LOCK();
	// Block, waiting for connection ...
	while (TAILQ_EMPTY(&so->so_comp) && so->so_error == 0) {
		// Check if the connection is already aborted?
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		error = msleep(&so->so_timeo, &accept_mtx, PSOCK | PCATCH, "accept", 0);
		if (error) {
			ACCEPT_UNLOCK();
			return error;
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		ACCEPT_UNLOCK();
		return error;
	}
	rsock = TAILQ_FIRST(&so->so_comp);
	SOCK_LOCK(rsock);
	soref(rsock);
	TAILQ_REMOVE(&so->so_comp, rsock, so_list);
	so->so_qlen--;
	rsock->so_qstate &= ~SQ_COMP;
	rsock->so_head = NULL;
	SOCK_UNLOCK(rsock);
	ACCEPT_UNLOCK();

	KNOTE_UNLOCKED(&so->so_rcv.sb_sel.si_note, 0);

	*retrun_sock = rsock;

	//CURVNET_SET(rsock->so_vnet)
	error = soaccept(rsock, &client_addr);
	//CURVNET_RESTORE()

	if (error) {
		log_warn("soaccept error = %d\n", error);
		return error;
	}
	return 0;
}

/**
 * Tries to read exact len bytes.
 * Stores number of read bytes int to `read`
 */
int my_kern_recv(struct socket *so, void* buf, int len, int* read,
		struct thread *td) {

	KASSERT((so->so_state & SS_NBIO) != 0, ("don't support non-blocking"));

	struct uio auio;
	struct iovec aiov;
	int flags = MSG_WAITALL;
	int error;

	aiov.iov_base = buf;
	aiov.iov_len = len;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = td;
	auio.uio_offset = 0;
	auio.uio_resid = aiov.iov_len;

	//CURVNET_SET(so->so_vnet);
	error = soreceive(so, NULL, &auio, NULL, NULL, &flags);
	//CURVNET_RESTORE();
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART || error == EINTR))
			error = 0;
	}
	if (error)
		return error;

	*read = len - auio.uio_resid;
	return 0;
}

int my_kern_send(struct socket *so, void* buf, int len, int* wrote,
		struct thread *td) {
	KASSERT((so->so_state & SS_NBIO) != 0, ("don't support non-blocking"));

	struct uio auio;
	struct iovec aiov;
	int error, flags = 0;

	aiov.iov_base = buf;
	aiov.iov_len = len;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = 0;
	auio.uio_resid = len;

	error = sosend(so, NULL, &auio, NULL, NULL, flags, td);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART || error == EINTR))
			error = 0;
		/* Generation of SIGPIPE can be controlled per socket: REMOVED! */
	}
	if (error == 0)
		*wrote = len - auio.uio_resid;
	return (error);
}

/**
 * Returns 0 and fill return_sock pointer on success
 * Returns a non-zero if fails
 */
int my_kern_connect(struct socket *so, struct sockaddr *sa, struct thread *td) {
	int err;

	KASSERT((so->so_state & SS_NBIO) != 0, ("don't support non-blocking"));

	if (so->so_state & SS_ISCONNECTING)
		return EALREADY;

	err = soconnect(so, sa, td);
	if (err)
		return err;

	SOCK_LOCK(so);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		err = msleep(&so->so_timeo, SOCK_MTX(so), PSOCK | PCATCH, "connec", 0);
		if (err) {
			if (err == EINTR || err == ERESTART) {
				so->so_state &= ~SS_ISCONNECTING;
				break;
			}
		}
	}
	if (so->so_error) {
		err = so->so_error;
		so->so_error = 0;
	}
	SOCK_UNLOCK(so);
	return err;
}

