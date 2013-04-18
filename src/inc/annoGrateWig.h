/* annoGrateWig -- subclass of annoGrator for bigWig or wiggle data */

#ifndef ANNOGRATEWIG_H
#define ANNOGRATEWIG_H

#include "annoGrator.h"

struct annoGrator *annoGrateWigNew(struct annoStreamer *wigSource);
/* Create an annoGrator subclass for source with rowType == arWig. */

struct annoGrator *annoGrateBigWigNew(char *fileOrUrl, struct annoAssembly *aa);
/* Create an annoGrator subclass for bigWig file or URL. */

#endif//ndef ANNOGRATEWIG_H
