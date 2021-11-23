/* probePage - put up page of info on probe. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef PROBEPAGE_H

void probePage(struct sqlConnection *conn, int probeId, int submissionSourceId);
/* Put up a page of info on probe. */

#endif /* PROBEPAGE_H */

