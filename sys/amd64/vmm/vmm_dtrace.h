/*-
 * Copyright (c) 2017 Domagoj Stolfa <domagoj.stolfa@gmail.com>
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

#ifndef _VMM_DTRACE_H_
#define _VMM_DTRACE_H_

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/sdt.h>

#ifdef SDT_PROVIDER_DECLARE
SDT_PROVIDER_DECLARE(vmm);
#endif

#define	VMM_PROBE_DEFINE0(mod, name) \
SDT_PROBE_DEFINE0(vmm, mod, , name)

#define	VMM_PROBE_DEFINE1(mod, name, arg0) \
SDT_PROBE_DEFINE1(vmm, mod, , name, arg0)

#define	VMM_PROBE_DEFINE2(mod, name, arg0, arg1) \
SDT_PROBE_DEFINE2(vmm, mod, , name, arg0, arg1)

#define	VMM_PROBE_DEFINE3(mod, name, arg0, arg1, arg2) \
SDT_PROBE_DEFINE3(vmm, mod, , name, arg0, arg1, arg2)

#define	VMM_PROBE_DEFINE4(mod, name, arg0, arg1, arg2, arg3) \
SDT_PROBE_DEFINE4(vmm, mod, , name, arg0, arg1, arg2, arg3)

#define	VMM_PROBE_DEFINE5(mod, name, arg0, arg1, arg2, arg3, arg4) \
SDT_PROBE_DEFINE5(vmm, mod, , name, arg0, arg1, arg2, arg3, arg4)

#define	VMM_PROBE_DEFINE6(mod, name, arg0, arg1, arg2, arg3, arg4, arg5) \
SDT_PROBE_DEFINE6(vmm, mod, , name, arg0, arg1, arg2, arg3, arg4, arg5)

#define	VMM_PROBE_DEFINE7(mod, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6) \
SDT_PROBE_DEFINE7(vmm, mod, , name, arg0, arg1, arg2, arg3, arg4, arg5, arg6)

#define	VMM_PROBE0(mod, name) \
SDT_PROBE0(vmm, mod, , name)

#define	VMM_PROBE1(mod, name, arg0) \
SDT_PROBE1(vmm, mod, , name, arg0)

#define	VMM_PROBE2(mod, name, arg0, arg1) \
SDT_PROBE2(vmm, mod, , name, arg0, arg1)

#define	VMM_PROBE3(mod, name, arg0, arg1, arg2) \
SDT_PROBE3(vmm, mod, , name, arg0, arg1, arg2)

#define	VMM_PROBE4(mod, name, arg0, arg1, arg2, arg3) \
SDT_PROBE4(vmm, mod, , name, arg0, arg1, arg2, arg3)

#define	VMM_PROBE5(mod, name, arg0, arg1, arg2, arg3, arg4) \
SDT_PROBE5(vmm, mod, , name, arg0, arg1, arg2, arg3, arg4)

#define	VMM_PROBE6(mod, name, arg0, arg1, arg2, arg3, arg4, arg5) \
SDT_PROBE6(vmm, mod, , name, arg0, arg1, arg2, arg3, arg4, arg5)

#define	VMM_PROBE7(mod, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6) \
SDT_PROBE7(vmm, mod, , name, arg0, arg1, arg2, arg3, arg4, arg5, arg6)

#endif /* _VMM_DTRACE_H_ */
