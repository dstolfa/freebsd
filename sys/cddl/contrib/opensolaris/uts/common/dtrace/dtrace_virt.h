/*-
 * Copyright (c) 2017 Domagoj Stolfa
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
 *
 * $FreeBSD$
 */

#include <sys/dtrace.h>

typedef struct dtrace_virt_probes {
	char		(*arg_types)[DTRACE_ARGTYPELEN];
	dtrace_id_t	id;
	int		enabled;
} dtrace_virt_probes_t;

/*
 * XXX(dstolfa): It's still questionable whether this should be a tree, a hash
 * table or simply an array. For the sake of simplicity, keep it an array for
 * now
 */
typedef struct dtrace_virt_params {
	dtrace_virt_probe **probe_array;
} dtrace_virt_params_t;

extern void	(*dtvirt_hook_commit)(const char *, dtrace_id_t,
           	    uintptr_t, uintptr_t, uintptr_t,
		    uintptr_t, uintptr_t);

/*
 * These operations need to be handled with great care. They are meant as a
 * generic set of operations for a DTvirt "provider generator".
 */
extern void dtps_virt_provide(void *, dtrace_probedesc_t *);
extern void dtps_virt_provide_module(void *, modctl_t *);
extern void dtps_virt_enable(void *, dtrace_id_t, void *);
extern void dtps_virt_disable(void *, dtrace_id_t, void *);
extern void dtps_virt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
extern void dtps_virt_destroy(void *, dtrace_id_t, void *);

/*
 * FIXME: This should not be here, but including sys/dtrace.h in this file would
 * cause a circular include when including the dtps_virt_* operations back into
 * dtrace.h
 */
dtrace_pops_t provider_ops[] = {
	[DTRACE_PROV_PURPOSE_NONE] = {
		.dtps_provide		= NULL,
		.dtps_provide_module	= NULL,
		.dtps_enable		= NULL,
		.dtps_disable		= NULL,
		.dtps_suspend		= NULL,
		.dtps_resume		= NULL,
		.dtps_getargdesc	= NULL,
		.dtps_getargval		= NULL,
		.dtps_usermode		= NULL,
		.dtps_destroy		= NULL
	}

	[DTRACE_PROV_PURPOSE_VIRT] = {
		.dtps_provide		= dtps_virt_provide,
		.dtps_provide_module	= NULL,
		.dtps_enable		= dtps_virt_enable,
		.dtps_disable		= dtps_virt_disable,
		.dtps_suspend		= NULL,
		.dtps_resume		= NULL,
		.dtps_getargdesc	= dtps_virt_getargdesc,
		.dtps_getargval		= NULL,
		.dtps_usermode		= NULL,
		.dtps_destroy		= dtps_virt_destroy
	}
}
