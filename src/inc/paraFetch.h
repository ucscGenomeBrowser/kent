/* paraFetch - fetch things from remote URLs in parallel. */

#ifndef PARAFETCH_H
#define PARAFETCH_H

boolean parallelFetch(char *url, char *outPath, int numConnections, int numRetries, boolean newer, boolean progress);
/* Open multiple parallel connections to URL to speed downloading */

boolean parallelFetchInterruptable(char *url, char *outPath, int numConnections, int numRetries, 
    boolean newer, boolean progress,
    boolean (*interrupt)(void *context),  void *context);
/* Open multiple parallel connections to URL to speed downloading.  If interrupt function 
 * is non-NULL,  then it gets called passing the context parameter,  and if it returns
 * TRUE the fetch is interrupted. */

void parallelFetchRemovePartial(char *destName);
/* Remove any files associated with partial downloads of file of given name. 
 * This is needed after a FALSE return from parallelFetch or parallelFetchInterruptable
 * unless you want to leave temp file there to try to restart. */

struct parallelConn
/* struct to information on a parallel connection */
    {
    struct parallelConn *next;  /* next connection */
    int sd;                     /* socket descriptor */
    off_t rangeStart;           /* where does the range start */
    off_t partSize;             /* range size */
    off_t received;             /* bytes received */
    };

boolean paraFetchReadStatus(char *origPath, 
    struct parallelConn **pPcList, char **pUrl, off_t *pFileSize, 
    char **pDateString, off_t *pTotalDownloaded);
/* Read a status file - which is just origPath plus .paraFetchStatus.  This is updated during 
 * transit by parallelFetch. Returns FALSE if status file not there - possibly because
 * transfer is finished.  Any of the return parameters (pThis and pThat) may be NULL */

time_t paraFetchTempUpdateTime(char *origPath);
/* Return last mod date of temp file - which is useful to see if process has stalled. */

#endif /* PARAFETCH_H */
