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
#include <sys/module.h>
#include <sys/malloc.h>

#include <sys/dtvirt.h>

#include <machine/vmm.h>

#define	VMMDT_INITIAL_NBUCKETS	(1 << 32)

struct vmmdt_probeinfo {
	uint8_t	exists;
	uint8_t	enabled;
};

struct vmmdt_table {
	struct vmmdt_probeinfo	*entries;
	size_t			 size;
	int			 largest;
};

static MALLOC_DEFINE(M_VMMDT, "VMM DTrace buffer", "Holds the data related to the VMM layer for DTvirt");

static struct vmmdt_table *vmmdt_probes;
static int vmmdt_initialized = 0;

static int			vmmdt_init(void);
static struct vmmdt_table *	vmmdt_alloc_table(void);
static void			vmmdt_cleanup(void);
static void			vmmdt_add_probe(int id);
static void			vmmdt_rm_probe(int id);
static void			vmmdt_enable_probe(int id);
static void			vmmdt_disable_probe(int id);

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

	vmmdt_probes = vmmdt_alloc_table();
	
	if (vmmdt_probes == NULL)
		error = ENOMEM;

	return (error);
}

static struct vmmdt_table *
vmmdt_alloc_table(void)
{
	struct vmmdt_table *table;

	table = malloc(sizeof(struct vmmdt_table),
	    M_VMMDT, M_ZERO | M_NOWAIT);

	return (table);
}

static void
vmmdt_cleanup(void)
{

}

static void
vmmdt_add_probe(int id)
{

}

static void
vmmdt_rm_probe(int id)
{

}

static void
vmmdt_enable_probe(int id)
{

}

static void
vmmdt_disable_probe(int id)
{

}

