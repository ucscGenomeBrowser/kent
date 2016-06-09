/* annoStreamVcf -- subclass of annoStreamer for tab files indexed with tabix */

#ifndef ANNOSTREAMTABIX_H
#define ANNOSTREAMTABIX_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamTabixNew(char *fileOrUrl, struct annoAssembly *aa, int maxRecords);
/* Create an annoStreamer (subclass) object from a tabix indexed tab file */

#endif//ndef ANNOSTREAMTABIX_H
