//#include <sys/cdefs.h> // kernel mode
//#include <sys/unistd.h> // included by others
#include <sys/param.h> // NULL
#include <netinet/in.h> // many socket related defs (like sockaddr_in)
#include <sys/socket.h> // struct sockaddr
#include <sys/socketvar.h> // many socket related defs (like sockaddr_in)
#include <sys/kernel.h>
#include <sys/systm.h> // many necessary funcs: printf, KASSERT, ...
#include "messaging.h"
#include "core.h"
#include "log.h"

#define SITE_QUEUE_MAX 30

#define STATE_NOT_INITIALIZED 0
#define STATE_HOLDER 1
#define STATE_REQUESTED 2
#define STATE_PROCESSING 3
#define STATE_PROCESS_WAITING 4
#define STATE_READY 5

int req_q_len = 0, proc_q_len = 0;
struct sockaddr req_q[SITE_QUEUE_MAX];
struct sockaddr proc_q[SITE_QUEUE_MAX];
struct sockaddr_in uplink, thisSite;

int state = STATE_NOT_INITIALIZED;

int _doProcessingState(void);
void _doHolderState(void);
int _isSiteLocal(const struct sockaddr *cur_site);

// Returns NULL if site is not initialized
struct sockaddr_in* current_addr(void) {
	if (state == STATE_NOT_INITIALIZED)
		return NULL;
	return &thisSite;
}

// check if is a site a local site or not
int _isSiteLocal(const struct sockaddr *cur_site) {
	return cur_site->sa_family == AF_UNIX;
}

/**
 * Do tasks (send token to requested sites) when entering holder state
 *
 * This function doesn't return if there is a site in request queue
 * 		and the system fails to send token to it
 */
void _doHolderState(void) {
	int i, error;
	log_debug("doHolderState");

	KASSERT(state == STATE_HOLDER, ("Bad state"));
	KASSERT(proc_q_len == 0, ("Process queue not empty"));

	// while there is a request in queue (may be because of error, or
	//		delivery of new request during processing
	do {
		error = 0;
		if (req_q_len) {
			// because proc_q is stack-like, reverse the proc_q
			for (i = 0; i < req_q_len; i++) {
				proc_q[i] = req_q[req_q_len - i - 1];
			}
			proc_q_len = req_q_len;
			req_q_len = 0;
			state = STATE_PROCESSING;
			error = _doProcessingState();
		}
		if (error)
			pause("wait", 100);
	} while (error);
}
// a bad scenario: can not send token to any of reqeusted sites!
/**
 * If sending a message to a site failed, pops it from process queue
 * 		and inserts it to request queue
 * Returns 0 if token successfully sent to a site,
 * 		negative value when sending token to all sites failed! in this case
 * 		it's guaranteed that process queue is empty and request queue may be not empty
 */
int _doProcessingState(void) {
	log_debug("doProcessingState");
	int error, new_state;

	// when entering processing, proc queue must be not empty
	KASSERT(proc_q_len, ("Proccess queue in empty!"));
	KASSERT(state == STATE_PROCESSING, ("Bad state"));

	// proc_q is stack-like, last-in first-out
	struct sockaddr *site;
	do {
		site = &proc_q[--proc_q_len];
		if (proc_q_len) {
			new_state = STATE_PROCESS_WAITING;
			error = sendMessageToSite(site, MESSAGE_TOKEN_AND_REQUEST,
					(struct sockaddr *) &thisSite, current_kthread());
		} else if (req_q_len || _isSiteLocal(site)) {
			// send T to site
			new_state = STATE_REQUESTED;
			error = sendMessageToSite(site, MESSAGE_TOKEN_AND_REQUEST,
					(struct sockaddr *) &thisSite, current_kthread());
		} else {
			new_state = STATE_READY;
			error = sendMessageToSite(site, MESSAGE_TOKEN,
					(struct sockaddr *) &thisSite, current_kthread());
			if (error == 0)
				uplink = *(struct sockaddr_in *) site;// site is not local

		}
		/* TEMP: dont attempt!
		 if (error) {
		 //error occured, insert the site to request queue
		 req_q[req_q_len++] = *site;
		 }*/
	} while (error && proc_q_len);

	if (error)
		state = STATE_HOLDER;
	else
		state = new_state;
	return error;
}
// a bad scenario: we get a request, but sending request to upstream fails!
/*
 * event handler when a request delivered from a site
 */
void deliverRequest(struct sockaddr *site) {
	log_debug("request delivered");

	// add it to request queue
	// if state is not ready or holder this is enough
	req_q[req_q_len++] = *site;

	if (state == STATE_READY) {
		//send request to upstream
		state = STATE_REQUESTED;
		int error, retries = 0;
		do {
			log_info("Sending request to uplink %s:%d", inet_ntoa(uplink.sin_addr), ntohs(uplink.sin_port));
			error = sendMessageToSite((struct sockaddr *) &uplink,
					MESSAGE_REQUEST, (struct sockaddr *) &thisSite,
					current_kthread());
			if (error)
				pause("wait", 100);
		} while (error && retries++ < 10);
		if (error) // can not send request to upper level, returning to READY
			state = STATE_READY;
	} else if (state == STATE_HOLDER) {
		// we have token, so we prosess the request
		_doHolderState();
	}
}

/**
 * event handler when token delivered to this site
 */
void deliverToken(void) {
	log_debug("token delivered");
	if (state == STATE_REQUESTED || state == STATE_READY) {
		// handler request queue
		state = STATE_HOLDER;
		_doHolderState();
	} else if (state == STATE_PROCESS_WAITING) {
		// continue processing the proc queue
		state = STATE_PROCESSING;
		int error = _doProcessingState();
		// if it fails, we are holder of the token now!
		if (error) {
			state = STATE_HOLDER;
			_doHolderState();
		}
	} else {
		log_error("Getting token in bad state: %d", state);
	}
}

/**
 * Initialize a site, with an address to listen and uplink site
 * If uplink set to NULL, the site will be holder of token
 *
 * Returns 0 on success, errno on failure
 * EALREADY - if already initialized
 */
int kern_init_site(struct sockaddr_in *cur_site,
		struct sockaddr_in *uplink_site, struct thread *td) {
	if (state != STATE_NOT_INITIALIZED)
		return EALREADY;

	thisSite = *cur_site;
	if (uplink_site == NULL) {
		state = STATE_HOLDER;
		log_debug("Initialize site %s:%d as a holder", inet_ntoa(thisSite.sin_addr), ntohs(thisSite.sin_port));
	} else {
		state = STATE_READY;
		uplink = *uplink_site;
		log_debug("Initialize site %s:%d ...", inet_ntoa(thisSite.sin_addr),
				ntohs(thisSite.sin_port));
		log_debug ("     with holder %s:%d", inet_ntoa(uplink.sin_addr), ntohs(uplink.sin_port));
	}

	int error = waitForMessages(&thisSite, td);
	if (error) {
		state = STATE_NOT_INITIALIZED;
		log_error ("Error initiating site %s:%d", inet_ntoa(thisSite.sin_addr), ntohs(thisSite.sin_port));
		return error;
	}
	return 0;
}

int kern_destroy_site(struct thread *td) {
	if (state == STATE_NOT_INITIALIZED)
		return EALREADY;

	int error = sendMessageToSite((struct sockaddr*) current_addr(),
			MESSAGE_EXIT, (struct sockaddr*) current_addr(), td);
	if (!error)
		state = STATE_NOT_INITIALIZED;
	return error;
}
