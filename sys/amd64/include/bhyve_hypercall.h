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

#include <sys/cdefs.h>
#include <sys/types.h>

#include <x86/x86_var.h>
#include <x86/cputypes.h>

#define PUSH(a) "push %[" #a "]\n"
#define PUSH2(a1, a2)							\
	PUSH(a1)							\
	PUSH(a2)
#define PUSH4(a1, a2, a3, a4)						\
	PUSH2(a1, a2)							\
	PUSH2(a3, a4)
#define PUSH6(a1, a2, a3, a4, a5, a6)					\
	PUSH4(a1, a2, a3, a4)						\
	PUSH2(a5, a6)
#define PUSH8(a1, a2, a3, a4, a5, a6, a7, a8)				\
	PUSH6(a1, a2, a3, a4, a5, a6)					\
	PUSH2(a7, a8)
#define PUSH10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)			\
	PUSH8(a1, a2, a3, a4, a5, a6, a7, a8)				\
	PUSH2(a9, a10)
#define PUSH12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)	\
	PUSH10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)			\
	PUSH2(a11, a12)

#define HYPERCALL_RET_NOT_IMPL	-2
#define HYPERCALL_RET_ERROR	-1
#define HYPERCALL_RET_SUCCESS	 0

#define VMCALL		".byte 0x0f,0x01,0xc1\n"
#define VMMCALL		".byte 0x0f,0x01,0xd9\n"

typedef struct hypercall_arg {
	__uint64_t	len;
	__uint64_t	val;
} hc_arg_t;

/*
 * Used to create additional known hypercalls. The name
 * of each of the enums should correspond to the function
 * being called once the hypercall is initiated.
 * Each enum should have it's corresponding number next
 * to it and should be in order, as the ring_plevel
 * array expects it to be that way.
 *
 * Keep in sync with ring_plevel.
 */
enum hypercall_index {
	HYPERCALL_DTRACE_PROBE_CREATE	= 0,
	HYPERCALL_DTRACE_PROBE		= 1,
	HYPERCALL_DTRACE_RESERVED1	= 2, /* Reserved for DTrace */
	HYPERCALL_DTRACE_RESERVED2	= 3, /* Reserved for DTrace */
	HYPERCALL_DTRACE_RESERVED3	= 4, /* Reserved for DTrace */
	HYPERCALL_DTRACE_RESERVED4	= 5, /* Reserved for DTrace */
	HYPERCALL_INDEX_MAX
};

static __inline __int64_t
hypercall0(__uint64_t c)
{
	const __uint64_t nargs = 0;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    VMCALL
		    : "=a"(ret)
		    : "a"(c), "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    VMMCALL
		    : "=a"(ret)
		    : "a"(c), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

static __inline __int64_t 
hypercall1(__uint64_t c, hc_arg_t *arg0)
{
	const __uint64_t nargs = 1;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    PUSH2(val0, len0)
		    VMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    PUSH2(val0, len0)
		    VMMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

static __inline __int64_t
hypercall2(__uint64_t c, hc_arg_t *arg0,
    hc_arg_t *arg1)
{
	const __uint64_t nargs = 2;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    PUSH4(val1, len1, val0, len0)
		    VMCALL
		    "add $32, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    PUSH4(val1, len1, val0, len0)
		    VMMCALL
		    "add $32, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline __int64_t
hypercall3(__uint64_t c, hc_arg_t *arg0,
    hc_arg_t *arg1, hc_arg_t *arg2)
{
	const __uint64_t nargs = 3;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    PUSH6(val2, len2, val1, len1,
		        val0, len0)
		    VMCALL
		    "add $48, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    PUSH6(val2, len2, val1, len1,
		        val0, len0)
		    VMMCALL
		    "add $48, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline __int64_t
hypercall4(__uint64_t c, hc_arg_t *arg0,
    hc_arg_t *arg1, hc_arg_t *arg2,
    hc_arg_t *arg3)
{
	const __uint64_t nargs = 4;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    PUSH8(val3, len3, val2, len2,
		        val1, len1, val0, len0)
		    VMCALL
		    "add $64, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val3] "r"(arg3->val), [len3] "r"(arg3->len),
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    PUSH8(val3, len3, val2, len2,
		        val1, len1, val0, len0)
		    VMMCALL
		    "add $64, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val3] "r"(arg3->val), [len3] "r"(arg3->len),
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline __int64_t
hypercall5(__uint64_t c, hc_arg_t *arg0,
    hc_arg_t *arg1, hc_arg_t *arg2,
    hc_arg_t *arg3, hc_arg_t *arg4)
{
	const __uint64_t nargs = 5;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    PUSH10(val4, len4, val3, len3,
		        val2, len2, val1, len1,
		        val0, len0)
		    VMCALL
		    "add $80, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val4] "r"(arg4->val), [len4] "r"(arg4->len),
		      [val3] "r"(arg3->val), [len3] "r"(arg3->len),
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    PUSH10(val4, len4, val3, len3,
		        val2, len2, val1, len1,
		        val0, len0)
		    VMMCALL
		    "add $80, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val4] "r"(arg4->val), [len4] "r"(arg4->len),
		      [val3] "r"(arg3->val), [len3] "r"(arg3->len),
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline __int64_t
hypercall6(__uint64_t c, hc_arg_t *arg0,
    hc_arg_t *arg1, hc_arg_t *arg2,
    hc_arg_t *arg3, hc_arg_t *arg4,
    hc_arg_t *arg5)
{
	const __uint64_t nargs = 6;
	__int64_t ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		__asm __volatile(
		    PUSH12(val5, len5, val4, len4,
		        val3, len3, val2, len2,
		        val1, len1, val0, len0)
		    VMCALL
		    "add $96, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val5] "r"(arg5->val), [len5] "r"(arg5->len),
		      [val4] "r"(arg4->val), [len4] "r"(arg4->len),
		      [val3] "r"(arg3->val), [len3] "r"(arg3->len),
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    PUSH12(val5, len5, val4, len4,
		        val3, len3, val2, len2,
		        val1, len1, val0, len0)
		    VMMCALL
		    "add $96, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      [val5] "r"(arg5->val), [len5] "r"(arg5->len),
		      [val4] "r"(arg4->val), [len4] "r"(arg4->len),
		      [val3] "r"(arg3->val), [len3] "r"(arg3->len),
		      [val2] "r"(arg2->val), [len2] "r"(arg2->len),
		      [val1] "r"(arg1->val), [len1] "r"(arg1->len), 
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

#endif /* _MACHINE_HYPERCALL_H_ */
