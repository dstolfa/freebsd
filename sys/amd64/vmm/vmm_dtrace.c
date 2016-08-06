#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <sys/dtrace.h>
#include <sys/dtrace_bsd.h>

#include "vmm_dtrace.h"

static int	dtvmm_unload(void);
static void	dtvmm_load(void *);
static void	dtvmm_provide(void *, dtrace_probedesc_t *);
static void	dtvmm_enable(void *, dtrace_id_t, void *);
static void	dtvmm_disable(void *, dtrace_id_t, void *);
static void	dtvmm_getargdesc(void *, dtrace_id_t *, void *, dtrace_argdesc_t *);

static dtrace_pattr_t dtvmm_attr = {
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
};

static char	*dtvmm_module_str		= "host";
static char	*dtvmm_name_create_str		= "vm_create";
static char	*dtvmm_name_suspend_str		= "vm_suspend";
static char	*dtvmm_name_run_str		= "vm_run";
static char	*dtvmm_name_nested_fault_str	= "nested_fault";

static dtrace_pops_t dtvmm_pops = {
	/* dtps_provide */		dtvmm_provide,
	/* dtps_provide_module */	NULL,
	/* dtps_enable */		dtvmm_enable,
	/* dtps_disable */		dtvmm_disable,
	/* dtps_suspend */		NULL,
	/* dtps_resume */		NULL,
	/* dtps_getargdesc */		dtvmm_getargdesc,
	/* dtps_getargval */		NULL,
	/* dtps_usermode */		NULL,
	/* dtps_destroy */		dtvmm_destroy
};

static dtrace_provider_id_t	dtvmm_id;

static int
dtvmm_create(const char *name, struct vm *vm)
{
	return 0;
}

static int
dtvmm_suspend(struct vm *vm, enum vm_suspend_how how)
{
	return 0;
}

static int
dtvmm_run(struct vm *vm, struct vm_run *vmrun)
{
	return 0;
}

static int
dtvmm_nested_fault(struct vm *vm, int vcpuid, uint64_t info)
{
	return 0;
}

static void
dtvmm_load(void *unused)
{
	if (dtrace_register("vmm", &dtvmm_attr, DTRACE_PRIV_USER, NULL,
	    &dtvmm_pops, NULL, &dtvmm_id) != 0)
		return;

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
