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

typedef struct dtrace_virt_probe {
	char		(*dtv_argtypes)[DTRACE_ARGTYPELEN];
	size_t		*dtv_argsizes;
	char		dtv_vm[DTRACE_INSTANCENAMELEN];
	dtrace_id_t	dtv_id;
	uint8_t		dtv_enabled;
	uint8_t		dtv_nargs;
} dtrace_virt_probe_t;

extern void	(*dtvirt_hook_commit)(const char *, dtrace_id_t,
           	    uintptr_t, uintptr_t, uintptr_t,
		    uintptr_t, uintptr_t);
extern int	(*dtvirt_hook_register)(const char *, const char *,
          	    struct uuid *, dtrace_pattr_t *, uint32_t, dtrace_pops_t *);
extern int	(*dtvirt_hook_unregister)(struct uuid *);
extern int	(*dtvirt_hook_create)(struct uuid *, const char *, const char *,
		const char *, char *, size_t *, uint8_t);
extern void	(*dtvirt_hook_enable)(void *, dtrace_id_t, void *);
extern void	(*dtvirt_hook_disable)(void *, dtrace_id_t, void *);
extern void	(*dtvirt_hook_getargdesc)(void *, dtrace_id_t,
           	    void *, dtrace_argdesc_t *);
extern uint64_t	(*dtvirt_hook_getargval)(void *, dtrace_id_t,
           	    void *, uint64_t, int);
extern void	(*dtvirt_hook_destroy)(void *, dtrace_id_t, void *);

/*
 * These operations need to be handled with great care. They are meant as a
 * generic set of operations for a DTvirt "provider generator".
 */
extern void dtps_virt_provide(void *, dtrace_probedesc_t *);
extern void dtps_virt_enable(void *, dtrace_id_t, void *);
extern void dtps_virt_disable(void *, dtrace_id_t, void *);
extern void dtps_virt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
extern void dtps_virt_destroy(void *, dtrace_id_t, void *);

