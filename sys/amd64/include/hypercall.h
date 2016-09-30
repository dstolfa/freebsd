#ifndef _MACHINE_HYPERCALL_H_
#define _MACHINE_HYPERCALL_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <x86/x86_var.h>
#include <x86/cputypes.h>

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
		    "push %[val0]\n"
		    "push %[len0]\n"
		    VMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c),
		      [val0] "r"(arg0->val), [len0] "r"(arg0->len),
		      "b"(nargs)
		    : "memory", "rsp");
	} else {
		__asm __volatile(
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val3]\n"
		    "push %[len3]\n"
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val3]\n"
		    "push %[len3]\n"
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val4]\n"
		    "push %[len4]\n"
		    "push %[val3]\n"
		    "push %[len3]\n"
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val4]\n"
		    "push %[len4]\n"
		    "push %[val3]\n"
		    "push %[len3]\n"
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val5]\n"
		    "push %[len5]\n"
		    "push %[val4]\n"
		    "push %[len4]\n"
		    "push %[val3]\n"
		    "push %[len3]\n"
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
		    "push %[val5]\n"
		    "push %[len5]\n"
		    "push %[val4]\n"
		    "push %[len4]\n"
		    "push %[val3]\n"
		    "push %[len3]\n"
		    "push %[val2]\n"
		    "push %[len2]\n"
		    "push %[val1]\n"
		    "push %[len1]\n"
		    "push %[val0]\n"
		    "push %[len0]\n"
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
