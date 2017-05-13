/*-
 * Copyright (c) 2016 Domagoj Stolfa <domagoj.stolfa@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/malloc.h>

#include <sys/dtvirt.h>

#include <machine/vmm.h>
#include <machine/vmm_dtrace.h>

struct vmmdt_probe {
	uint64_t	vmdtp_args[VMMDT_MAXARGS];
	int		vmdtp_id;
	uint8_t		vmdtp_enabled;
};

struct vmmdt_probelist {
	struct vmmdt_probe	*vmdpl_list;
	size_t			 vmdpl_size;
};

struct vmmdt_vmlist {
	struct vmmdt_probelist	 *vm_list;
	size_t			  vm_listsize;
#define	VMMDT_INITSIZ		  4096
};

static MALLOC_DEFINE(M_VMMDT, "VMM DTrace buffer",
    "Holds the data related to the VMM layer for DTvirt");

static struct vmmdt_vmlist vmmdt_vms;
static int vmmdt_initialized = 0;

static int	vmmdt_init(void);
static int	vmmdt_alloc_vmlist(void);
static void	vmmdt_cleanup(void);
static int	vmmdt_add_probe(const char *, int);
static void	vmmdt_rm_probe(const char *, int);
static void	vmmdt_enable_probe(const char *, int);
static void	vmmdt_disable_probe(const char *, int);
static int	vmmdt_enabled(const char *, int);
static void	vmmdt_fire_probe(const char *, int,
           	    uintptr_t, uintptr_t, uintptr_t,
		    uintptr_t, uintptr_t);
static uint64_t	vmmdt_valueof(const char *, int, int);
static void	vmmdt_set_args(const char *, int,
           	    const uint64_t[VMMDT_MAXARGS]);
static struct vmmdt_probelist * vmmdt_hash_lookup(const char *);
static int	vmmdt_hash_is_enabled(const char *, int);
static uint64_t	vmmdt_hash_getargval(const char *, int, int);
static struct vmmdt_probe * vmmdt_binary_find(struct vmmdt_probelist *,
                                int);

static int
vmmdt_handler(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		error = vmmdt_init();
		if (error == 0)
			vmmdt_initialized = 1;
		break;
	case MOD_UNLOAD:
		vmmdt_cleanup();
		error = 0;
		vmmdt_initialized = 0;
	default:
		error = 0;
		break;
	}

	return (error);
}

static moduledata_t vmmdt_kmod = {
	"vmmdt",
	vmmdt_handler,
	NULL
};

MODULE_VERSION(vmmdt, 1);
MODULE_DEPEND(vmmdt, vmm, 1, 1, 1);
MODULE_DEPEND(vmmdt, dtrace, 1, 1, 1);

DECLARE_MODULE(vmmdt, vmmdt_kmod, SI_SUB_SMP + 1, SI_ORDER_ANY);

static int
vmmdt_init(void)
{
	int error;

	error = 0;

	vmmdt_hook_add = vmmdt_add_probe;
	vmmdt_hook_rm = vmmdt_rm_probe;
	vmmdt_hook_enable = vmmdt_enable_probe;
	vmmdt_hook_disable = vmmdt_disable_probe;
	vmmdt_hook_fire_probe = vmmdt_fire_probe;
	vmmdt_hook_valueof = vmmdt_valueof;
	vmmdt_hook_setargs = vmmdt_set_args;

	error = vmmdt_alloc_vmlist();

	return (error);
}

static int
vmmdt_alloc_vmlist(void)
{
	vmmdt_vms.vm_list = malloc(
	    sizeof(struct vmmdt_probelist) * VMMDT_INITSIZ,
	    M_VMMDT, M_ZERO | M_NOWAIT);

	if (vmmdt_vms.vm_list == NULL)
		return (ENOMEM);

	vmmdt_vms.vm_listsize = VMMDT_INITSIZ;

	return (0);
}



static void
vmmdt_cleanup(void)
{
	if (vmmdt_vms.vm_list != NULL)
		free(vmmdt_vms.vm_list, M_VMMDT);

	vmmdt_hook_add = NULL;
	vmmdt_hook_rm = NULL;
	vmmdt_hook_enable = NULL;
	vmmdt_hook_disable = NULL;
	vmmdt_hook_fire_probe = NULL;
	vmmdt_hook_valueof = NULL;
	vmmdt_hook_setargs = NULL;
}

static int
vmmdt_add_probe(const char *vm, int id)
{
	struct vmmdt_probelist *probelist;
	int error;

	probelist = vmmdt_hash_lookup(vm);
	error = vmmdt_insert_sorted_probelist(probelist, id);

	return (error);
}

static void
vmmdt_rm_probe(const char *vm, int id)
{
	struct vmmdt_probe *probe;
	struct vmmdt_probelist *probelist;

	probelist = vmmdt_hash_lookup(vm);
	probe = vmmdt_remove_probelist(probelist, id);

	free(probe, M_VMMDT);
}

static void
vmmdt_toggle_probe(const char *vm, int id, int flag)
{
	struct vmmdt_probe *probe;
	struct vmmdt_probelist *probelist;

	probelist = vmmdt_hash_lookup(vm);
	probe = vmmdt_binary_find(probelist, id);

	probe->vmdtp_enabled = flag;
}

static __inline void
vmmdt_enable_probe(const char *vm, int id)
{
	vmmdt_toggle_probe(vm, id, 1);
}

static void
vmmdt_disable_probe(const char *vm, int id)
{
	vmmdt_toggle_probe(vm, id, 0);
}

static __inline int
vmmdt_enabled(const char *vm, int probe)
{
	/*
	 * TODO:
	 * Make a big table of pointers to radix trees that are indexed with the
	 * probe id, get the necessary radix tree, walk down the enabled VMs and
	 * find the one we want
	 */
	return (vmmdt_hash_is_enabled(vm, probe));
}

static  __inline void
vmmdt_fire_probe(const char *vm, int probeid,
    uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,
    uintptr_t arg3, uintptr_t arg4)
{
	if (vmmdt_enabled(vm, probeid))
		dtvirt_hook_commit(vm, probeid, arg0, arg1,
		    arg2, arg3, arg4);
}

static __inline uint64_t
vmmdt_valueof(const char *vm, int probeid, int ndx)
{
	return (vmmdt_hash_getargval(vm, probeid, ndx));
}

static void
vmmdt_set_args(const char *vm, int probeid, const uint64_t args[VMMDT_MAXARGS])
{
	struct vmmdt_probelist *probelist;
	struct vmmdt_probe *probe;

	probelist = vmmdt_hash_lookup(vm);
	probe = vmmdt_binary_find(probelist, probeid);

	memcpy(probe->vmdtp_args, args, VMMDT_MAXARGS);
}

/*
 * Lookup functions
 */

static struct vmmdt_probelist *
vmmdt_hash_lookup(const char *vm)
{
	uint32_t idx;
	uint32_t hash_res;
	uint32_t i;

	i = 0;
	hash_res = murmur3_32_hash(vm, strlen(vm), INIT_HASH);

	while (vmmdt_vms.vm_list[idx] != NULL &&
	    strcmp(vm, vmmdt_vms.vm_list[idx]) != 0) {
		i++;
		idx = hash_res + c1*i + c2*i*i;
	}

	return (vmmdt_vms.vm_list[idx]);
}

static int
vmmdt_hash_is_enabled(const char *vm, int probeid)
{
	struct vmmdt_probelist *probelist;

	probes = vmmdt_hash_lookup(vm);

	return (vmmdt_binary_find(probelist, probeid) == NULL ? 0 : 1);
}

static uint64_t
vmmdt_hash_getargval(const char *vm, int probeid, int ndx)
{
	struct vmmdt_probelist *probelist;
	struct vmmdt_probe *probe;
	uint64_t argval;
	int istcid;

	probelist = vmmdt_hash_lookup(vm);
	probe = vmmdt_binary_find(probelist, probeid);

	/*
	 * FIXME: This should fill in the DTRACE_ARGNONE
	 */
	if (probe == NULL)
		return (0);

	return (probe->vmdtp_argval[ndx]);
}

static struct vmmdt_probe *
vmmdt_binary_find(struct vmmdt_probelist *probelist, int probeid)
{
	uint32_t first;
	uint32_t last;
	uint32_t middle;

	first = 0;
	last = probelist->vmdpl_size - 1;
	middle = (first + last) / 2;

	while (first <= last) {
		if (probelist->vmdpl_list[middle] < probeid)
			first = middle + 1;
		else if (probelist->vmdpl_list[middle] == probeid)
			return (probelist->vmdpl_list[middle]);
		else
			last = middle - 1;
		
		middle = (first + last) / 2;
	}

	return (NULL);
}

static struct vmmdt_probe *
vmmdt_remove_probelist(struct vmmdt_probelist *probelist, int probeid)
{
	struct vmmdt_probe *probe;
	struct vmmdt_probe *plist;
	int i;

	plist = probelist->vmdpl_list;
	probe = &plist[probeid];
	i = probeid;

	while (plist[i] != NULL) {
		
	}

	return (probe);
}
