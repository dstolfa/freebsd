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

#include <sys/module.h>
#include <sys/taskqueue.h>

#include <sys/dtrace.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virqueue.h>
#include <dev/virtio/dtrace/virtio_dtrace.h>

struct vtdtr_probe {
	uint32_t			vtdprobe_id;
	SLIST_ENTRY(vtdtr_probe)	vtdprobe_next;
}

struct vtdtr_softc {
	device_t			vtdtr_dev;
	struct mtx			vtdtr_mtx;
	uint64_t			vtdtr_features;
	uint32_t			vtdtr_flags;
#define	VTDTR_FLAG_DETACHED	0x01
#define	VTDTR_FLAG_PROBEACTION	0x02
#define	VTDTR_FLAG_PROVACTION	0x04

	struct task			 vtdtr_ctrl_task;
	struct virtqueue		*vtdtr_ctrl_rxvq;
	struct virtqueue		*vtdtr_ctrl_txvq;
	struct mtx			 vtdtr_ctrl_tx_mtx;

	/*
	 * We need to keep track of all the enabled probes in the
	 * driver in order to act as a bridge between the guest DTrace
	 * and host DTrace. The driver is the one asking to install
	 * or uninstall the probes on the guest, as instructed by host.
	 */
	SLIST_HEAD(, vtdtr_probe)	vtdtr_enabled_probes;
	struct mtx			vtdtr_probe_list_mtx;
};

static device_method_t vtdtr_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtdtr_probe),
	DEVMETHOD(device_attach,	vtdtr_attach),
	DEVMETHOD(device_detach,	vtdtr_detach),

	/* VirtIO methods. */
	DEVMETHOD(virtio_config_change,	vtdtr_config_change),

	DEVMETHOD_END
};

#define	VTDTR_FEATURES	VIRTIO_DTRACE_F_PROBE

static driver_t vtcon_driver = {
	"vtdtr",
	vtdtr_methods,
	sizeof(struct vtdtr_softc)
};
static devclass_t vtdtr_devclass;

DRIVER_MODULE(virtio_dtrace, virtio_pci, vtdtr_driver, vtdtr_devclass,
    vtdtr_modevent, 0);
MODULE_VERSION(virtio_dtrace, 1);
MODULE_DEPEND(virtio_dtrace, virtio, 1, 1, 1);
MODULE_DEPEND(virtio_dtrace, dtrace, 1, 1, 1);

static struct virtio_feature_desc vtdtr_feature_desc[] = {
	{ VIRTIO_DTRACE_F_PROBE,	"ProbeModifyingActions"    },
	{ VIRTIO_DTRACE_F_PROV,		"ProviderModifyingActions" },

	{ 0, NULL }
};

static int	vtdtr_modevent(module_t, int, void *);
static void	vtdtr_cleanup(void);

static int	vtdtr_probe(device_t);
static int	vtdtr_attach(device_t);
static int	vtdtr_detach(device_t);
static int	vtdtr_config_change(device_t);
static void	vtdtr_negotiate_features(struct vtdtr_softc *);
static void	vtdtr_setup_features(struct vtdtr_softc *);
static int	vtdtr_alloc_probelist(struct vtdtr_softc *);
static int	vtdtr_alloc_virtqueues(struct vtdtr_softc *);
static void	vtdtr_ctrl_task_act(void *, int);
static int	vtdtr_ctrl_init(struct vtdtr_softc *);
static void	vtdtr_enable_interrupts(struct vtdtr_softc *);
static void 	vtdtr_ctrl_send_control(struct vtdtr_softc *, uint32_t,
    uint16_t, uint16_t);
static void	vtdtr_detach(device_t);
static void	vtdtr_stop(struct softc *);
static void	vtdtr_ctrl_deinit(struct softc *);
static void	vtdtr_destroy_probelist(struct softc *);

static int
vtdtr_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		error = 0;
		break;
	case MOD_QUIESCE:
		error = 0;
		break;
	case MOD_UNLOAD:
		vtdtr_cleanup();
		error = 0;
		break;
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static void
vtdtr_cleanup(void)
{
	/*
	 * Perhaps to be used for cleaning up the module unloading later on. For
	 * now, the VMM handles that.
	 */
}

static int
vtdtr_probe(device_t dev)
{
	if (virtio_get_device_type(dev) != VIRTIO_ID_DTRACE)
		return (ENXIO);

	device_set_desc(dev, "VirtIO DTrace driver");

	return (BUS_PROBE_DEFAULT);
}

static int
vtdtr_attach(device_t dev)
{
	struct vtdtr_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vtdtr_dev = dev;

	mtx_init(&sc->vtdtr_mtx, "vtdtrmtx", NULL, MTX_DEF);
	mtx_init(&sc->vtdtr_ctrl_tx_mtx, "vtdtrctrlmtx", NULL, MTX_DEF);

	virtio_set_feature_desc(dev, vtdtr_feature_desc);
	vtdtr_setup_features(sc);

	error = vtdtr_alloc_probelist(sc);
	if (error) {
		device_printf(dev, "cannot allocate softc probe list\n");
		goto fail;
	}

	error = vtdtr_alloc_virtqueues(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueues\n");
		goto fail;
	}

	/*
	 * Here we set up the handlers for each of the features(actions) that
	 * are supported
	 */
	if (sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION ||
	    sc->vtdtr_flags & VTDTR_FLAG_PROVACTION)
		TASK_INIT(&sc->vtdtr_ctrl_task, 0, vtdtr_ctrl_task_act, sc);

	error = vtdtr_ctrl_init(sc);
	if (error)
		goto fail;

	error = virtio_setup_intr(dev, INTR_TYPE_DTRACE);
	if (error) {
		device_printf(dev, "cannot set up virtqueue interrupts\n");
		goto fail;
	}

	vtdtr_enable_interrupts(sc);
	vtdtr_ctrl_send_control(sc, VIRTIO_DTRACE_BAD_ID,
	    VIRTIO_DTRACE_DEVICE_READY, 1);
fail:
	if (error)
		vtdtr_detach(dev);

	return (error);
}

static int
vtdtr_detach(device_t dev)
{
	struct vtdtr_softc *sc;

	sc = device_get_softc(dev);

	VTDTR_LOCK(sc);
	sc->vtdtr_flags |= VTDTR_FLAG_DETACHED;
	if (device_is_attached(dev))
		vtdtr_stop(sc);
	VTDTR_UNLOCK(sc);

	if (sc->vtdtr_flags & VTDTR_FLAG_DTRACEACTION) {
		taskqueue_drain(taskqueue_thread, &sc->vtdtr_ctrl_task);
		vtdtr_ctrl_deinit(sc);
	}

	vtdtr_destroy_probelist(sc);
	mtx_destroy(&sc->vtdtr_mtx);
	mtx_destroy(&sc->vtdtr_ctrl_tx_mtx);

	return (0);
}

static int
vtdtr_config_change(device_t dev)
{
	/*
	 * There is no reason to change the configuration yet.
	 */
}

static void
vtdtr_negotiate_features(struct vtdtr_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtdtr_dev;
	features = VTDTR_FEATURES;

	sc->vtdtr_features = virtio_negotiate_features(dev, features);
}

static void
vtdtr_setup_features(struct vtdtr_softc *sc)
{
	device_t dev;

	dev = sc->vtdtr_dev;

	vtdtr_negotiate_features(sc);

	if (virtio_with_feature(dev, VIRTIO_DTRACE_F_PROBE))
		sc->vtdtr_flags |= VTDTR_FLAG_PROBEACTION;
	if (virtio_with_feature(dev, VIRTIO_DTRACE_F_PROV))
		sc->vtdtr_flags |= VTDTR_FLAG_PROVACTION;
}

static void
vtdtr_alloc_probelist(struct softc *sc)
{
	/*
	 * Initialize the list here
	 * SLIST_INIT
	 */
	return (0);
}
