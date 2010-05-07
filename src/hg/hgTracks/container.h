/* container.h - stuff for container tracks. */

#ifndef CONTAINER_H
#define CONTAINER_H

void makeContainerTrack(struct track *track, struct trackDb *tdb);
/* Construct track subtrack list from trackDb entry for container tracks. */

void multiWigContainerMethods(struct track *track);
/* Override general container methods for multiWig. */

#endif /* CONTAINER_H */

