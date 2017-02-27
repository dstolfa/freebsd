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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sglist.h>
#include <sys/taskqueue.h>
#include <sys/queue.h>

#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

/*
 * DTrace communication layer
 */
#include <sys/dtvirt.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/dtrace/virtio_dtrace.h>

#include "virtio_if.h"

#define	VTDTR_BULK_BUFSZ	128

struct vtdtr_probe {
	uint32_t			vtdprobe_id;
	SLIST_ENTRY(vtdtr_probe)	vtdprobe_next;
};

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

/*
 * TODO:
 *  - Event handler
 *  - Bidirectional flow
 *  - Routines to interact(read and write to virtqueues)
 */

#define	VTDTR_LOCK(__sc)		(mtx_lock(&((__sc)->vtdtr_mtx)))
#define	VTDTR_UNLOCK(__sc)		(mtx_unlock(&((__sc)->vtdtr_mtx)))
#define	VTDTR_LOCK_ASSERT(__sc) \
    (mtx_assert(&((__sc)->vtdtr_mtx), MA_OWNED))
#define	VTDTR_LOCK_ASSERT_NOTOWNED(__sc) \
    (mtx_assert(&((__sc)->vtdtr_mtx), MA_NOTOWNED))
#define	VTDTR_CTRL_TX_LOCK(__sc)	(mtx_lock(&((__sc)->vtdtr_ctrl_tx_mtx)))
#define	VTDTR_CTRL_TX_UNLOCK(__sc)	(mtx_unlock(&((__sc)->vtdtr_ctrl_tx_mtx)))
#define	VTDTR_CTRL_TX_LOCK_ASSERT(__sc) \
    (mtx_assert(&((__sc)->vtdtr_ctrl_tx_mtx), MA_OWNED))
#define	VTDTR_CTRL_TX_LOCK_ASSERT_NOTOWNED(__sc) \
    (mtx_assert(&((__sc)->vtdtr_ctrl_tx_mtx), MA_NOTOWNED))

static MALLOC_DEFINE(M_VIRTIO_DTRACE, "dtvirtio", "DTrace VirtIO");

static int	vtdtr_modevent(module_t, int, void *);
static void	vtdtr_cleanup(void);

static int	vtdtr_probe(device_t);
static int	vtdtr_attach(device_t);
static int	vtdtr_detach(device_t);
static int	vtdtr_config_change(device_t);
static void	vtdtr_negotiate_features(struct vtdtr_softc *);
static void	vtdtr_setup_features(struct vtdtr_softc *);
static void	vtdtr_alloc_probelist(struct vtdtr_softc *);
static int	vtdtr_alloc_virtqueues(struct vtdtr_softc *);
static void	vtdtr_stop(struct vtdtr_softc *);
static int	vtdtr_ctrl_event_enqueue(struct vtdtr_softc *,
         	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_event_create(struct vtdtr_softc *);
static void	vtdtr_ctrl_event_requeue(struct vtdtr_softc *,
         	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_event_populate(struct vtdtr_softc *);
static void	vtdtr_ctrl_event_drain(struct vtdtr_softc *);
static int	vtdtr_ctrl_init(struct vtdtr_softc *);
static void	vtdtr_ctrl_deinit(struct vtdtr_softc *);
static void	vtdtr_ctrl_event_intr(void *);
static void	vtdtr_ctrl_task_act(void *, int);
static void	vtdtr_enable_interrupts(struct vtdtr_softc *);
static void	vtdtr_disable_interrupts(struct vtdtr_softc *);
static void	vtdtr_ctrl_process_event(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static void	vtdtr_ctrl_process_provaction(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static void	vtdtr_ctrl_process_selfdestroy();
static void	vtdtr_ctrl_process_probeaction(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static void	vtdtr_ctrl_send_control(struct vtdtr_softc *, uint32_t, uint32_t);
static void	vtdtr_destroy_probelist(struct vtdtr_softc *);

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

static driver_t vtdtr_driver = {
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

	vtdtr_alloc_probelist(sc);

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
	vtdtr_ctrl_send_control(sc, VIRTIO_DTRACE_DEVICE_READY, 1);
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

	if (sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION ||
	    sc->vtdtr_flags & VTDTR_FLAG_PROVACTION) {
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
	return (0);
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

static __inline void
vtdtr_alloc_probelist(struct vtdtr_softc *sc)
{
	SLIST_INIT(&sc->vtdtr_enabled_probes);
}

static int
vtdtr_alloc_virtqueues(struct vtdtr_softc *sc)
{
	device_t dev;
	struct vq_alloc_info *info;
	int error;

	dev = sc->vtdtr_dev;

	info = malloc(sizeof(struct vq_alloc_info), M_TEMP, M_NOWAIT);
	if (info == NULL)
		return (ENOMEM);

	VQ_ALLOC_INFO_INIT(&info[0], 0, vtdtr_ctrl_event_intr, sc, &sc->vtdtr_ctrl_rxvq,
	    "%s-control RX", device_get_nameunit(dev));
	VQ_ALLOC_INFO_INIT(&info[1], 0, NULL, sc, &sc->vtdtr_ctrl_txvq,
	    "%s-control TX", device_get_nameunit(dev));

	error = virtio_alloc_virtqueues(dev, 0, 2, info);
	free(info, M_TEMP);

	return (error);
}

static void
vtdtr_stop(struct vtdtr_softc *sc)
{
	vtdtr_disable_interrupts(sc);
	virtio_stop(sc->vtdtr_dev);
}

static int
vtdtr_ctrl_event_enqueue(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	struct sglist_seg segs[2];
	struct sglist sg;
	struct virtqueue *vq;
	int error;

	vq = sc->vtdtr_ctrl_rxvq;

	sglist_init(&sg, 2, segs);
	error = sglist_append(&sg, ctrl,
	    sizeof(struct virtio_dtrace_control) + VTDTR_BULK_BUFSZ);
	KASSERT(error == 0,
	    ("%s: error %d adding control to sglist", __func__, error));

	return (virtqueue_enqueue(vq, ctrl, &sg, 0, sg.sg_nseg));
}

static int
vtdtr_ctrl_event_create(struct vtdtr_softc *sc)
{
	struct virtio_dtrace_control *ctrl;
	int error;

	ctrl = malloc(sizeof(struct virtio_dtrace_control) + VTDTR_BULK_BUFSZ,
	    M_DEVBUF, M_ZERO | M_NOWAIT);

	if (ctrl == NULL)
		return (ENOMEM);

	error = vtdtr_ctrl_event_enqueue(sc, ctrl);
	if (error)
		free(ctrl, M_DEVBUF);

	return (error);
}

static void
vtdtr_ctrl_event_requeue(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	int error;

	bzero(ctrl, sizeof(struct virtio_dtrace_control) + VTDTR_BULK_BUFSZ);
	error = vtdtr_ctrl_event_enqueue(sc, ctrl);
	KASSERT(error == 0, ("%s: cannot requeue control buffer %d",
	    __func__, error));
}

static int
vtdtr_ctrl_event_populate(struct vtdtr_softc *sc)
{
	struct virtqueue *vq;
	int error, nbufs;

	vq = sc->vtdtr_ctrl_rxvq;
	error = ENOSPC;

	for (nbufs = 0; !virtqueue_full(vq); nbufs++) {
		error = vtdtr_ctrl_event_create(sc);
		if (error)
			break;
	}

	if (nbufs > 0) {
		virtqueue_notify(vq);
		error = 0;
	}

	return (error);
}

static void
vtdtr_ctrl_event_drain(struct vtdtr_softc *sc)
{
	struct virtio_dtrace_control *ctrl;
	struct virtqueue *vq;
	int last;

	vq = sc->vtdtr_ctrl_rxvq;
	last = 0;

	if (vq == NULL)
		return;

	VTDTR_LOCK(sc);

	while ((ctrl = virtqueue_drain(vq, &last)) != NULL)
		free(ctrl, M_DEVBUF);

	VTDTR_UNLOCK(sc);
}

static __inline int
vtdtr_ctrl_init(struct vtdtr_softc *sc)
{
	return (vtdtr_ctrl_event_populate(sc));
}

static __inline void
vtdtr_ctrl_deinit(struct vtdtr_softc *sc)
{
	vtdtr_ctrl_event_drain(sc);
}

static void
vtdtr_ctrl_event_intr(void *xsc)
{
	struct vtdtr_softc *sc;
	sc = xsc;

	/*
	 * XXX: It might be a good idea to use the DTrace taskqueue here, but it
	 * would require intertwining between the virtio driver and the DTrace
	 * framework itself
	 */
	taskqueue_enqueue(taskqueue_thread, &sc->vtdtr_ctrl_task);
}

static void
vtdtr_ctrl_task_act(void *xsc, int nprobes)
{
	struct vtdtr_softc *sc;
	struct virtqueue *vq;
	struct virtio_dtrace_control *ctrl;
	uint32_t len;

	sc = xsc;
	vq = sc->vtdtr_ctrl_rxvq;
	len = 0;

	VTDTR_LOCK(sc);

	while (nprobes-- > 0) {
		ctrl = virtqueue_dequeue(vq, &len);
		if (ctrl == NULL)
			break;

		VTDTR_UNLOCK(sc);
		vtdtr_ctrl_process_event(sc, ctrl);
		VTDTR_LOCK(sc);

		vtdtr_ctrl_event_requeue(sc, ctrl);
	}

	VTDTR_UNLOCK(sc);
}

static __inline void
vtdtr_enable_interrupts(struct vtdtr_softc *sc)
{
	VTDTR_LOCK(sc);
	virtqueue_enable_intr(sc->vtdtr_ctrl_rxvq);
	VTDTR_UNLOCK(sc);
}

static __inline void
vtdtr_disable_interrupts(struct vtdtr_softc *sc)
{
	VTDTR_LOCK_ASSERT(sc);
	virtqueue_disable_intr(sc->vtdtr_ctrl_rxvq);
}

static void
vtdtr_ctrl_process_event(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	device_t dev;

	dev = sc->vtdtr_dev;

	/*
	 * XXX: Double switch statement... meh.
	 */
	switch (ctrl->event) {
	case VIRTIO_DTRACE_REGISTER:
	case VIRTIO_DTRACE_UNREGISTER:
		if (sc->vtdtr_flags & VTDTR_FLAG_PROVACTION)
			vtdtr_ctrl_process_provaction(sc, ctrl);
		break;
	case VIRTIO_DTRACE_DESTROY:
		vtdtr_ctrl_process_selfdestroy(sc, ctrl);
		break;
	case VIRTIO_DTRACE_PROBE_CREATE:
	case VIRTIO_DTRACE_PROBE_INSTALL:
	case VIRTIO_DTRACE_PROBE_UNINSTALL:
		if (sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION)
			vtdtr_ctrl_process_probeaction(sc, ctrl);
		break;
	}
}

static void
vtdtr_ctrl_process_provaction(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	int error;
	/*
	 * XXX: What is the "value" parameter in this context?
	 */
	switch (ctrl->event) {
	case VIRTIO_DTRACE_REGISTER:
		error = dtvirt_register(ctrl->value, ctrl->info);
		KASSERT(error == 0,
		    ("%s: error %d registering provider in dtvirt",
		    __func__, error));
		break;
	case VIRTIO_DTRACE_UNREGISTER:
		error = dtvirt_unregister(ctrl->value);
		KASSERT(error == 0,
		    ("%s: error %d unregistering provider in dtvirt",
		    __func__, error));
		break;
	}
}

static void
vtdtr_ctrl_process_selfdestroy()
{
	int error;
	error = dtvirt_selfdestroy();
	KASSERT(error == 0, ("%s: error %d self-destroying",
	    __func__, error));
}

static void
vtdtr_ctrl_process_probeaction(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	int error;
	/*
	 * TODO: Implement the functionality
	 */
	switch (ctrl->event) {
	case VIRTIO_DTRACE_PROBE_CREATE:
		/*
		 * The info here relates to the probe specification
		 */
		error = dtvirt_probe_create(ctrl->value, ctrl->info);
		KASSERT(error == 0, ("%s: error %d creating probe",
		    __func__, error));
		break;
	case VIRTIO_DTRACE_PROBE_INSTALL:
		/*
		 * The ctrl->info here related to the DIF that might want
		 * to be uploaded in the future.
		 */
		error = dtvirt_probe_install(ctrl->value, ctrl->info);
		KASSERT(error == 0, ("%s: error %d installing probe",
		    __func__, error));
		break;
	case VIRTIO_DTRACE_PROBE_UNINSTALL:
		error = dtvirt_probe_uninstall(ctrl->value);
		KASSERT(error == 0, ("%s: error %d uninstalling probe",
		    __func__, error));
		break;
	}
}

static void
vtdtr_ctrl_poll(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	struct sglist_seg segs[2];
	struct sglist sg;
	struct virtqueue *vq;
	int error;

	vq = sc->vtdtr_ctrl_txvq;

	sglist_init(&sg, 2, segs);
	error = sglist_append(&sg, ctrl, sizeof(struct virtio_dtrace_control));
	KASSERT(error == 0,
	    ("%s: error %d adding control to sglist", __func__, error));

	/*
	 * XXX: Is this correct?
	 *
	 * There does not seem to be a good reason to lock the entire device,
	 * only the TX virtqueue. The reasoning is that we do not care whether
	 * we delegate processing requests, such as probe installing or
	 * uninstalling, provider registration and similar operations to DTrace
	 * while we are responding to a message or when we notify that we are
	 * shutting down.
	 */
	VTDTR_CTRL_TX_LOCK(sc);
	KASSERT(sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION ||
	    sc->vtdtr_flags & VTDTR_FLAG_PROVACTION,
	    ("%s: PROBEACTION || PROVACTION is not negotiated", __func__));

	if (!virtqueue_empty(vq))
		return;
	if (virtqueue_enqueue(vq, ctrl, &sg, sg.sg_nseg, 0) != 0)
		return;

	virtqueue_notify(vq);
	virtqueue_poll(vq, NULL);
	VTDTR_CTRL_TX_UNLOCK(sc);
}

static void
vtdtr_ctrl_send_control(struct vtdtr_softc *sc, uint32_t event, uint32_t value)
{
	struct virtio_dtrace_control ctrl;

	if (((sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION) |
	     (sc->vtdtr_flags & VTDTR_FLAG_PROVACTION)) == 0)
		return;

	ctrl.event = event;
	ctrl.value = value;

	vtdtr_ctrl_poll(sc, &ctrl);
}

static void
vtdtr_destroy_probelist(struct vtdtr_softc *sc)
{
	struct vtdtr_probe *tmp = NULL;
	while (!SLIST_EMPTY(&sc->vtdtr_enabled_probes)) {
		tmp = SLIST_FIRST(&sc->vtdtr_enabled_probes);
		SLIST_REMOVE_HEAD(&sc->vtdtr_enabled_probes, vtdprobe_next);
		free(tmp, M_VIRTIO_DTRACE);
	}
}
