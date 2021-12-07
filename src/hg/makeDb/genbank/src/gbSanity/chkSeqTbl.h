/* chkSeqTbl - validation of seq with extFile and fastas */

/* Copyright (C) 2006 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef CHKSEQTBL_H
#define CHKSEQTBL_H
struct gbSelect;
struct sqlConnection;
struct metaDataTbls;

void loadSeqData(struct metaDataTbls* metaDataTbls,
                 struct gbSelect* select, struct sqlConnection* conn,
                 boolean checkExtSeqRecs, char* gbdbMapToCurrent);
/* load seq table data, gbCdnaInfo table should be loaded. */
#endif
