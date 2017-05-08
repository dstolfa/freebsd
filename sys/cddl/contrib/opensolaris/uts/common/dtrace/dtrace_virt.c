/*-
 * Copyright (c) 2017 Domagoj Stolfa
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

#include <sys/dtrace_virt.h>

static void
dtvirt_commit(const char *instance, dtrace_id_t id,
    uintptr_t, uintptr_t, uintptr_t,
    uintptr_t, uintptr_t);

/*
 * Here we allocate a new virtual probe and attempt to register it with the
 * DTrace framework. 
 *
 * TODO: Give the information to the hypervisor layer so that we can find out
 * whether or not the probe is enabled and send it over to dtvirt_commit.
 */
void dtps_virt_provide(void *arg, dtrace_probedesc_t *desc)
{
	dtrace_provider_id_t id;
	dtrace_virt_params_t *params;
	dtrace_virt_probe_t *virt_probe;
	params = (dtrace_virt_params_t *) arg;

	if (desc == NULL)
		return;

	id = params->dtvp_provid;

	if (id == NULL)
		return;

	virt_probe = kmem_zalloc(sizeof(dtrace_virt_probe_t), KM_SLEEP);
	virt_probe->enabled = 0;

	if (dtrace_probe_lookup(id, desc->dtpd_mod, desc->dtpd_func,
	    desc->dtpd_name) == 0) {
		virt_probe->id = dtrace_probe_create(id, desc->dtpd_mod,
		    desc->dtpd_func, desc->dtpd_name, 0, virt_probe);
	}

	ASSERT(virt_probe->id != 0);
}

/*
 * Simply enable or disable the probe, not much more is needed here
 */
void dtps_virt_enable(void *arg, dtrace_id_t id, void *parg)
{
	dtrace_virt_probe_t *virt_probe;
	virt_probe = (dtrace_virt_probe_t *) parg;
	
	virt_probe->enabled = 1;
	/*
	 * TODO: Notify the hypervisor layer that the probe is enabled
	 */
}

void dtps_virt_disable(void *arg, dtrace_id_t id, void *parg)
{
	dtrace_virt_probe_t *virt_probe;
	virt_probe = (dtrace_virt_probe_t *) parg;
	
	virt_probe->enabled = 0;
	/*
	 * TODO: Notify the hypervisor layer that the probe is disabled
	 */
}

void dtps_virt_getargdesc(void *arg, dtrace_id_t id, void *parg,
    dtrace_argdesc_t *desc)
{
	dtrace_virt_probe_t *virt_probe;
	int id;

	virt_probe = (dtrace_virt_probe_t *) parg;
	id = desc->dtargd_ndx;

	strlcpy(desc->dtargd_native, virt_probe->arg_types[id],
	    sizeof(desc->dtargd_native));
}

void dtps_virt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	dtrace_virt_probe_t *virt_probe;

	virt_probe = (dtrace_virt_probe_t *) parg;
	kmem_free(virt_probe, sizeof(dtrace_virt_probe_t));
	/*
	 * TODO: Notify the hypervisor layer that the probe is destroyed and
	 * should not be called or checked for anymore.
	 */
}

/*
 * Just calls dtrace_distributed_probe(). The caller should check whether or not
 * this probe is enabled.
 */
static void
dtvirt_commit(const char *instance, dtrace_id_t id,
    uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,
    uintptr_t arg3, uintptr_t arg4)
{
	dtrace_distributed_probe(instance, id, arg0, arg1, arg2,
	    arg3, arg4);
}
