/* annoStreamLongTabix -- subclass of annoStreamer for longTabix files */

#ifndef ANNOSTREAMLONGTABIX_H
#define ANNOSTREAMLONGTABIX_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamLongTabixNew(char *fileOrUrl, struct annoAssembly *aa, int maxRecords);
/* Create an annoStreamer (subclass) object from a longTabix file */

#endif
