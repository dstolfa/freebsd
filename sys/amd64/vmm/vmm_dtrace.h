#ifndef _VMM_DTRACE_H_
#define _VMM_DTRACE_H_

#include <sys/_cpuset.h>

#include <machine/vmm.h>

#ifndef _KERNEL
#error "no user-servicable parts inside"
#endif

#ifdef KDTRACE_HOOKS
extern int	dtrace_probes_enabled;

extern int	(*dtvmm_hook_create)(const char *name);
extern int	(*dtvmm_hook_suspend)(struct vm *vm,
          	    enum vm_suspend_how how);
extern int	(*dtvmm_hook_run)(struct vm *vm, struct vm_run *vmrun);
extern int	(*dtvmm_hook_nested_fault)(struct vm *vm, int vcpuid,
          	    uint64_t info);
extern int	(*dtvmm_hook_map_mmio)(const char *name, vm_paddr_t gpa,
          	    size_t len, vm_paddr_t hpa);
#endif

#endif