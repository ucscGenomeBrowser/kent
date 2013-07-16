/* annoStreamBigBed -- subclass of annoStreamer for bigBed file or URL */

#ifndef ANNOSTREAMBIGBED_H
#define ANNOSTREAMBIGBED_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamBigBedNew(char *fileOrUrl, struct annoAssembly *aa, int maxItems);
/* Create an annoStreamer (subclass) object from a file or URL; if
 * maxItems is 0, all items from a query will be returned, otherwise
 * output is limited to maxItems. */

#endif//ndef ANNOSTREAMBIGBED_H
