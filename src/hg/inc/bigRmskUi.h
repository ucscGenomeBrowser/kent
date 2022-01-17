/* bigRmskUi - bigRmsk tracks */

/* Copyright (C) 2021 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef BIGRMSKUI_H
#define BIGRMSKUI_H

/* Visual features */
// Show unaligned extensions arms on detailed glyphs
#define BIGRMSK_SHOW_UNALIGNED_EXTENTS           "bigrmsk.showUnalignedExtents"
#define BIGRMSK_SHOW_UNALIGNED_EXTENTS_DEFAULT   TRUE

// Show inline labels preceeding each glyph
#define BIGRMSK_SHOW_LABELS           "bigrmsk.showLabels"
#define BIGRMSK_SHOW_LABELS_DEFAULT   TRUE

// Revert to the original UCSC class-based 'pack' visualization
#define BIGRMSK_ORIG_PACKVIZ          "bigrmsk.origPackViz"
#define BIGRMSK_ORIG_PACKVIZ_DEFAULT   FALSE

// The name filter value
#define BIGRMSK_NAME_FILTER           "bigrmsk.nameFilter"
#define BIGRMSK_NAME_FILTER_DEFAULT   "*"

// Allow the user to use regular expressions in the name filter
#define BIGRMSK_REGEXP_FILTER         "bigrmsk.regexpFilter"
#define BIGRMSK_REGEXP_FILTER_DEFAULT FALSE

#endif /* BIGRMSKUI_H */
