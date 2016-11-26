/*-
 * Copyright (c) 2016 Domagoj Stolfa
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

#ifndef _MACHINE_HYPERCALL_H_
#define _MACHINE_HYPERCALL_H_

#include <sys/linker_set.h>

#define	HYPERCALL_DTRACE_PROBE_CREATE	0
#define	HYPERCALL_DTRACE_PROBE		1
#define	HYPERCALL_DTRACE_RESERVED1	2
#define	HYPERCALL_DTRACE_RESERVED2	3
#define	HYPERCALL_DTRACE_RESERVED3	4
#define	HYPERCALL_DTRACE_RESERVED4	5
#define	HYPERCALL_INDEX_MAX		6

#define	HYPERCALL_RET_ERROR		-1
#define	HYPERCALL_RET_NOT_IMPL		-2
#define	HYPERCALL_RET_SUCCESS		 0

#ifndef __asm__

int hypercall_dtrace_probe_create(uintptr_t id, const char *mod, const char *func,
    const char *name, int aframes, void *arg);
int hypercall_dtrace_probe(int probe, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,
    uintptr_t arg3, uintptr_t arg4);

#endif

#endif
