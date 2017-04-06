/* annoStreamTab -- subclass of annoStreamer for tab-separated text files/URLs */

#ifndef ANNOSTREAMTAB_H
#define ANNOSTREAMTAB_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamTabNew(char *fileOrUrl, struct annoAssembly *aa,
				      struct asObject *asObj, int maxOutRows);
/* Create an annoStreamer (subclass) object from a tab-separated text file/URL
 * whose columns are described by asObj (possibly excepting bin column at beginning).
 * If maxOutRows is greater than 0 then it is an upper limit on the number of rows
 * that the streamer will produce. */

#endif//ndef ANNOSTREAMTAB_H
