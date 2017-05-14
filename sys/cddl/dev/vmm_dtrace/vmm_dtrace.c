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
#include <sys/hash.h>
#include <sys/tree.h>

#include <sys/dtvirt.h>

#include <machine/vmm.h>
#include <machine/vmm_dtrace.h>

struct vmmdt_probe {
	RB_ENTRY(vmmdt_probe)	vmdtp_node;
	uint64_t		vmdtp_args[VMMDT_MAXARGS];
	int			vmdtp_id;
	uint8_t			vmdtp_enabled;
};

struct vmdtree {
	RB_HEAD(vmmdt_probetree, vmmdt_probe)	vmdtree_head;
	struct mtx				vmdtree_mtx;
	char					vmdtree_vmname[VM_MAX_NAMELEN];
};

struct vmmdt_vmlist {
	struct vmdtree				**vm_list;
	struct mtx				  vm_listmtx;
#define	VMMDT_INITSIZ		 		  4096
};

static MALLOC_DEFINE(M_VMMDT, "VMM DTrace buffer",
    "Holds the data related to the VMM layer for DTvirt");

static struct vmmdt_vmlist vmmdt_vms;
static int vmmdt_initialized = 0;

static int	vmmdt_init(void);
static int	vmmdt_alloc_vmlist(void);
static void	vmmdt_cleanup(void);
static int	vmmdt_add_probe(const char *, int);
static int	vmmdt_rm_probe(const char *, int);
static void	vmmdt_enable_probe(const char *, int);
static void	vmmdt_disable_probe(const char *, int);
static int	vmmdt_enabled(const char *, int);
static void	vmmdt_fire_probe(const char *, int,
           	    uintptr_t, uintptr_t, uintptr_t,
		    uintptr_t, uintptr_t);
static uint64_t	vmmdt_valueof(const char *, int, int);
static void	vmmdt_set_args(const char *, int,
           	    const uint64_t[VMMDT_MAXARGS]);
static struct vmmdt_probetree * vmmdt_hash_lookup(const char *);
static int	vmmdt_hash_is_enabled(const char *, int);
static uint64_t	vmmdt_hash_getargval(const char *, int, int);
static int	vmmdt_probe_cmp(struct vmmdt_probe *, struct vmmdt_probe *);

RB_GENERATE_STATIC(vmmdt_probetree, vmmdt_probe, vmdtp_node,
    vmmdt_probe_cmp);

static uint8_t c1 = 2;
static uint8_t c2 = 2;
static uint32_t init_hash;

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

	init_hash = arc4random();

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
	char mtxname[32];
	int i;

	vmmdt_vms.vm_list = malloc(sizeof(struct vmdtree *) * VMMDT_INITSIZ,
	    M_VMMDT, M_ZERO | M_NOWAIT);

	if (vmmdt_vms.vm_list == NULL)
		return (ENOMEM);

	mtx_init(&vmmdt_vms.vm_listmtx, "vmlistmtx", NULL, MTX_DEF);

	mtx_lock(&vmmdt_vms.vm_listmtx);
	for (i = 0; i < VMMDT_INITSIZ; i++) {
		vmmdt_vms.vm_list[i] = malloc(sizeof(struct vmdtree),
		    M_VMMDT, M_ZERO | M_NOWAIT);
		if (vmmdt_vms.vm_list[i] == NULL)
			return (ENOMEM);

		snprintf(mtxname, sizeof(mtxname), "vmdtree_mtx-%d", i);
		mtx_init(&vmmdt_vms.vm_list[i]->vmdtree_mtx, mtxname, NULL, MTX_DEF);
		RB_INIT(&vmmdt_vms.vm_list[i]->vmdtree_head);
	}
	mtx_unlock(&vmmdt_vms.vm_listmtx);

	return (0);
}



static void
vmmdt_cleanup(void)
{
	struct vmmdt_probetree *rbhead;
	struct vmmdt_probe *tmp, *probe;
	int i;
	if (vmmdt_vms.vm_list == NULL)
		return;

	mtx_lock(&vmmdt_vms.vm_listmtx);
	for (i = 0; i < VMMDT_INITSIZ; i++) {
		rbhead = &vmmdt_vms.vm_list[i]->vmdtree_head;
		RB_FOREACH_SAFE(probe, vmmdt_probetree, rbhead, tmp) {
			if (probe != NULL) {
				free(probe, M_VMMDT);
			}
		}

		mtx_destroy(&vmmdt_vms.vm_list[i]->vmdtree_mtx);
		free(vmmdt_vms.vm_list[i], M_VMMDT);
		vmmdt_vms.vm_list[i] = NULL;
	}
	mtx_unlock(&vmmdt_vms.vm_listmtx);

	mtx_destroy(&vmmdt_vms.vm_listmtx);
	free(vmmdt_vms.vm_list, M_VMMDT);

	vmmdt_vms.vm_list = NULL;

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
	struct vmmdt_probetree *tree;
	struct vmmdt_probe *probe;

	probe = malloc(sizeof(struct vmmdt_probe),
	    M_VMMDT, M_ZERO | M_NOWAIT);

	if (probe == NULL)
		return (ENOMEM);

	probe->vmdtp_id = id;
	probe->vmdtp_enabled = 0;

	tree = vmmdt_hash_lookup(vm);

	if (tree == NULL)
		return (EINVAL);

	RB_INSERT(vmmdt_probetree, tree, probe);

	return (0);
}

static int
vmmdt_rm_probe(const char *vm, int id)
{
	struct vmmdt_probetree *tree;
	struct vmmdt_probe *probe, tmp;

	tree = vmmdt_hash_lookup(vm);

	if (tree == NULL)
		return (EINVAL);

	tmp.vmdtp_id = id;

	probe = RB_REMOVE(vmmdt_probetree, tree, &tmp);
	if (probe == NULL)
		return (EINVAL);

	free(probe, M_VMMDT);

	return (0);
}

static void
vmmdt_toggle_probe(const char *vm, int id, int flag)
{
	struct vmmdt_probetree *tree;
	struct vmmdt_probe *probe, tmp;

	tmp.vmdtp_id = id;
	tree = vmmdt_hash_lookup(vm);
	probe = RB_FIND(vmmdt_probetree, tree, &tmp);

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
vmmdt_enabled(const char *vm, int probeid)
{
	/*
	 * TODO:
	 * Make a big table of pointers to radix trees that are indexed with the
	 * probe id, get the necessary radix tree, walk down the enabled VMs and
	 * find the one we want
	 */
	struct vmmdt_probetree *tree;
	struct vmmdt_probe tmp;

	tmp.vmdtp_id = probeid;
	tree = vmmdt_hash_lookup(vm);
	return (RB_FIND(vmmdt_probetree, tree, &tmp) != NULL);
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
	struct vmmdt_probetree *tree;
	struct vmmdt_probe *probe, tmp;

	tmp.vmdtp_id = probeid;
	tree = vmmdt_hash_lookup(vm);
	probe = RB_FIND(vmmdt_probetree, tree, &tmp);
	KASSERT(probe != NULL, ("%s: invalid probe", __func__));

	return (probe->vmdtp_args[ndx]);
}

static void
vmmdt_set_args(const char *vm, int probeid, const uint64_t args[VMMDT_MAXARGS])
{
	struct vmmdt_probetree *tree;
	struct vmmdt_probe *probe, tmp;

	tmp.vmdtp_id = probeid;
	tree = vmmdt_hash_lookup(vm);
	probe = RB_FIND(vmmdt_probetree, tree, &tmp);

	memcpy(probe->vmdtp_args, args, VMMDT_MAXARGS);
}

static struct vmmdt_probetree *
vmmdt_hash_lookup(const char *vm)
{
	uint32_t idx;
	uint32_t hash_res;
	uint32_t i;

	i = 0;
	hash_res = murmur3_32_hash(vm, strlen(vm), init_hash);
	idx = hash_res;

	while (vmmdt_vms.vm_list[idx] != NULL &&
	    strcmp(vm, vmmdt_vms.vm_list[idx]->vmdtree_vmname) != 0) {
		i++;
		idx = hash_res + i/c1 + i*i/c2;
	}

	return (&vmmdt_vms.vm_list[idx]->vmdtree_head);
}

static int
vmmdt_probe_cmp(struct vmmdt_probe *p1, struct vmmdt_probe *p2)
{
	if (p1->vmdtp_id == p2->vmdtp_id)
		return (0);

	return ((p1->vmdtp_id > p2->vmdtp_id) ? 1 : -1);
}
