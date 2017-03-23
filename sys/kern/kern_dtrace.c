/*-
 * Copyright (c) 2007-2008 John Birrell <jb@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/dtrace_bsd.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/event.h>
#include <sys/uio.h>

#define KDTRACE_PROC_SIZE	64
#define	KDTRACE_THREAD_SIZE	256

FEATURE(kdtrace_hooks,
    "Kernel DTrace hooks which are required to load DTrace kernel modules");

static MALLOC_DEFINE(M_KDTRACE, "kdtrace", "DTrace hooks");

/* Hooks used in the machine-dependent trap handlers. */
dtrace_trap_func_t		dtrace_trap_func;
dtrace_doubletrap_func_t	dtrace_doubletrap_func;
dtrace_pid_probe_ptr_t		dtrace_pid_probe_ptr;
dtrace_return_probe_ptr_t	dtrace_return_probe_ptr;
dtrace_install_probe_ptr_t	dtrace_install_probe_ptr;
dtrace_uninstall_probe_ptr_t	dtrace_uninstall_probe_ptr;

systrace_probe_func_t		systrace_probe_func;

static int filt_dtraceattach(struct knote *);
static void filt_dtracedetach(struct knote *);
static int filt_dtrace(struct knote *, long);
static void filt_dtracetouch(struct knote *, struct kevent *, u_long);

struct knlist dtrace_knlist;
struct mtx dtrace_knlist_mtx;

struct filterops dtrace_filtops = {
	.f_attach = filt_dtraceattach,
	.f_detach = filt_dtracedetach,
	.f_event = filt_dtrace,
};

/* Return the DTrace process data size compiled in the kernel hooks. */
size_t
kdtrace_proc_size()
{

	return (KDTRACE_PROC_SIZE);
}

static void
kdtrace_proc_ctor(void *arg __unused, struct proc *p)
{

	p->p_dtrace = malloc(KDTRACE_PROC_SIZE, M_KDTRACE, M_WAITOK|M_ZERO);
}

static void
kdtrace_proc_dtor(void *arg __unused, struct proc *p)
{

	if (p->p_dtrace != NULL) {
		free(p->p_dtrace, M_KDTRACE);
		p->p_dtrace = NULL;
	}
}

/* Return the DTrace thread data size compiled in the kernel hooks. */
size_t
kdtrace_thread_size()
{

	return (KDTRACE_THREAD_SIZE);
}

static void
kdtrace_thread_ctor(void *arg __unused, struct thread *td)
{

	td->td_dtrace = malloc(KDTRACE_THREAD_SIZE, M_KDTRACE, M_WAITOK|M_ZERO);
}

static void
kdtrace_thread_dtor(void *arg __unused, struct thread *td)
{

	if (td->td_dtrace != NULL) {
		free(td->td_dtrace, M_KDTRACE);
		td->td_dtrace = NULL;
	}
}

/*
 *  Initialise the kernel DTrace hooks.
 */
static void
init_dtrace(void *dummy __unused)
{

	EVENTHANDLER_REGISTER(process_ctor, kdtrace_proc_ctor, NULL,
	    EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(process_dtor, kdtrace_proc_dtor, NULL,
	    EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(thread_ctor, kdtrace_thread_ctor, NULL,
	    EVENTHANDLER_PRI_ANY);
	EVENTHANDLER_REGISTER(thread_dtor, kdtrace_thread_dtor, NULL,
	    EVENTHANDLER_PRI_ANY);
	mtx_init(&dtrace_knlist_mtx, "dtknlmtx", NULL, MTX_DEF);
	knlist_init_mtx(&dtrace_knlist, &dtrace_knlist_mtx);
}

static void
uninit_dtrace(void *dummy __unused)
{
	mtx_destroy(&dtrace_knlist_mtx);
	knlist_destroy(&dtrace_knlist);
}

static int
filt_dtraceattach(struct knote *kn)
{
	if ((kn->kn_filter & EVFILT_DTRACE) == 0)
		return (ENXIO);

	if ((kn->kn_sfflags & (NOTE_PROBE_INSTALL | NOTE_PROBE_UNINSTALL)) == 0)
		return (EINVAL);

	kn->kn_status |= KN_KQUEUE;
	knlist_add(&dtrace_knlist, kn, 0);
	return (0);
}

static __inline void
filt_dtracedetach(struct knote *kn)
{
	knlist_remove(&dtrace_knlist, kn, 0);
}

static __inline int
filt_dtrace(struct knote *kn, long hint)
{
	return ((kn->kn_sfflags & NOTE_PROBE_INSTALL)   &&
	        (kn->kn_sfflags & NOTE_PROBE_UNINSTALL) &&
	        (kn->kn_iov != NULL));
}

static void
filt_dtracetouch(struct knote *kn, struct kevent *kev, u_long type)
{
/*
	struct uio uio;
	struct iovec iov;
	int error;

	switch (type) {
	case EVENT_REGISTER:
		kn->kn_sdata = kev->data;
		kn->kn_sfflags = kev->fflags;
		break;
	case EVENT_PROCESS:
		iov.iov_base = (void *)kn->kn_sdata;
		iov.iov_len = sizeof(struct dtrace_probeinfo);

		if (iov.iov_base == NULL ||
		    ((long)iov.iov_base) - iov.iov_len <= 0)
			return;
		if (kn->kn_data == 0)
			return;

		uio = (struct uio){
			.uio_iov = &iov,
			.uio_iovcnt = 1,
			.uio_offset = 0,
			.uio_segflg = UIO_USERSPACE,
			.uio_rw = UIO_READ,
			.uio_resid = sizeof(struct dtrace_probeinfo),
			.uio_td = kn->kn_ctxtd
		};


		error = uiomove_frombuf((void *)kn->kn_data,
		    sizeof(struct dtrace_probeinfo), &uio);

		KASSERT(error == 0, ("%s: error %d in uiomove(9)",
		    __func__, error));

		if (kn->kn_flags & EV_CLEAR) {
			kn->kn_data = 0;
			kn->kn_fflags = 0;
		}
		break;
	default:
		panic("filt_dtracetouch() - invalid type (%ld)", type);
		break;
	}
*/
}


SYSINIT(kdtrace, SI_SUB_KDTRACE, SI_ORDER_FIRST, init_dtrace, NULL);
SYSUNINIT(kdtrace, SI_SUB_KDTRACE, SI_ORDER_FIRST, uninit_dtrace, NULL);
