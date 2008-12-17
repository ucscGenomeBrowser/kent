/* Put some common stuff in here for hgTrackUi CGI. */

#ifndef HGTRACKUI_H
#define HGTRACKUI_H

#ifndef WEB_H
#include "web.h"
#endif

#ifndef CART_H
#include "cart.h"
#endif

#ifndef HDB_H
#include "hdb.h"
#endif

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

#ifndef CUSTOM_TRACK_H
#include "customTrack.h"
#endif

extern struct cart *cart;	/* Cookie cart with UI settings */
extern char *database;		/* Current database. */
extern char *chromosome;	        /* Chromosome. */

/* Functions */

void cgapSageUi(struct trackDb *tdb);
/* CGAP SAGE UI options. Highlight certain libs/tissues and filter by score. */

void encodePeakUi(struct trackDb *tdb, struct customTrack *ct);
/* ENCODE peak UI options. */

#endif /* HGTRACKUI_H */
