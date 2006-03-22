/* overlapFilter - filter overlapping alignments */
#ifndef OVERLAPFILTER_H
#define OVERLAPFILTER_H

struct cDnaQuery;

void overlapFilter(struct cDnaQuery *cdna);
/* Remove overlapping alignments, keeping only one by some criteria.  This is
 * designed to be used with windowed alignments, so one alignment might be
 * trucated. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

