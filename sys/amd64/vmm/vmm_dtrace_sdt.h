#ifndef _VMM_DTRACE_SDT_H_
#define _VMM_DTRACE_SDT_H_

#include <sys/sdt.h>

#ifndef _KERNEL
#error "no user-servicable parts inside"
#endif

#ifdef KDTRACE_HOOKS
SDT_PROVIDER_DECLARE(vmm);
SDT_PROBE_DECLARE(vmm, vmm_host, vm_create, vm_create);
SDT_PROBE_DECLARE(vmm, vmm_host, vm_suspend, vm_suspend);
SDT_PROBE_DECLARE(vmm, vmm_host, vm_run, vm_run);
SDT_PROBE_DECLARE(vmm, vmm_host, nested_fault, nested_fault);
#endif

#endif
