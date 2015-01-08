/* annoFormatTab -- collect fields from all inputs and print them out, tab-separated. */

#ifndef ANNOFORMATTAB_H
#define ANNOFORMATTAB_H

#include "annoFormatter.h"

struct annoFormatter *annoFormatTabNew(char *fileName);
/* Return a formatter that will write its tab-separated output to fileName. */

void annoFormatTabSetColumnVis(struct annoFormatter *self, char *sourceName, char *colName,
                               boolean enabled);
/* Explicitly include or exclude column in output.  sourceName must be the same
 * as the corresponding annoStreamer source's name. */

#endif//ndef ANNOFORMATTAB_H
