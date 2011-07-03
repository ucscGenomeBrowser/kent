/* overlapFilter - filter overlapping alignments */
#ifndef OVERLAPFILTER_H
#define OVERLAPFILTER_H

struct cDnaQuery;

void overlapFilterFlagWeird(struct cDnaQuery *cdna);
/* Flag alignments that have weird overlap with other alignments */

void overlapFilterOverlapping(struct cDnaQuery *cdna);
/* Remove overlapping alignments, keeping only one by some criteria.  This is
 * designed to be used with overlapping, windowed alignments, so one alignment
 * might be truncated. */

void overlapFilterWeirdFilter(struct cDnaQuery *cdna);
/* Filter alignments identified as weirdly overlapping, keeping only the
 * highest scoring ones. */

#endif
