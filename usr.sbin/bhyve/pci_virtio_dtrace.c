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
#include <sys/linker_set.h>
#include <sys/uio.h>
#include <sys/types.h>

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

#include "bhyverun.h"
#include "pci_emul.h"
#include "virtio.h"
#include "mevent.h"
#include "sockstream.h"

#define	VTDTR_DEVICE_READY		0x00
#define	VTDTR_DEVICE_REGISTER		0x01
#define	VTDTR_DEVICE_UNREGISTER		0x02
#define	VTDTR_DEVICE_DESTROY		0x03
#define	VTDTR_DEVICE_PROBE_CREATE	0x04
#define	VTDTR_DEVICE_PROBE_INSTALL	0x05
#define	VTDTR_DEVICE_PROBE_UNINSTALL	0x06

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
	uint32_t	 event;
	uint32_t	 value;
	char		*info;
} __attribute__((packed));

struct pci_vtdtr_softc {
	struct virtio_softc		vsd_vs;
	struct vqueue_info		vsd_ctrl_rx;
	struct vqueue_info		vsd_ctrl_tx;
	pthread_mutex_t			vsd_mtx;
	uint64_t			vsd_features;
	int				vsd_ready;
};

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
pci_vtdtr_control_tx(struct pci_vtdtr_softc *sc, void *arg, struct iovec *iov,
    int niov)
{
	struct pci_vtdtr_control  resp;
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
		if (sc->vtd_features & VTDTR_F_PROV)
			pci_vtdtr_process_prov_evt(sc, ctrl);
		break;
	case VTDTR_DEVICE_PROBE_CREATE:
	case VTDTR_DEVICE_PROBE_INSTALL:
	case VTDTR_DEVICE_PROBE_UNINSTALL:
		if (sc->vtd_features & VTDTR_F_PROBE)
			pci_vtdtr_process_probe_evt(sc, ctrl);
		break;
	}
}

static void
pci_vtdtr_control_send(struct pci_vtdtr_softc *sc,
    struct pci_vtdtr_control *ctrl)
{
	/*
	 * XXX: This might not be very correct
	 */
	struct vqueue_info *vq;
	struct iovec iov;
	uint32_t len;
	uint16_t idx;
	int n;

	vq = sc->vsd_ctrl_tx;

	if (!vq_has_descs(vq))
		return;

	n = vq_getchain(vq, &idx, &iov, 1, NULL);

	assert (n == 1);

	memcpy(iov.iov_base, ctrl, sizeof(struct pci_vtdtr_control));
	len = iov.iov_len;

	vq_relchain(vq, idx, len);
	vq_endchains(vq, 1);
}

struct pci_devemu pci_de_vdtr = {
	.pe_emu		= "virtio-dtrace",
	.pe_init	= pci_vtdtr_init,
	.pe_barwrite	= vi_pci_write,
	.pe_barread	= vi_pci_read
};
PCI_EMUL_SET(pci_de_vdtr);
