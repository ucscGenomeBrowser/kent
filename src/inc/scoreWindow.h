/* calculate score array with score at each position in s, match to char c adds 1 to score, mismatch adds -1 */
/* index of max score is returned , size is size of s */
/* used for scoring polyA tails */
int scoreWindow(char c, char *s, int size, int *score, int *start, int *end, int match, int misMatch);
