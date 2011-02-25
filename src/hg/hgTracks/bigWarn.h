/* bigWarn -- shared handlers for displaying big/udc warn/error messages */

#ifndef BIGWARN_H
#define BIGWARN_H

#include "common.h"
#include "hgTracks.h"
#include "container.h"

char *bigWarnReformat(char *errMsg);
/* Return a copy of the re-formatted error message,
 * such as breaking longer lines */

void bigDrawWarning(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                 int xOff, int yOff, int width, MgFont *font, Color color,
                 enum trackVisibility vis);
/* Draw the network error message */

int bigWarnTotalHeight(struct track *tg, enum trackVisibility vis);
/* Return total height. Called before and after drawItems.
 * Must set the following variables: height, lineHeight, heightPer. */

#endif /* BIGWARN_H */
