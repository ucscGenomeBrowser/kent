
void fragFind(struct seqList *goodSeq, char *badName, int fragSize, int mismatchesAllowed, 
    boolean considerRc, double profile[16][4]);
/* Do fast finding of patterns that are in FA file "goodName", but not in FA file
 * "badName."  BadName can be null.  Pass in the size of the pattern (fragSize) and
 * the number of mismatches to pattern you're willing to tolerate (mismatchesAllowed).
 * It returns the pattern in profile. */
