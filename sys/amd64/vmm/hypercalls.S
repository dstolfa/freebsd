#include <machine/asmacros.h>

ENTRY(hypercall_dtrace_probe_create)
	mov %rax, 0
	.byte 0x0f,0x01,0xc1
	ret
END(hypercall_dtrace_probe_create)

ENTRY(hypercall_dtrace_probe)
	mov %rax, 1
	.byte 0x0f,0x01,0xc1
	ret
END(hypercall_dtrace_probe)
