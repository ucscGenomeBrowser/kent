/* hgSession - Manage information associated with a user identity. */

#ifndef HGSESSION_H
#define HGSESSION_H

/* hgSession form inputs */
#define hgSessionPrefix "hgS_"

#define hgsNewSessionName hgSessionPrefix "newSessionName"
#define hgsNewSessionShare hgSessionPrefix "newSessionShare"
#define hgsDoNewSession hgSessionPrefix "doNewSession"

#define hgsUnsharePrefix hgSessionPrefix "unshare_"
#define hgsSharePrefix hgSessionPrefix "share_"
#define hgsLoadPrefix hgSessionPrefix "load_"
#define hgsDeletePrefix hgSessionPrefix "delete_"

#define hgsOtherUserName hgSessionPrefix "otherUserName"
#define hgsOtherUserSessionName hgSessionPrefix "otherUserSessionName"
#define hgsDoOtherUser hgSessionPrefix "doOtherUser"

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
