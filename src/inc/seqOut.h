/* seqOut - stuff to output sequences and alignments in web 
 * or ascii viewable form. */

struct cfm
/* Colored web character output formatter. */
    {
    int wordLen;	/* Number of characters between spaces (often 10) */
    int lineLen;        /* Number of characters between lines (often 50) */
    int inWord, inLine; /* Position in word and line. */
    bool lineNumbers;   /* True if write position at end of line. */
    bool countDown;     /* True if want numbers counting down. */
    int lastColor;      /* Cache last color here. */
    long charCount;     /* Number of characters written total. */
    FILE *out;          /* File to write to. */
    int numOff;         /* Number to start with. */
    };

void cfmInit(struct cfm *cfm, int wordLen, int lineLen, 
	boolean lineNumbers, boolean countDown, FILE *out, int numOff);
/* Set up colored sequence formatting for html. */

void cfmOut(struct cfm *cfm, char c, int color);
/* Write out a byte, and depending on color formatting extras  */

void cfmCleanup(struct cfm *cfm);
/* Finish up cfm formatting job. */

struct baf
/* Block allignment formatter. */
    {
    char nChars[50];
    char hChars[50];
    int cix;
    int nLineStart;
    int hLineStart;
    int nCurPos;
    int hCurPos;
    DNA *needle, *haystack;
    int nNumOff, hNumOff;
    FILE *out;
    bool hCountDown;     /* True if want numbers counting down. */
    };

void bafInit(struct baf *baf, DNA *needle, int nNumOff, DNA *haystack, int hNumOff, 
	boolean hCountDown, FILE *out);
/* Initialize block alignment formatter. */

void bafSetAli(struct baf *baf, struct ffAli *ali);
/* Set up block formatter around an ffAli block. */

void bafStartLine(struct baf *baf);
/* Set up block formatter to start new line at current position. */

void bafWriteLine(struct baf *baf);
/* Write out a line of an alignment (which takes up
 * three lines on the screen. */

void bafOut(struct baf *baf, char n, char h);
/* Write a pair of character to block alignment. */

void bafFlushLine(struct baf *baf);
/* Write out alignment line if it has any characters in it. */

