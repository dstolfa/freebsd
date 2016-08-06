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
static char	*dtvmm_name_create_str		= "create";
static char	*dtvmm_name_suspend_str		= "suspend";
static char	*dtvmm_name_run_str		= "run";
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

