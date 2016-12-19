/*-
 * Copyright (c) 2016 Domagoj Stolfa <domagoj.stolfa@gmail.com>

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


#include <machine/asmacros.h>
#include <machine/bhyve_hypercall.h>

#define VENTER push %rbp ; mov %rsp, %rbp
#define VLEAVE pop %rbp

.globl __start_set_hypercall
.globl __stop_set_hypercall

/*
 * The hypercalls implemented here are only the ones
 * that are called from the kernel context. The
 * hypercalls that are intended to be called from
 * userland are expected to be implemented from each
 * program individually or within a library, as
 * opposed to here.
 */

ENTRY(hypercall_prototype)
	VENTER
	movq $HYPERCALL_PROTOTYPE, %rax
hc_prototype:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_prototype)

ENTRY(hypercall_dtrace_probe_create)
	VENTER
	movq $HYPERCALL_DTRACE_PROBE_CREATE, %rax
hc_dtrace_probe_create:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtrace_probe_create)

ENTRY(hypercall_dtrace_probe)
	VENTER
	movq $HYPERCALL_DTRACE_PROBE, %rax
hc_dtrace_probe:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtrace_probe)

ENTRY(hypercall_dtrace_register)
	VENTER
	movq $HYPERCALL_DTRACE_REGISTER, %rax
hc_dtrace_register:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtrace_register)

ENTRY(hypercall_dtrace_unregister)
	VENTER
	movq $HYPERCALL_DTRACE_UNREGISTER, %rax
hc_dtrace_unregister:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtrace_unregister)

ENTRY(hypercall_dtps_getargval)
	VENTER
	movq $HYPERCALL_DTPS_GETARGVAL, %rax
hc_dtps_getargval:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtps_getargval)

ENTRY(hypercall_dtps_getargdesc)
	VENTER
	movq $HYPERCALL_DTPS_GETARGDESC, %rax
hc_dtps_getargdesc:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtps_getargdesc)

.type __set_hypercall_prototype, @object
.type __set_hypercall_dtrace_probe_create, @object
.type __set_hypercall_dtrace_probe, @object
.type __set_hypercall_dtrace_register, @object
.type __set_hypercall_dtrace_unregister, @object
.type __set_hypercall_dtps_getargval, @object
.type __set_hypercall_dtps_getargdesc, @object
.section set_hypercall, "a", @progbits
.align 8
__set_hypercall_prototype:
	.quad hc_prototype
	.size __set_hypercall_prototype, 3
__set_hypercall_dtrace_probe_create:
	.quad hc_dtrace_probe_create
	.size __set_hypercall_dtrace_probe_create, 3
__set_hypercall_dtrace_probe:
	.quad hc_dtrace_probe
	.size __set_hypercall_dtrace_probe, 3
__set_hypercall_dtrace_register:
	.quad hc_dtrace_register
	.size __set_hypercall_dtrace_register, 3
__set_hypercall_dtrace_unregister:
	.quad hc_dtrace_unregister
	.size __set_hypercall_dtrace_unregister, 3
__set_hypercall_dtps_getargval:
	.quad hc_dtps_getargval
	.size __set_hypercall_dtps_getargval, 3
__set_hypercall_dtps_getargdesc:
	.quad hc_dtps_getargdesc
	.size __set_hypercall_dtps_getargdesc, 3

