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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/event.h>
#include <sys/linker_set.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/dtrace_bsd.h>

#include <machine/vmm.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <libgen.h>
#include <sysexits.h>

#include <vmmapi.h>

#include "bhyverun.h"
#include "pci_emul.h"
#include "virtio.h"
#include "mevent.h"
#include "sockstream.h"

#define	VTDTR_RINGSZ			64
#define	VTDTR_MAXQ			4

#define	VTDTR_DEVICE_READY		0x00
#define	VTDTR_DEVICE_DESTROY		0x01
#define	VTDTR_DEVICE_REGISTER		0x02
#define	VTDTR_DEVICE_UNREGISTER		0x03
#define	VTDTR_DEVICE_PROBE_CREATE	0x04
#define	VTDTR_DEVICE_PROBE_INSTALL	0x05
#define	VTDTR_DEVICE_PROBE_UNINSTALL	0x06
#define	VTDTR_MAXEV			0x05 /* 0x02 - 0x06 */

#define	VTDTR_INDEX(x) ((x) - 2)

#define	VTDTR_F_PROBE			0x01
#define	VTDTR_F_PROV			0x02
/*
 * We only support probe operations right now
 */
#define	VTDTR_S_HOSTFEATURES		\
    (VTDTR_F_PROBE)

static int pci_vtdtr_debug;
#define	DPRINTF(params)		if (pci_vtdtr_debug) printf params
#define	WPRINTF(params)		printf params

struct pci_vtdtr_control {
	uint32_t	event;
	uint32_t	value;
	char		info[DTRACE_INSTANCENAMELEN];
} __attribute__((packed));

struct pci_vtdtr_softc {
	struct virtio_softc		 vsd_vs;
	struct vqueue_info		 vsd_queues[VTDTR_MAXQ];
	struct vmctx			*vsd_vmctx;
	struct dtrace_probeinfo		 vsd_pbi;
	struct mevent			*vtdtr_mev;
	pthread_mutex_t			 vsd_mtx;
	uint64_t			 vsd_features;
	uint64_t			 vsd_cfg;
	int				 vsd_ready;
};

static void	pci_vtdtr_reset(void *);
static void	pci_vtdtr_neg_features(void *, uint64_t);
static void	pci_vtdtr_control_tx(struct pci_vtdtr_softc *,
           	    struct iovec *, int);
static void	pci_vtdtr_process_prov_evt(struct pci_vtdtr_softc *,
           	    struct pci_vtdtr_control *);
static void	pci_vtdtr_process_probe_evt(struct pci_vtdtr_softc *,
           	    struct pci_vtdtr_control *);
static void	pci_vtdtr_control_send(struct pci_vtdtr_softc *,
           	    struct pci_vtdtr_control *);
static void	pci_vtdtr_notify_tx(void *, struct vqueue_info *);
static void	pci_vtdtr_notify_rx(void *, struct vqueue_info *);
static void	pci_vtdtr_handle_mev(int, enum ev_type, int, void *);
static int	pci_vtdtr_init(struct vmctx *, struct pci_devinst *, char *);

static struct virtio_consts vtdtr_vi_consts = {
	"vtdtr",			/* name */
	VTDTR_MAXQ,			/* maximum virtqueues */
	0,				/* config reg size */
	pci_vtdtr_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	NULL,				/* read virtio config */
	NULL,				/* write virtio config */
	pci_vtdtr_neg_features,		/* apply negotiated features */
	VTDTR_S_HOSTFEATURES		/* features */
};

static void
pci_vtdtr_reset(void *vsc)
{
	struct pci_vtdtr_softc *sc;

	sc = vsc;

	DPRINTF(("vtdtr: device reset requested!\n"));
	vi_reset_dev(&sc->vsd_vs);
}

static __inline void
pci_vtdtr_neg_features(void *vsc, uint64_t negotiated_features)
{
	struct pci_vtdtr_softc *sc = vsc;
	sc->vsd_features = negotiated_features;
}

static void
pci_vtdtr_control_tx(struct pci_vtdtr_softc *sc, struct iovec *iov, int niov)
{
	/*
	 * TODO
	 */
}

static void
pci_vtdtr_control_rx(struct pci_vtdtr_softc *sc, struct iovec *iov, int niov)
{
	/*struct pci_vtdtr_control  resp;*/
	struct pci_vtdtr_control *ctrl;

	assert(niov == 1);

	ctrl = (struct pci_vtdtr_control *)iov->iov_base;
	switch (ctrl->event) {
	case VTDTR_DEVICE_READY:
		sc->vsd_ready = 1;
		break;
	case VTDTR_DEVICE_REGISTER:
	case VTDTR_DEVICE_UNREGISTER:
	case VTDTR_DEVICE_DESTROY:
		if (sc->vsd_features & VTDTR_F_PROV)
			pci_vtdtr_process_prov_evt(sc, ctrl);
		break;
	case VTDTR_DEVICE_PROBE_CREATE:
	case VTDTR_DEVICE_PROBE_INSTALL:
	case VTDTR_DEVICE_PROBE_UNINSTALL:
		if (sc->vsd_features & VTDTR_F_PROBE)
			pci_vtdtr_process_probe_evt(sc, ctrl);
		break;
	}
}

static void
pci_vtdtr_process_prov_evt(struct pci_vtdtr_softc *sc,
    struct pci_vtdtr_control *ctrl)
{
	/*
	 * XXX: The processing functions... are the actually
	 * necessary, or do we want a layer that DTrace talks
	 * to and simply delegates it towards the virtio driver?
	 */
}

static void
pci_vtdtr_process_probe_evt(struct pci_vtdtr_softc *sc,
    struct pci_vtdtr_control *ctrl)
{

}

static void
pci_vtdtr_control_send(struct pci_vtdtr_softc *sc,
    struct pci_vtdtr_control *ctrl)
{
	/*
	 * FIXME: This outright does not work
	 */
	struct vqueue_info *vq;
	struct iovec iov;
	uint32_t len;
	uint16_t idx;
	int n;

	vq = &sc->vsd_queues[0];

	/*
	 * Can't fill the descs if the guest does not
	 * have memory mapped
	 */
	if (!vq_has_descs(vq)) {
		WPRINTF(("No available descriptors in order"
		    " to send a control message\n"));
		return;
	}

	/*
	 * Retrieve the iovec with the memory location
	 */
	n = vq_getchain(vq, &idx, &iov, 1, NULL);
	assert(n == 1);
		
	len = sizeof(struct pci_vtdtr_control);
	memcpy(iov.iov_base, ctrl, len);

	vq_relchain(vq, idx, len);
	vq_endchains(vq, 1);
}

/*
 * XXX: These two routines are practically the same, is TX
 * really necessary here? We don't really need to do anything
 */
static void
pci_vtdtr_notify_tx(void *vsc, struct vqueue_info *vq)
{
}

static void
pci_vtdtr_notify_rx(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtdtr_softc *sc;
	struct iovec iov[1];
	uint16_t idx;
	uint16_t flags[8];
	int n;

	sc = vsc;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, 1, flags);
		pci_vtdtr_control_rx(sc, iov, 1);
		vq_relchain(vq, idx, sizeof(struct dtrace_probeinfo));
	}

	vq_endchains(vq, 1);
}

static void
pci_vtdtr_handle_mev(int fd __unused, enum ev_type et __unused, int ne, void *vsc)
{
	struct pci_vtdtr_softc *sc;
	struct pci_vtdtr_control ctrl;
	char *name;

	sc = vsc;
	name = vm_get_name(sc->vsd_vmctx);
/*	if (strcmp(name, sc->vsd_pbi.instance) != 0)
		return; */
	/*
	 * Testing purposes... good god.
	 */

	if (ne & NOTE_PROBE_INSTALL)
		ctrl.event = VTDTR_DEVICE_PROBE_INSTALL;
	else if (ne & NOTE_PROBE_UNINSTALL)
		ctrl.event = VTDTR_DEVICE_PROBE_UNINSTALL;
	else
		return;
	ctrl.value = sc->vsd_pbi.id;
	strcpy(ctrl.info, sc->vsd_pbi.instance);
	
	pci_vtdtr_control_send(sc, &ctrl);
}

static int
pci_vtdtr_init(struct vmctx *ctx, struct pci_devinst *pci_inst, char *opts)
{
	struct pci_vtdtr_softc *sc;

	sc = calloc(1, sizeof(struct pci_vtdtr_softc));

	vi_softc_linkup(&sc->vsd_vs, &vtdtr_vi_consts, sc, pci_inst, sc->vsd_queues);
	sc->vsd_vs.vs_mtx = &sc->vsd_mtx;
	sc->vsd_vmctx = ctx;

	sc->vsd_queues[0].vq_qsize = VTDTR_RINGSZ;
	sc->vsd_queues[0].vq_notify = pci_vtdtr_notify_tx;
	sc->vsd_queues[1].vq_qsize = VTDTR_RINGSZ;
	sc->vsd_queues[1].vq_notify = pci_vtdtr_notify_rx;

	pci_set_cfgdata16(pci_inst, PCIR_DEVICE, VIRTIO_DEV_DTRACE);
	pci_set_cfgdata16(pci_inst, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pci_inst, PCIR_CLASS, PCIC_OTHER);
	pci_set_cfgdata16(pci_inst, PCIR_SUBDEV_0, VIRTIO_TYPE_DTRACE);
	pci_set_cfgdata16(pci_inst, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (vi_intr_init(&sc->vsd_vs, 1, fbsdrun_virtio_msix()))
		return (1);

	vi_set_io_bar(&sc->vsd_vs, 0);

	sc->vtdtr_mev = mevent_add(0, EVF_DTRACE, pci_vtdtr_handle_mev,
	    sc, (__intptr_t)&(sc->vsd_pbi));
	return (0);
}

struct pci_devemu pci_de_vdtr = {
	.pe_emu		= "virtio-dtrace",
	.pe_init	= pci_vtdtr_init,
	.pe_barwrite	= vi_pci_write,
	.pe_barread	= vi_pci_read
};
PCI_EMUL_SET(pci_de_vdtr);
