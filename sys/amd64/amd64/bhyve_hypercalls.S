#include <machine/asmacros.h>
#include <machine/bhyve_hypercall.h>

#define VENTER push %rbp ; mov %rsp, %rbp
#define VLEAVE pop %rbp

.globl __start_set_hypercall
.globl __stop_set_hypercall

ENTRY(hypercall_dtrace_probe_create)
	VENTER
	movq $HYPERCALL_DTRACE_PROBE_CREATE, %rax
hc_dt_pb_c:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtrace_probe_create)

ENTRY(hypercall_dtrace_probe)
	VENTER
	movq $HYPERCALL_DTRACE_PROBE, %rax
hc_dt_pb:
	.byte 0x0f,0x01,0xc1
	VLEAVE
	ret
END(hypercall_dtrace_probe)

.type __set_hypercall_dtrace_probe_create, @object
.type __set_hypercall_dtrace_probe, @object
.section set_hypercall, "a", @progbits
.align 8
__set_hypercall_dtrace_probe_create:
	.quad hc_dt_pb_c
	.size __set_hypercall_dtrace_probe_create, 3
__set_hypercall_dtrace_probe:
	.quad hc_dt_pb
	.size __set_hypercall_dtrace_probe, 3
