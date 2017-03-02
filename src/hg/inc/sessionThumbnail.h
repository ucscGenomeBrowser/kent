/* sessionThumbnail - this header provides an interface for getting
 * the filename, filesystem path, and web URI for the thumbnail image
 * associated with a session.
 *
 * Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

char *sessionThumbnailFilePath(char *userIdentifier, char *encSessionName,
                               char *firstUse);
/* Returns the path to the thumbnail image of the specified session as seen by CGIs.
 * Result must be freed. */

char *sessionThumbnailFileUri(char *userIdentifier, char *encSessionName,
                              char *firstUse);
/* Returns the path to the thumbnail image of the specified session as seen by web viewers.
 * Result must be freed. */

char *sessionThumbnailGetUserIdentifier(char *userName, char *userIdx);
/* Return a unique identifier for the specified userName and (if non-NULL) userIdx from the gbMembers table. */
