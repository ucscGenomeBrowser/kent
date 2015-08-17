/* annoGrateWig -- subclass of annoGrator for bigWig or wiggle data */

#ifndef ANNOGRATEWIG_H
#define ANNOGRATEWIG_H

#include "annoGrator.h"

enum annoGrateWigMode { agwmAverage, agwmPerBase };

struct annoGrator *annoGrateWigNew(struct annoStreamer *wigSource, enum annoGrateWigMode mode);
/* Create an annoGrator subclass for source with wiggle or bigWig data.
 * If mode is agwmAverage, then this produces at most one annoRow per primary item
 * with type arWigSingle, containing the average of all overlapping non-NAN values.
 * If mode is agwmPerBase, then this produces a list of annoRows split by NANs, with type
 * arWigVec (vector of per-base values). */

struct annoGrator *annoGrateBigWigNew(char *fileOrUrl, struct annoAssembly *aa,
                                      enum annoGrateWigMode mode);
/* Create an annoGrator subclass for bigWig file or URL.  See above description of mode. */

#endif//ndef ANNOGRATEWIG_H
