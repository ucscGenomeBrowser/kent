/* container.h - stuff for container tracks. */

#ifndef CONTAINER_H
#define CONTAINER_H

void makeContainerTrack(struct track *track, struct trackDb *tdb);
/* Construct track subtrack list from trackDb entry for container tracks. */

void multiWigContainerMethods(struct track *track);
/* Override general container methods for multiWig. */

void containerLoadItems(struct track *track);
/* containerLoadItems - call load routine on all children. */

char *parentContainerType(struct track *track);
/* Determine parent's container type if any or NULL */

void containerDrawItems(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw items in container. */

#endif /* CONTAINER_H */

