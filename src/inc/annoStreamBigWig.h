/* annoStreamBigWig -- subclass of annoStreamer for bigWig file or URL */

#ifndef ANNOSTREAMBIGWIG_H
#define ANNOSTREAMBIGWIG_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamBigWigNew(char *fileOrUrl, struct annoAssembly *aa);
/* Create an annoStreamer (subclass) object from a file or URL. */

struct asObject *annoStreamBigWigAsObject();
/* Return an autoSql object that describes annoRow contents for wiggle/bigWig (just float value). */

#endif//ndef ANNOSTREAMBIGWIG_H
