/* scoreWindow - find window with most matches to a given char */

/* simple program to find max scoring window representing string of char c in a string s of size size */
/* index of max score is returned , match and misMatch are the scores to assign, suggested defaults are match=1 and misMatch=1*/
/* when used for scoring polyA tails, set c='A' for positive strand  or c='T' for neg strand */
/* start, end are returned pointing to the start and end of the highest scoring window in s */
int scoreWindow(char c, char *s, int size, int *score, int *start, int *end, int match, int misMatch);
