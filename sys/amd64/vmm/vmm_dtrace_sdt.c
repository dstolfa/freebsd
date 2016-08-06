#include <sys/param.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>

#include "vmm_dtrace_sdt.h"

/* Temporarily to test things out, ideally this is implemented using
 * <sys/dtrace.h> and <sys/dtrace_bsd.h>, however there are compilation
 * issues. This needs to be solved, but until then this seems like a good
 * way to experiment with DTrace probes inside vmm.
 *
 * The API provided is the same and there would ideally be no changed in
 * vmm.c.
 */

SDT_PROVIDER_DEFINE(vmm);
SDT_PROBE_DEFINE2(vmm, host, vm_create, vm_create, "const char *", "struct vm *");
SDT_PROBE_DEFINE2(vmm, host, vm_suspend, vm_suspend, "struct vm *", "enum vm_suspend_how");
SDT_PROBE_DEFINE2(vmm, host, vm_run, vm_run, "struct vm *", "struct vm_run *");
SDT_PROBE_DEFINE3(vmm, host, nested_fault, nested_fault, "struct vm *", "int", "uint64_t");

static int	dtvmm_unload(void);
static void	dtvmm_load(void *);
//static void	dtvmm_provide(void *, dtrace_probedesc_t *);
//static void	dtvmm_enable(void *, dtrace_id_t, void *);
//static void	dtvmm_disable(void *, dtrace_id_t, void *);
//static void	dtvmm_getargdesc(void *, dtrace_id_t *, void *, dtrace_argdesc_t *);

static int
dtvmm_create(const char *name, struct vm *vm)
{
	if (dtrace_probes_enabled) {
		SDT_PROBE2(vmm, host, vm_create, vm_create, name, vm);
	}

	return dtrace_probes_enabled;
}

static int
dtvmm_suspend(struct vm *vm, enum vm_suspend_how how)
{
	if (dtrace_probes_enabled) {
		SDT_PROBE2(vmm, host, vm_suspend, vm_suspend, vm, how);
	}

	return dtrace_probes_enabled;
}

static int
dtvmm_run(struct vm *vm, struct vm_run *vmrun)
{
	if (dtrace_probes_enabled) {
		SDT_PROBE2(vmm, host, vm_run, vm_run, vm, vmrun);
	}

	return dtrace_probes_enabled;
}

static int
dtvmm_nested_fault(struct vm *vm, int vcpuid, uint64_t info)
{
	if (dtrace_probes_enabled) {
		SDT_PROBE3(vmm, host, nested_fault, nested_fault, vm, vcpuid, info);
	}

	return dtrace_probes_enabled;
}

static void
dtvmm_load(void *unused)
{
	dtvmm_hook_create	= dtvmm_create;
	dtvmm_hook_suspend	= dtvmm_suspend;
	dtvmm_hook_run		= dtvmm_run;
	dtvmm_hook_nested_fault	= dtvmm_nested_fault;
}

static int
dtvmm_unload(void)
{
	dtvmm_hook_create	= NULL;
	dtvmm_hook_suspend	= NULL;
	dtvmm_hook_run		= NULL;
	dtvmm_hook_nested_fault	= NULL;
	return (0);
}

static int
dtvmm_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

SYSINIT(dtvmm_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, dtvmm_load, NULL);
SYSUNINIT(dtvmm_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, dtvmm_unload, NULL);

DEV_MODULE(dtvmm, dtvmm_modevent, NULL);
MODULE_VERSION(dtvmm, 1);
MODULE_DEPEND(dtnfscl, dtrace, 1, 1, 1);
MODULE_DEPEND(dtnfscl, opensolaris, 1, 1, 1);
