/* bigBedLabel.h - Label things in big beds . */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BIGBEDLABEL_H
#define BIGBEDLABEL_H
void bigBedLabelCalculateFields(struct cart *cart, struct trackDb *tdb, struct bbiFile *bbi,  struct slInt **labelColumns );
/* Figure out which fields are available to label a bigBed track. */

char *bigBedMakeLabel(struct trackDb *tdb, struct slInt *labelColumns, struct bigBedInterval *bb, char *chromName);
// Build a label for a bigBedTrack from the requested label fields.
#endif /* BIGBEDLABEL_H */
