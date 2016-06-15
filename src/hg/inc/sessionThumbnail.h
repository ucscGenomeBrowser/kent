/* sessionThumbnail - this header provides an interface for getting
 * the filename, filesystem path, and web URI for the thumbnail image
 * associated with a session.
 *
 * Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

char *sessionThumbnailFilePath(char *userIdx, char *encSessionName,
                               char *firstUse);
/* Returns the path to the thumbnail image of the specified session as seen by CGIs.
 * Result must be freed. */

char *sessionThumbnailFileUri(char *userIdx, char *encSessionName,
                              char *firstUse);
/* Returns the path to the thumbnail image of the specified session as seen by web viewers.
 * Result must be freed. */
