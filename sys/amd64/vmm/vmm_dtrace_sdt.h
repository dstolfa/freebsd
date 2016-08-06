#ifndef _VMM_DTRACE_SDT_H_
#define _VMM_DTRACE_SDT_H_

#include <sys/sdt.h>
#include <machine/vmm.h>

#ifndef _KERNEL
#error "no user-servicable parts inside"
#endif

#ifdef KDTRACE_HOOKS
extern int	dtrace_probes_enabled;

extern int	(*dtvmm_hook_create)(const char *name, struct vm *vm);
extern int	(*dtvmm_hook_suspend)(struct vm *vm,
		                      enum vm_suspend_how how);
extern int	(*dtvmm_hook_run)(struct vm *vm, struct vm_run *vmrun);
extern int	(*dtvmm_hook_nested_fault)(struct vm *vm, int vcpuid,
		                           uint64_t info);

SDT_PROVIDER_DECLARE(vmm);
SDT_PROBE_DECLARE(vmm, vmm_host, vm_create, vm_create);
SDT_PROBE_DECLARE(vmm, vmm_host, vm_suspend, vm_suspend);
SDT_PROBE_DECLARE(vmm, vmm_host, vm_run, vm_run);
SDT_PROBE_DECLARE(vmm, vmm_host, nested_fault, nested_fault);
#endif

#endif
