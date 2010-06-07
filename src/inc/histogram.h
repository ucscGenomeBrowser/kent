/* histogram function for data array in memory	*/

#if !defined(HISTOGRAM_H)
#define HISTOGRAM_H

#define	DEFAULT_BIN_COUNT	26
/*	ARBITRARY default choice of # of bins	*/

struct histoResult {
    struct histoResult *next;	/*	might be a list of them	*/
    float binSize;		/*	size of a bin	*/
    unsigned binCount;		/*	number of bins	*/
    unsigned count;		/*	number of values	*/
    float binZero;		/*	bin 0 starts at this value	*/
    unsigned *binCounts;	/*	array binCounts[binCount]	*/
    float *pValues;		/*	array pValues[binCount]		*/
};

void freeHistoGram(struct histoResult **histoResults);
/*      free the histoResults list	*/

struct histoResult *histoGram(float *values, size_t N, float binSize,
	unsigned binCount, float minValue, float min, float max,
	struct histoResult *accumHisto);
/*	construct histogram of data in values[N] array.	*/
#endif
