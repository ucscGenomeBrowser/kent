/* functions to auto-kill CGIs after a certain amount of time has passed */

#ifndef CGIAPOPTOSIS_H
#define CGIAPOPTOSIS_H

#include <utime.h>
#include <htmlPage.h>
#include <signal.h>
#include "geoMirror.h"
#include <regex.h>
#include "trackHub.h"

void lazarusLives(unsigned long newExpireSeconds);
/* long running process requests more time */

void cgiApoptosisSetup();
/* setting up cgi auto killing after x minutes */

#endif /* CGIAPOPTOSIS_H */
