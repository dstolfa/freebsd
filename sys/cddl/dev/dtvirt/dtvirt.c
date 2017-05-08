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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/dtvirt.h>

static void	dtvirt_load(void);
static void	dtvirt_unload(void);
static void	dtvirt_commit(const char *, dtrace_id_t,
           	    uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static void	dtvirt_provide(void *, dtrace_probedesc_t *);
static void	dtvirt_enable(void *, dtrace_id_t, void *);
static void	dtvirt_disable(void *, dtrace_id_t, void *);
static void	dtvirt_getargdesc(void *, dtrace_id_t,
           	    void *, dtrace_argdesc_t *);
static void	dtvirt_destroy(void *, dtrace_id_t, void *);


static int
dtvirt_handler(module_t mod, int what, void *arg)
{
	int error;
	switch (what) {
	case MOD_LOAD:
		dtvirt_load();
		error = 0;
		break;
	case MOD_UNLOAD:
		error = 0;
		break;
	default:
		error = 0;
		break;
	}

	return (error);
}

static moduledata_t dtvirt_kmod = {
	"dtvirt",
	dtvirt_handler,
	NULL
};

MODULE_VERSION(dtvirt, 1);
MODULE_DEPEND(dtvirt, dtrace, 1, 1, 1);
MODULE_DEPEND(dtvirt, vmm, 1, 1, 1);

DECLARE_MODULE(dtvirt, dtvirt_kmod, SI_SUB_DTRACE, SI_ORDER_ANY);

static void
dtvirt_load(void)
{
	dtvirt_hook_commit = dtvirt_commit;
	dtvirt_hook_provide = dtvirt_provide;
	dtvirt_hook_enable = dtvirt_enable;
	dtvirt_hook_disable = dtvirt_disable;
	dtvirt_hook_getargdesc = dtvirt_getargdesc;
	dtvirt_hook_destroy = dtvirt_destroy;
}

static void
dtvirt_unload(void)
{
	dtvirt_hook_commit = NULL;
	dtvirt_hook_provide = NULL;
	dtvirt_hook_enable = NULL;
	dtvirt_hook_disable = NULL;
	dtvirt_hook_getargdesc = NULL;
	dtvirt_hook_destroy = NULL;
}

static void
dtvirt_commit(const char *instance, dtrace_id_t id,
    uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,
    uintptr_t arg3, uintptr_t arg4)
{
	dtrace_distributed_probe(instance, id, arg0, arg1,
	    arg2, arg3, arg4);
}

static void
dtvirt_provide(void *arg, dtrace_probedesc_t *desc)
{

}

static void
dtvirt_enable(void *arg, dtrace_id_t id, void *parg)
{

}

static void
dtvirt_disable(void *arg, dtrace_id_t id, void *parg)
{

}

static void
dtvirt_getargdesc(void *arg, dtrace_id_t id,
    void *parg, dtrace_argdesc_t *adesc)
{

}

static void
dtvirt_destroy(void *arg, dtrace_id_t id, void *parg)
{

}

