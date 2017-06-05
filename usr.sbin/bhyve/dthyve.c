#include <sys/dtrace.h>
#include <sys/uuid.h>
#include <sys/capsicum.h>
#include <sys/tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <dt_impl.h>
#include <dt_printf.h>
#include <dtrace.h>

#include "dthyve.h"

static dtrace_hdl_t *g_dtp;
static const char *g_prog = "dthyve";

struct dthyve_prov {
	RB_ENTRY(dthyve_prov)	node;
	char			vm[DTRACE_INSTANCENAMELEN];
	char			name[DTRACE_PROVNAMELEN];
	struct uuid		*uuid;
};

static int	dthyve_priv_unregister(struct dthyve_prov *);
static int	uuidcmp(const struct uuid *, const struct uuid *);
static int	dthyve_prov_cmp(struct dthyve_prov *, struct dthyve_prov *);

RB_HEAD(dthyve_provtree, dthyve_prov) dthyve_provider_tree =
    RB_INITIALIZER(_dthyve_prov);

RB_GENERATE_STATIC(dthyve_provtree, dthyve_prov, node,
    dthyve_prov_cmp);

int
dthyve_open(void)
{
	int error;

	g_dtp = dtrace_open(DTRACE_VERSION, 0, &error);
	return (error);
}

int
dthyve_register_provider(const char *vm, const char *name)
{
	dtrace_virt_providerdesc_t virt_pv;
	struct dthyve_prov *pv;
	int error;

	strlcpy(virt_pv.vpvd_instance, vm, DTRACE_INSTANCENAMELEN);
	strlcpy(virt_pv.vpvd_name, name, DTRACE_PROVNAMELEN);
	virt_pv.vpvd_uuid = malloc(sizeof(struct uuid));

	if (virt_pv.vpvd_uuid == NULL)
		return (ENOMEM);

	uuidgen(virt_pv.vpvd_uuid, 1);

	if ((error = dt_ioctl(g_dtp, DTRACEIOC_PROVCREATE, &virt_pv)) != 0)
		return (error);

	pv = malloc(sizeof(struct dthyve_prov));
	if (pv == NULL) {
		error = dt_ioctl(g_dtp, DTRACEIOC_PROVDESTROY,
		    virt_pv.vpvd_uuid);
		assert(error == 0);

		free(virt_pv.vpvd_uuid);
		return (ENOMEM);
	}

	strlcpy(pv->vm, vm, DTRACE_INSTANCENAMELEN);
	strlcpy(pv->name, name, DTRACE_PROVNAMELEN);
	pv->uuid = virt_pv.vpvd_uuid;

	RB_INSERT(dthyve_provtree, &dthyve_provider_tree, pv);

	return (0);
}

static int
dthyve_priv_unregister(struct dthyve_prov *pv)
{
	int error;

	error = dt_ioctl(g_dtp, DTRACEIOC_PROVDESTROY, pv->uuid);
	RB_REMOVE(dthyve_provtree, &dthyve_provider_tree, pv);
	free(pv->uuid);
	free(pv);

	return (error);
}

int
dthyve_unregister_provider(struct uuid *uuid)
{
	struct dthyve_prov *pv, tmp;
	tmp.uuid = uuid;

	if (uuid == NULL)
		return (EINVAL);

	pv = RB_FIND(dthyve_provtree, &dthyve_provider_tree, &tmp);
	if (pv == NULL)
		return (ESRCH);

	return (dthyve_priv_unregister(pv));
}

int
dthyve_probe_create(struct uuid *uuid, const char *mod,
    const char *func, const char *name)
{
	dtrace_virt_probedesc_t vpdesc;

	strlcpy(vpdesc.vpbd_mod, mod, DTRACE_MODNAMELEN);
	strlcpy(vpdesc.vpbd_func, func, DTRACE_FUNCNAMELEN);
	strlcpy(vpdesc.vpbd_name, name, DTRACE_NAMELEN);

	vpdesc.vpbd_uuid = uuid;

	return (dt_ioctl(g_dtp, DTRACEIOC_PROBECREATE, &vpdesc));
}

void
dthyve_cleanup(void)
{
	struct dthyve_prov *pv, *tmp;
	int error;

	RB_FOREACH_SAFE(pv, dthyve_provtree, &dthyve_provider_tree, tmp) {
		error = dthyve_priv_unregister(pv);
		assert(error == 0);
	}
}

static int
uuidcmp(const struct uuid *uuid1, const struct uuid *uuid2)
{
	const uint64_t *u1_hi, *u1_lo, *u2_hi, *u2_lo;

	assert(uuid1 != NULL);
	assert(uuid2 != NULL);

	u1_hi = (const uint64_t *) uuid1;
	u1_lo = (const uint64_t *) (u1_hi + 1);

	u2_hi = (const uint64_t *) uuid2;
	u2_lo = (const uint64_t *) (u2_hi + 1);

	if (*u1_hi > *u2_hi)
		return (1);
	else if (*u1_hi < *u2_hi)
		return (-1);

	if (*u1_lo > *u2_lo)
		return (1);
	else if (*u1_lo < *u2_lo)
		return (-1);

	return (0);
}

static int
dthyve_prov_cmp(struct dthyve_prov *p1, struct dthyve_prov *p2)
{
	struct uuid *p1_uuid, *p2_uuid;

	p1_uuid = p1->uuid;
	p2_uuid = p2->uuid;

	return (uuidcmp(p1_uuid, p2_uuid));
}
