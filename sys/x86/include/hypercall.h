#ifndef _X86_HYPERCALL_H_
#define _X86_HYPERCALL_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <x86/x86_var.h>
#include <x86/cputypes.h>


#define VMCALL		".byte 0x0f,0x01,0xc1\n"
#define VMMCALL		".byte 0x0f,0x01,0xd9\n"

typedef u_char bool;

#ifdef __amd64__

/* XXX: Each of these hypercalls can
 * be rewritten in a better way with
 * less redundancy
 */

static __inline long
hypercall0(unsigned long c)
{
	const long nargs = 0;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    VMCALL
		    : "=a"(ret)
		    : "a"(c), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    VMMCALL
		    : "=a"(ret)
		    : "a"(c), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

static __inline long 
hypercall1(unsigned long c, unsigned long arg0)
{
	const long nargs = 1;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    "push %1\n"
		    VMCALL
		    "add $8, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    "push %1\n"
		    VMMCALL
		    "add $8, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

static __inline long
hypercall2(unsigned long c, unsigned long arg0,
    unsigned long arg1)
{
	const long nargs = 2;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    "push %2\n"
		    "push %1\n"
		    VMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    "push %2\n"
		    "push %1\n"
		    VMMCALL
		    "add $16, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline long
hypercall3(unsigned long c, unsigned long arg0,
    unsigned long arg1, unsigned long arg2)
{
	const long nargs = 3;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMCALL
		    "add $24, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMMCALL
		    "add $24, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline long
hypercall4(unsigned long c, unsigned long arg0,
    unsigned long arg1, unsigned long arg2,
    unsigned long arg3)
{
	const long nargs = 4;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMCALL
		    "add $32, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMMCALL
		    "add $32, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline long
hypercall5(unsigned long c, unsigned long arg0,
    unsigned long arg1, unsigned long arg2,
    unsigned long arg3, unsigned long arg4)
{
	const long nargs = 5;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMCALL
		    "add $40, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "g"(arg4), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMMCALL
		    "add $40, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "g"(arg4), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
} 

static __inline long
hypercall6(unsigned long c, unsigned long arg0,
    unsigned long arg1, unsigned long arg2,
    unsigned long arg3, unsigned long arg4,
    unsigned long arg5)
{
	const long nargs = 6;
	long ret;
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		asm volatile(
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMCALL
		    "add $48, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "g"(arg4), "g"(arg5), "b"(nargs)
		    : "memory", "rsp");
	} else {
		asm volatile(
		    "push %6\n"
		    "push %5\n"
		    "push %4\n"
		    "push %3\n"
		    "push %2\n"
		    "push %1\n"
		    VMMCALL
		    "add $48, %%rsp\n"
		    : "=a"(ret)
		    : "a"(c), "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "g"(arg4), "g"(arg5), "b"(nargs)
		    : "memory", "rsp");
	}
	return (ret);
}

#endif /* __amd64__ */

#endif /* _X86_HYPERCALL_H_ */
