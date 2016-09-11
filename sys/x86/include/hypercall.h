#ifndef _X86_HYPERCALL_H_
#define _X86_HYPERCALL_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <x86/x86_var.h>
#include <x86/cputypes.h>


#define VMCALL		".byte 0x0f,0x01,0xc1\n"
#define VMMCALL		".byte 0x0f,0x01,0xd9\n"

#ifdef __amd64__

typedef struct hypercall_arg {
	__uint64_t len;
	__uint64_t val;
} hc_arg_t;

/* XXX: Each of these hypercalls can
 * be rewritten in a better way with
 * less redundancy
 */

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
		    "push %3\n"
		    "push %2\n"
		    VMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %3\n"
		    "push %2\n"
		    VMMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      "g"(arg0->val), "g"(arg0->len),
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
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMCALL
		    "add $32, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len), 
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %3\n"
		    "push %2\n"
		    VMMCALL
		    "add $32, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), 
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len), 
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
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMCALL
		    "add $48, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMMCALL
		    "add $48, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
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
		    "push %9\n"
		    "push %8\n"
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMCALL
		    "add $64, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "g"(arg3->val), "g"(arg3->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %9\n"
		    "push %8\n"
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMMCALL
		    "add $64, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "g"(arg3->val), "g"(arg3->len),
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
		    "push %11\n"
		    "push %10\n"
		    "push %9\n"
		    "push %8\n"
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMCALL
		    "add $80, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "g"(arg3->val), "g"(arg3->len),
		      "g"(arg4->val), "g"(arg4->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %11\n"
		    "push %10\n"
		    "push %9\n"
		    "push %8\n"
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMMCALL
		    "add $40, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "g"(arg3->val), "g"(arg3->len),
		      "g"(arg4->val), "g"(arg4->len),
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
		    "push %13\n"
		    "push %12\n"
		    "push %11\n"
		    "push %10\n"
		    "push %9\n"
		    "push %8\n"
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMCALL
		    "add $96, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "g"(arg3->val), "g"(arg3->len),
		      "g"(arg4->val), "g"(arg4->len),
		      "g"(arg5->val), "g"(arg5->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %13\n"
		    "push %12\n"
		    "push %11\n"
		    "push %10\n"
		    "push %9\n"
		    "push %8\n"
		    "push %7\n"
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    VMMCALL
		    "add $96, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      "g"(arg0->val), "g"(arg0->len),
		      "g"(arg1->val), "g"(arg1->len),
		      "g"(arg2->val), "g"(arg2->len),
		      "g"(arg3->val), "g"(arg3->len),
		      "g"(arg4->val), "g"(arg4->len),
		      "g"(arg5->val), "g"(arg5->len),
		      "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

#endif /* __amd64__ */

#endif /* _X86_HYPERCALL_H_ */
