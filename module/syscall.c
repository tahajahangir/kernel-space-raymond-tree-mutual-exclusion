#include <sys/types.h> // socket
#include <sys/un.h> //sockaddr_un
#include <sys/unistd.h> // included by others
#include <netinet/in.h> //inet_ntoa, sockaddr_in
#include <sys/socketvar.h> // many socket related defs (like sockaddr_in)
#include <sys/syscallsubr.h> // !!
#include <sys/proc.h>
// kld
#include <sys/sysproto.h> // sysent struct
#include <sys/sysent.h> // sysent struct
#include <sys/kernel.h> // kernel mode
#include <sys/systm.h> // many necessary funcs: printf, bzero, ...
#include <sys/kthread.h> // kproc_create

#include "core.h"
#include "local.h"
#include "log.h"

// ========= exit_critical syscall =================================
static int exit_critical(struct thread *td, void *syscall_args) {
	log_debug(""); //print an empty line
	int error = kern_exit_critical(td);
	if (error)
		log_error("kern_exit_critical returned %d", error);
	else
		log_info("Successfully exited critical");
	return error;
}

// The `sysent' for the syscall
static struct sysent exit_critical_sysent = { 0, exit_critical };
//he offset in sysent where the syscall is allocated.
static int offset_exit_critical = NO_SYSCALL;
SYSCALL_MODULE(exit_critical, &offset_exit_critical, &exit_critical_sysent, NULL, NULL);

// ========= enter_critical syscall =================================
static int enter_critical(struct thread *td, void *syscall_args) {
	log_debug(""); //print an empty line
	int error = kern_enter_critical(td);
	if (error)
		log_error("kern_enter_critical returned %d", error);
	else
		log_info("Successfully entered critical");
	return error;
}

// The `sysent' for the syscall
static struct sysent enter_critical_sysent = { 0, enter_critical };
//he offset in sysent where the syscall is allocated.
static int offset_enter_critical = NO_SYSCALL;
SYSCALL_MODULE(enter_critical, &offset_enter_critical, &enter_critical_sysent, NULL, NULL);

// ========= init_site syscall =================================
struct init_site_args {
	struct sockaddr_in *serv_addr;
	struct sockaddr_in *uplink;
};

static int init_site(struct thread *td, void *syscall_args) {
	log_debug(""); //print an empty line
	struct init_site_args *args = (struct init_site_args *) syscall_args;

	return kern_init_site(args->serv_addr, args->uplink, td);
}

// The `sysent' for the syscall
static struct sysent init_site_sysent = { 2, init_site };
//he offset in sysent where the syscall is allocated.
static int offset_init_site = NO_SYSCALL;
SYSCALL_MODULE(init_site, &offset_init_site, &init_site_sysent, NULL, NULL);

// ========= destroy_site syscall =================================
static int destroy_site(struct thread *td, void *syscall_args) {
	log_debug(""); //print an empty line
	return kern_destroy_site(td);
}

// The `sysent' for the syscall
static struct sysent destroy_site_sysent = { 0, destroy_site };
//he offset in sysent where the syscall is allocated.
static int offset_destroy_site = NO_SYSCALL;
SYSCALL_MODULE(destroy_site, &offset_destroy_site, &destroy_site_sysent, NULL, NULL);
