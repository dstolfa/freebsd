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

#ifndef _VIRTIO_DTRACE_H_
#define _VIRTIO_DTRACE_H_

#define	VIRTIO_DTRACE_F_PROBE	0x01
#define	VIRTIO_DTRACE_F_PROV	0x02

/*
 * The only ones used presently are:
 * 	VIRTIO_DTRACE_PROBE_INSTALL
 * 	VIRTIO_DTRACE_PROBE_UNINSTALL
 *
 * The rest is simpler handled through the hypervisor itself due to it handling
 * the virtual machine name and there is no implicity trust being placed in the
 * userspace to handle things properly(bhyve).
 */
#define	VIRTIO_DTRACE_DEVICE_READY	0x00	/* The device is ready */
#define	VIRTIO_DTRACE_REGISTER		0x01	/* Provider Registration */
#define	VIRTIO_DTRACE_UNREGISTER	0x02	/* Provider Unregistration */
#define	VIRTIO_DTRACE_DESTROY		0x03	/* Instance Destruction */
#define	VIRTIO_PROBE_CREATE		0x04	/* Probe Creation */
#define	VIRTIO_DTRACE_PROBE_INSTALL	0x05	/* Probe Installation */
#define	VIRTIO_DTRACE_PROBE_UNINSTALL	0x06	/* Probe Uninstallation */

#define	VIRTIO_DTRACE_BAD_ID (~(uint32_t)0)

struct virtio_dtrace_control {
	uint32_t	event;
	uint32_t	value;
};

#endif
