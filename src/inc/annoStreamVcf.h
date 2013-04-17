/* annoStreamVcf -- subclass of annoStreamer for VCF files */

#ifndef ANNOSTREAMVCF_H
#define ANNOSTREAMVCF_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamVcfNew(char *fileOrUrl, boolean isTabix, struct annoAssembly *aa,
				      int maxRecords);
/* Create an annoStreamer (subclass) object from a VCF file, which may
 * or may not have been compressed and indexed by tabix. */

#endif//ndef ANNOSTREAMVCF_H
