/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef GOOGLEANALYTICS_H
#define GOOGLEANALYTICS_H

void googleAnalyticsSetGa4Key(void);
/* if the google analytics key is GA4 key, set the variable in htmlshell */

void googleAnalytics(void);
/* check for analytics configuration items and output google hooks if OK */

#endif /* GOOGLEANALYTICS_H */
