/* hgSession - Manage information associated with a user identity. */

#ifndef HGSESSION_H
#define HGSESSION_H

/* NOTE: some of the original contents have been moved up to hui.h for 
 * sharing with hgTracks. */

#include "hui.h"

/* hgSession form inputs */
#define hgsNewSessionName hgSessionPrefix "newSessionName"
#define hgsNewSessionShare hgSessionPrefix "newSessionShare"
#define hgsDoNewSession hgSessionPrefix "doNewSession"

#define hgsUnsharePrefix hgSessionPrefix "unshare_"
#define hgsSharePrefix hgSessionPrefix "share_"
#define hgsLoadPrefix hgSessionPrefix "load_"
#define hgsDeletePrefix hgSessionPrefix "delete_"

#define hgsSaveLocalFileName hgSessionPrefix "saveLocalFileName"
#define hgsSaveLocalFileCompress hgSessionPrefix "saveLocalFileCompress"
#define hgsDoSaveLocal hgSessionPrefix "doSaveLocal"

#define hgsLoadLocalFileName hgSessionPrefix "loadLocalFileName"
#define hgsDoLoadLocal hgSessionPrefix "doLoadLocal"

#define hgsLoadUrlName hgSessionPrefix "loadUrlName"
#define hgsDoLoadUrl hgSessionPrefix "doLoadUrl"

#define hgsDoMainPage hgSessionPrefix "doMainPage"

#define hgsDo hgSessionPrefix "do"

#endif /* HGSESSION_H */
