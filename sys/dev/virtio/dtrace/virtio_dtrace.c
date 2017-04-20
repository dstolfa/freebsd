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
#include <sys/dtrace.h>
#include <sys/dtrace_bsd.h>
*/

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/dtrace/virtio_dtrace.h>

#include "virtio_if.h"

#define	VTDTR_BULK_BUFSZ	128

struct vtdtr_pb {
	uint32_t				vtdprobe_id;
	SLIST_ENTRY(vtdtr_pb)			vtdprobe_next;
};

struct vtdtr_softc {
	device_t				vtdtr_dev;
	struct mtx				vtdtr_mtx;
	uint64_t				vtdtr_features;
	uint32_t				vtdtr_flags;
#define	VTDTR_FLAG_DETACHED	0x01
#define	VTDTR_FLAG_PROBEACTION	0x02
#define	VTDTR_FLAG_PROVACTION	0x04

	struct virtio_dtrace_queue		vtdtr_txq;
	struct virtio_dtrace_queue		vtdtr_rxq;
	int					vtdtr_tx_nseg;
	int					vtdtr_rx_nseg;

	/*
	 * We need to keep track of all the enabled probes in the
	 * driver in order to act as a bridge between the guest DTrace
	 * and host DTrace. The driver is the one asking to install
	 * or uninstall the probes on the guest, as instructed by host.
	 */
	SLIST_HEAD(, vtdtr_pb)			vtdtr_enabled_probes;
	struct mtx				vtdtr_probe_list_mtx;
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
static void	vtdtr_drain_virtqueues(struct vtdtr_softc *);
static int	vtdtr_queue_populate(struct virtio_dtrace_queue *);
static int	vtdtr_queue_enqueue_ctrl(struct virtio_dtrace_queue *,
          	    struct virtio_dtrace_control *, int, int);
static void	vtdtr_queue_requeue_ctrl(struct virtio_dtrace_queue *,
           	    struct virtio_dtrace_control *, int, int);
static int	vtdtr_queue_new_ctrl(struct virtio_dtrace_queue *);

static int	vtdtr_enable_interrupts(struct vtdtr_softc *);
static void	vtdtr_disable_interrupts(struct vtdtr_softc *);
static int	vtdtr_ctrl_process_event(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_provaction(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static void	vtdtr_ctrl_process_selfdestroy(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_probeaction(struct vtdtr_softc *,
           	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_prov_register(struct vtdtr_softc *,
          	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_prov_unregister(struct vtdtr_softc *,
          	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_probe_create(struct vtdtr_softc *,
          	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_probe_install(struct vtdtr_softc *,
          	    struct virtio_dtrace_control *);
static int	vtdtr_ctrl_process_probe_uninstall(struct vtdtr_softc *,
          	    struct virtio_dtrace_control *);
static void	vtdtr_queue_send_ctrl(struct virtio_dtrace_queue *,
           	    uint32_t, uint32_t);
static void	vtdtr_destroy_probelist(struct vtdtr_softc *);
static void	vtdtr_start_taskqueues(struct vtdtr_softc *);
static void	vtdtr_free_taskqueues(struct vtdtr_softc *);
static void	vtdtr_drain_taskqueues(struct vtdtr_softc *);
static int	vtdtr_vq_enable_intr(struct virtio_dtrace_queue *);
static void	vtdtr_vq_disable_intr(struct virtio_dtrace_queue *);
static void	vtdtr_tq_start(struct virtio_dtrace_queue *);
static void	vtdtr_txq_tq_intr(void *, int);
static void	vtdtr_rxq_tq_intr(void *, int);
static void	vtdtr_rxq_vq_intr(void *);
static void	vtdtr_txq_vq_intr(void *);
static int	vtdtr_init_txq(struct vtdtr_softc *, int);
static int	vtdtr_init_rxq(struct vtdtr_softc *, int);
static void	vtdtr_destroy_rxq(struct vtdtr_softc *, int);
static void	vtdtr_destroy_txq(struct vtdtr_softc *, int);

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

/*
 * These are the attributes for every arbitrarily created provider
static dtrace_pattr_t virtio_dtrace_attr = {
{ DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL },
{ DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL },
{ DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL },
{ DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL },
{ DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL, DTRACE_STABILITY_EXTERNAL }
};
 */

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

/*
 * Performs all of the initialization in the module.
 * The following values get assigned:
 *     sc                       <- device softc
 *     sc->dev                  <- dev
 *     sc->vtdtr_mtx            <- MTX_DEF
 *     sc->vtdtr_txq            <- vtdtr_txq_init()
 *     sc->vtdtr_rxq            <- vtdtr_rxq_init()
 *     sc->vtdtr_enabled_probes <- vtdtr_alloc_probelist()
 *
 * This function also sets up the interrupt type on
 * the driver, as well as enables interrupts.
 * As it returns, it also sends the control message
 * to the userspace PCI device on the host that the
 * driver is ready.
 */
static int
vtdtr_attach(device_t dev)
{
	struct vtdtr_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vtdtr_dev = dev;
	sc->vtdtr_rx_nseg = 1;
	sc->vtdtr_tx_nseg = 1;

	mtx_init(&sc->vtdtr_mtx, "vtdtrmtx", NULL, MTX_DEF);

	virtio_set_feature_desc(dev, vtdtr_feature_desc);
	vtdtr_setup_features(sc);

	vtdtr_alloc_probelist(sc);

	error = vtdtr_init_rxq(sc, 0);
	if (error) {
		device_printf(dev, "cannot initialize RX queue\n");
		goto fail;
	}
	
	error = vtdtr_init_txq(sc, 0);
	if (error) {
		device_printf(dev, "cannot initialize TX queue\n");
		goto fail;
	}
	
	error = vtdtr_alloc_virtqueues(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueues\n");
		goto fail;
	}

	device_printf(dev, "txq = %s, rxq = %s\n",
	    sc->vtdtr_txq.vtdq_name, sc->vtdtr_rxq.vtdq_name);

	error = vtdtr_queue_populate(&sc->vtdtr_rxq);
	if (error) {
		device_printf(dev, "cannot populate %s\n",
		    sc->vtdtr_rxq.vtdq_name);
		goto fail;
	}

	if (error) {
		device_printf(dev, "cannot populate %s\n",
		    sc->vtdtr_txq.vtdq_name);
		goto fail;
	}

	error = virtio_setup_intr(dev, INTR_TYPE_MISC);
	if (error) {
		device_printf(dev, "cannot set up virtio interrupts\n");
		goto fail;
	}

	vtdtr_enable_interrupts(sc);
	/*
	if (error)
		error = 0;
	else {
		device_printf(dev, "failed to enable the interrupts for virtqueues\n");
		error = ENXIO;
	}
	*/
	vtdtr_queue_send_ctrl(&sc->vtdtr_txq, VIRTIO_DTRACE_DEVICE_READY, 1);
fail:
	if (error)
		vtdtr_detach(dev);

	return (error);
}

/*
 * Here we free all the used memory in the
 * driver, drain all the taskqeueues and
 * destroy all the mutexes.
 */
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
		vtdtr_drain_taskqueues(sc);
		vtdtr_drain_virtqueues(sc);
	}

	vtdtr_destroy_probelist(sc);
	vtdtr_destroy_rxq(sc, 0);
	vtdtr_destroy_txq(sc, 0);
	mtx_destroy(&sc->vtdtr_mtx);
	
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

/*
 * Wrapper function for virtio_negotiate_features()
 */
static void
vtdtr_negotiate_features(struct vtdtr_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtdtr_dev;
	features = VTDTR_FEATURES;

	sc->vtdtr_features = virtio_negotiate_features(dev, features);
}

/*
 * Set up the features that this driver can handle
 */
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

/*
 * Allocate all the virtqueues.
 */
static int
vtdtr_alloc_virtqueues(struct vtdtr_softc *sc)
{
	device_t dev;
	struct vq_alloc_info *info;
	struct virtio_dtrace_queue *rxq, *txq;
	int error;

	dev = sc->vtdtr_dev;
	rxq = &sc->vtdtr_rxq;
	txq = &sc->vtdtr_txq;
	rxq->vtdq_vqintr = vtdtr_rxq_vq_intr;
	txq->vtdq_vqintr = vtdtr_txq_vq_intr;
	
	info = malloc(sizeof(struct vq_alloc_info), M_TEMP, M_NOWAIT);
	if (info == NULL)
		return (ENOMEM);

	VQ_ALLOC_INFO_INIT(&info[0], sc->vtdtr_rx_nseg, rxq->vtdq_vqintr, sc,
	    &rxq->vtdq_vq, "%s-control RX", device_get_nameunit(dev));
	VQ_ALLOC_INFO_INIT(&info[1], sc->vtdtr_tx_nseg, txq->vtdq_vqintr, sc,
	    &txq->vtdq_vq, "%s-control TX", device_get_nameunit(dev));

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

static void
vtdtr_queue_requeue_ctrl(struct virtio_dtrace_queue *q,
    struct virtio_dtrace_control *ctrl, int readable, int writable)
{
	int error;

	bzero(ctrl, sizeof(struct virtio_dtrace_control) + VTDTR_BULK_BUFSZ);
	error = vtdtr_queue_enqueue_ctrl(q, ctrl, readable, writable);
	KASSERT(error == 0, ("%s: cannot requeue control buffer %d",
	    __func__, error));
}

static int
vtdtr_queue_populate(struct virtio_dtrace_queue *q)
{
	struct virtqueue *vq;
	int nbufs;
	int error;

	vq = q->vtdq_vq;
	error = ENOSPC;

	for (nbufs = 0; !virtqueue_full(vq); nbufs++) {
		error = vtdtr_queue_new_ctrl(q);
		if (error)
			break;
	}

	if (nbufs > 0) {
		virtqueue_notify(vq);
		error = 0;
	}

	return (error);
}

static int
vtdtr_queue_new_ctrl(struct virtio_dtrace_queue *q)
{
	struct vtdtr_softc *sc;
	struct virtio_dtrace_control *ctrl;
	int error;

	sc = q->vtdq_sc;
	ctrl = malloc(sizeof(struct virtio_dtrace_control),
	    M_VIRTIO_DTRACE, M_NOWAIT | M_ZERO);
	if (ctrl == NULL)
		return (ENOMEM);

	error = vtdtr_queue_enqueue_ctrl(q, ctrl, 0, 1);
	if (error)
		free(ctrl, M_VIRTIO_DTRACE);

	return (error);
}

static int
vtdtr_queue_enqueue_ctrl(struct virtio_dtrace_queue *q,
    struct virtio_dtrace_control *ctrl, int readable, int writable)
{
	struct sglist_seg seg;
	struct sglist sg;
	struct virtqueue *vq;
	int error;

	vq = q->vtdq_vq;

	sglist_init(&sg, 1, &seg);
	error = sglist_append(&sg, ctrl, sizeof(struct virtio_dtrace_control));
	KASSERT(error == 0, ("%s: error %d adding control to sglist",
	    __func__, error));
	
	return (virtqueue_enqueue(vq, ctrl, &sg, readable, writable));
}

static int
vtdtr_ctrl_process_event(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	device_t dev;
	int error;
	
	dev = sc->vtdtr_dev;
	error = 0;

	/*
	 * XXX: Double switch statement... meh.
	 */
	switch (ctrl->event) {
	case VIRTIO_DTRACE_REGISTER:
	case VIRTIO_DTRACE_UNREGISTER:
		if (sc->vtdtr_flags & VTDTR_FLAG_PROVACTION)
			error = vtdtr_ctrl_process_provaction(sc, ctrl);
		break;
	case VIRTIO_DTRACE_DESTROY:
		vtdtr_ctrl_process_selfdestroy(sc, ctrl);
		break;
	case VIRTIO_DTRACE_PROBE_CREATE:
	case VIRTIO_DTRACE_PROBE_INSTALL:
	case VIRTIO_DTRACE_PROBE_UNINSTALL:
		if (sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION)
			error = vtdtr_ctrl_process_probeaction(sc, ctrl);
		break;
	}

	return (error);
}

static int
vtdtr_ctrl_process_provaction(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	int error;
	/*
	 * XXX: What is the "value" parameter in this context?
	 */
	switch (ctrl->event) {
	case VIRTIO_DTRACE_REGISTER:
		error = vtdtr_ctrl_process_prov_register(sc, ctrl);
		break;
	case VIRTIO_DTRACE_UNREGISTER:
		error = vtdtr_ctrl_process_prov_unregister(sc, ctrl);
		break;
	}

	return (error);
}

static void
vtdtr_ctrl_process_selfdestroy(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	int error;
	error = 0;
	/*
	 * NOT SUPPORTED YET
	 */
	KASSERT(error == 0, ("%s: error %d self-destroying",
	    __func__, error));
}

static int
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
		error = vtdtr_ctrl_process_probe_create(sc, ctrl);
		break;
	case VIRTIO_DTRACE_PROBE_INSTALL:
		/*
		 * The ctrl->info here related to the DIF that might want
		 * to be uploaded in the future.
		 */
		error = vtdtr_ctrl_process_probe_install(sc, ctrl);
		break;
	case VIRTIO_DTRACE_PROBE_UNINSTALL:
		error = vtdtr_ctrl_process_probe_uninstall(sc, ctrl);
		break;
	}

	return (error);
}

/*
 * TODO
 */
static int
vtdtr_ctrl_process_prov_register(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	/*
	dtrace_provider_id_t id;
	char *provname;
	*/
	int error;

	/* provname = ctrl->info; */
	error = 0;
	/*
	error = dtrace_register(provname, &virtio_dtrace_attr, virtio_dtrace_priv,
	    virtio_dtrace_cr, virtio_dtrace_pops, NULL, &id);
	*/
	return (error);
}

/*
 * TODO
 */
static int
vtdtr_ctrl_process_prov_unregister(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	return (0);
}

/*
 * TODO
 */
static int
vtdtr_ctrl_process_probe_create(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	return (0);
}

/*
 * This function is responsible for processing the
 * probe installation event. It calls out to the
 * DTrace framework and adds the said probe to the
 * front of the list of the enabled probes.
 */
static int
vtdtr_ctrl_process_probe_install(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl /* UNUSED */)
{
	struct vtdtr_pb *probe;

	probe = malloc(sizeof(struct vtdtr_pb), M_VIRTIO_DTRACE, M_NOWAIT | M_ZERO);
	if (probe == NULL)
		return (ENOMEM);
	probe->vtdprobe_id = ctrl->value;
	SLIST_INSERT_HEAD(&sc->vtdtr_enabled_probes, probe, vtdprobe_next);
/*	dtrace_probeid_disable(probe->vtdprobe_id); */
	return (0);
}

/*
 * This function is responsible for processing the
 * probe uninstallation event. It calls out to the
 * DTrace framework and removes the said probe from
 * the list of enabled probes. 
 */
static int
vtdtr_ctrl_process_probe_uninstall(struct vtdtr_softc *sc,
    struct virtio_dtrace_control *ctrl)
{
	struct vtdtr_pb *probe;
	struct vtdtr_pb *ptmp;

	/*dtrace_probeid_enable(ctrl->value); */

	SLIST_FOREACH_SAFE(probe, &sc->vtdtr_enabled_probes, vtdprobe_next, ptmp) {
		if (probe->vtdprobe_id == ctrl->value)
			SLIST_REMOVE(&sc->vtdtr_enabled_probes, probe,
			    vtdtr_pb, vtdprobe_next);
	}

	return (0);
}

/*
 * The purpose of this function is to pack the
 * control event and send it through a virtqueue
 * to the host PCI device. 
 */
static void
vtdtr_queue_ctrl_poll(struct virtio_dtrace_queue *txq,
    struct virtio_dtrace_control *ctrl)
{
	struct vtdtr_softc *sc;
	struct virtqueue *vq;
	device_t dev;
	int error;

	sc = txq->vtdq_sc;
	dev = sc->vtdtr_dev;
	vq = txq->vtdq_vq;


	VTDTR_QUEUE_LOCK(txq);

	error = vtdtr_queue_enqueue_ctrl(txq, ctrl, 1, 0);
	
	if (error == 0) {
		virtqueue_notify(vq);
		virtqueue_poll(vq, NULL);
	} else {
		device_printf(dev, "Failed to poll:\n"
		    "ctrl->event = %d\n"
		    "ctrl->value = %d\n",
		    ctrl->event, ctrl->value);
	}
	VTDTR_QUEUE_UNLOCK(txq);
}

/*
 * Send a control event to the host PCI device.
 */
static void
vtdtr_queue_send_ctrl(struct virtio_dtrace_queue *txq,
    uint32_t event, uint32_t value)
{
	struct virtio_dtrace_control ctrl;
	struct vtdtr_softc *sc;

	sc = txq->vtdq_sc;

	if (((sc->vtdtr_flags & VTDTR_FLAG_PROBEACTION) |
	     (sc->vtdtr_flags & VTDTR_FLAG_PROVACTION)) == 0)
		return;

	ctrl.event = event;
	ctrl.value = value;

	vtdtr_queue_ctrl_poll(txq, &ctrl);
}

/*
 * A wrapper function used to free the list of enabled
 * probes.
 */
static void
vtdtr_destroy_probelist(struct vtdtr_softc *sc)
{
	struct vtdtr_pb *tmp = NULL;
	while (!SLIST_EMPTY(&sc->vtdtr_enabled_probes)) {
		tmp = SLIST_FIRST(&sc->vtdtr_enabled_probes);
		SLIST_REMOVE_HEAD(&sc->vtdtr_enabled_probes, vtdprobe_next);
		free(tmp, M_VIRTIO_DTRACE);
	}
}

/*
 * Start all the taskqueues on the cpu_id = 1. This
 * ensures that each of the taskqueues have a thread
 * assigned to them that will then process all the
 * events that take place.
 */
static void
vtdtr_start_taskqueues(struct vtdtr_softc *sc)
{
	struct virtio_dtrace_queue *rxq, *txq;
	device_t dev;
	int error;

	dev = sc->vtdtr_dev;
	rxq = &sc->vtdtr_rxq;
	txq = &sc->vtdtr_txq;

	error = taskqueue_start_threads(&rxq->vtdq_tq, 1, PI_SOFT,
	    "%s rxq %d", device_get_nameunit(dev), rxq->vtdq_id);
	if (error)
		device_printf(dev, "failed to start rx taskq %d\n",
		    txq->vtdq_id);
	error = taskqueue_start_threads(&txq->vtdq_tq, 1, PI_SOFT,
	    "%s txq %d", device_get_nameunit(dev), txq->vtdq_id);
	if (error)
		device_printf(dev, "failed to start tx taskq %d\n",
		    txq->vtdq_id);
}

/*
 * Free all the taskqueues in the driver.
 * The taskqueues were allocated in vtdtr_init_txq()
 * and vtdtr_init_rxq() 
 */
static void
vtdtr_free_taskqueues(struct vtdtr_softc *sc)
{
	struct virtio_dtrace_queue *rxq, *txq;

	rxq = &sc->vtdtr_rxq;
	txq = &sc->vtdtr_txq;

	if (rxq->vtdq_tq != NULL) {
		taskqueue_free(rxq->vtdq_tq);
		rxq->vtdq_tq = NULL;
	}

	if (txq->vtdq_tq != NULL) {
		taskqueue_free(txq->vtdq_tq);
		txq->vtdq_tq = NULL;
	}
}

/*
 * Drain all the taskqueues in the driver
 */
static void
vtdtr_drain_taskqueues(struct vtdtr_softc *sc)
{
	struct virtio_dtrace_queue *rxq, *txq;

	rxq = &sc->vtdtr_rxq;
	txq = &sc->vtdtr_txq;

	if (rxq->vtdq_tq != NULL)
		taskqueue_drain(rxq->vtdq_tq, &rxq->vtdq_intrtask);

	if (txq->vtdq_tq != NULL)
		taskqueue_drain(txq->vtdq_tq, &txq->vtdq_intrtask);
}

static void
vtdtr_drain_virtqueues(struct vtdtr_softc *sc)
{
	struct virtio_dtrace_queue *rxq, *txq;
	struct virtio_dtrac_control *ctrl;
	struct virtqueue *vq;
	uint32_t last;
	
	rxq = &sc->vtdtr_rxq;
	txq = &sc->vtdtr_txq;
	last = 0;

	vq = rxq->vtdq_vq;

	while ((ctrl = virtqueue_drain(vq, &last)) != NULL)
		free(ctrl, M_DEVBUF);

	vq = txq->vtdq_vq;

	while ((ctrl = virtqueue_drain(vq, &last)) != NULL)
		free(ctrl, M_DEVBUF);
}

/*
 * Disable interrupts on all of the virtqueues in the
 * virtio driver.
 */
static void
vtdtr_disable_interrupts(struct vtdtr_softc *sc)
{
	VTDTR_LOCK(sc);
	vtdtr_vq_disable_intr(&sc->vtdtr_rxq);
	vtdtr_vq_disable_intr(&sc->vtdtr_txq);
	VTDTR_UNLOCK(sc);
}

/*
 * Enable interrupts on all of the virtqueues in the
 * virtio driver.
 */
static int
vtdtr_enable_interrupts(struct vtdtr_softc *sc)
{
	int retval;
	VTDTR_LOCK(sc);
	retval = vtdtr_vq_enable_intr(&sc->vtdtr_txq);
	if (retval != 0)
		goto end;
	retval = vtdtr_vq_enable_intr(&sc->vtdtr_rxq);
end:
	VTDTR_UNLOCK(sc);

	return (retval);
}

/*
 * A wrapper function to enable interrupts in a virtqueue
 */
static int
vtdtr_vq_enable_intr(struct virtio_dtrace_queue *q)
{
	VTDTR_LOCK_ASSERT(q->vtdq_sc);
	return (virtqueue_enable_intr(q->vtdq_vq));
}

/*
 * A wrapper function to disable interrupts in a virtqueue
 */
static void
vtdtr_vq_disable_intr(struct virtio_dtrace_queue *q)
{
	VTDTR_LOCK_ASSERT(q->vtdq_sc);
	virtqueue_disable_intr(q->vtdq_vq);
}

static void
vtdtr_tq_start(struct virtio_dtrace_queue *dtq)
{

}

/*
 * TODO:
 * This is the function that is called when an interrupt is
 * generated in the TX taskqueue.
 */
static void
vtdtr_txq_tq_intr(void *xtxq, int pending)
{
	struct vtdtr_softc *sc;
	struct virtio_dtrace_queue *txq;
	device_t dev;

	txq = xtxq;
	sc = txq->vtdq_sc;
	dev = sc->vtdtr_dev;

	device_printf(dev, "Am I to process this?\n");
}

/*
 * TODO:
 * This is the function that is called when an interrupt is
 * generated in the RX taskqueue.
 */
static void
vtdtr_rxq_tq_intr(void *xrxq, int pending)
{
	struct vtdtr_softc *sc;
	struct virtio_dtrace_queue *rxq;
	struct virtio_dtrace_control *ctrl;
	uint32_t len;
	int error;
	device_t dev;

	rxq = xrxq;
	sc = rxq->vtdq_sc;
	error = 0;
	dev = sc->vtdtr_dev;

	device_printf(dev, "RX: Taskqueue interrupt\n");
	
	VTDTR_QUEUE_LOCK(rxq);

	while ((ctrl = virtqueue_dequeue(rxq->vtdq_vq, &len)) != NULL) {
		VTDTR_QUEUE_UNLOCK(rxq);
		printf("Event: (%s, %d)\n", ctrl->info, ctrl->value);
		error = vtdtr_ctrl_process_event(sc, ctrl);
		KASSERT(error == 0, ("%s: error %d processing event: (%s, %d)",
		    __func__, error, ctrl->info, ctrl->value));
		VTDTR_QUEUE_LOCK(rxq);
		vtdtr_queue_requeue_ctrl(rxq, ctrl, 1, 0);
	}
	
	VTDTR_QUEUE_UNLOCK(rxq);
}

static void
vtdtr_rxq_vq_intr(void *xsc)
{
	struct vtdtr_softc *sc;
	struct virtio_dtrace_queue *rxq;
	device_t dev;

	sc = xsc;
	rxq = &sc->vtdtr_rxq;
	dev = sc->vtdtr_dev;

	device_printf(dev, "RX: VQ Interrupt\n");
	taskqueue_enqueue(rxq->vtdq_tq, &rxq->vtdq_intrtask);
}

static void
vtdtr_txq_vq_intr(void *xsc)
{
	struct vtdtr_softc *sc;
	struct virtio_dtrace_queue *txq;
	device_t dev;

	sc = xsc;
	dev = sc->vtdtr_dev;
	txq = &sc->vtdtr_txq;

	device_printf(dev, "TX: VQ Interrupt\n");
	
	taskqueue_enqueue(txq->vtdq_tq, &txq->vtdq_intrtask);
}

/*
 * This functions sets up all the necessary data for the correct
 * operation of a RX taskqueue of the VirtIO driver:
 *     rxq->vtdq_sc       <- sc
 *     rxq->vtdq_id       <- id
 *     rxq->vtdq_name     <- devname-rx(1|2|3...)
 *     rxq->vtdq_sg       <- allocate or ENOMEM
 *     rxq->vtdq_intrtask <- vtdtr_rxq_tq_intr
 *     rxq->vtdq_tq       <- create taskqueue
 */
static int
vtdtr_init_rxq(struct vtdtr_softc *sc, int id)
{
	struct virtio_dtrace_queue *rxq;

	rxq = &sc->vtdtr_rxq;

	snprintf(rxq->vtdq_name, sizeof(rxq->vtdq_name), "%s-rx%d",
	    device_get_nameunit(sc->vtdtr_dev), id);
	mtx_init(&rxq->vtdq_mtx, rxq->vtdq_name, NULL, MTX_DEF);

	rxq->vtdq_sc = sc;
	rxq->vtdq_id = id;

	TASK_INIT(&rxq->vtdq_intrtask, 0, vtdtr_rxq_tq_intr, rxq);
	rxq->vtdq_tq = taskqueue_create(rxq->vtdq_name, M_NOWAIT,
	    taskqueue_thread_enqueue, &rxq->vtdq_tq);

	return (rxq->vtdq_tq == NULL ? ENOMEM : 0);
}

/*
* This functions sets up all the necessary data for the correct
* operation of a RX taskqueue of the VirtIO driver:
*     txq->vtdq_sc       <- sc
*     txq->vtdq_id       <- id
*     txq->vtdq_name     <- devname-txx(1|2|3...)
*     txq->vtdq_sg       <- allocate or ENOMEM
*     txq->vtdq_intrtask <- vtdtr_txq_tq_intr
*     txq->vtdq_tq       <- create taskqueue
*/
static int
vtdtr_init_txq(struct vtdtr_softc *sc, int id)
{
	struct virtio_dtrace_queue *txq;

	txq = &sc->vtdtr_txq;

	snprintf(txq->vtdq_name, sizeof(txq->vtdq_name), "%s-tx%d",
	    device_get_nameunit(sc->vtdtr_dev), id);
	mtx_init(&txq->vtdq_mtx, txq->vtdq_name, NULL, MTX_DEF);

	txq->vtdq_sc = sc;
	txq->vtdq_id = id;

	TASK_INIT(&txq->vtdq_intrtask, 0, vtdtr_txq_tq_intr, txq);
	txq->vtdq_tq = taskqueue_create(txq->vtdq_name, M_NOWAIT,
	    taskqueue_thread_enqueue, &txq->vtdq_tq);

	return (txq->vtdq_tq == NULL ? ENOMEM : 0);
}

static void
vtdtr_destroy_rxq(struct vtdtr_softc *sc, int id)
{
	struct virtio_dtrace_queue *rxq;

	rxq = &sc->vtdtr_rxq;

	mtx_destroy(&rxq->vtdq_mtx);
	rxq->vtdq_sc = NULL;

	taskqueue_free(rxq->vtdq_tq);
}

static void
vtdtr_destroy_txq(struct vtdtr_softc *sc, int id)
{
	struct virtio_dtrace_queue *txq;

	txq = &sc->vtdtr_txq;

	mtx_destroy(&txq->vtdq_mtx);
	txq->vtdq_sc = NULL;

	taskqueue_free(txq->vtdq_tq);
}
