/* knetUdc -- legacy shim, now delegates to hfile UDC backend.
 * The old knetfile hook mechanism is no longer supported by htslib.
 * This stub keeps existing callers of knetUdcInstall() compiling
 * while UDC I/O is handled by the hfile scheme handler in hfileUdc.c. */

/* Copyright (C) 2014 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "knetUdc.h"
#include "hfileUdc.h"

void knetUdcInstall()
/* Install UDC I/O via hfile backend.  Legacy wrapper — new code should
 * call hfileUdcInstall() directly. */
{
hfileUdcInstall();
}