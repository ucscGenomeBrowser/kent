/* sessionThumbnail - this header provides an interface for getting
 * the filename, filesystem path, and web URI for the thumbnail image
 * associated with a session.
 *
 * Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

char *sessionThumbnailFilePath(char *userIdx, char *encSessionName,
                               char *firstUse);
/* Returns NULL if the image directory for session thumbnails hasn't been
 * set in hg.conf.  Otherwise, returns the filename and path for the
 * thumbnail of the specified session */

char *sessionThumbnailFileUri(char *userIdx, char *encSessionName,
                              char *firstUse);
/* Returns NULL if the web path to session thumbnails hasn't been
 * defined in hg.conf.  Otherwise, returns the path and filename for the
 * thumbnail of the specified session */

