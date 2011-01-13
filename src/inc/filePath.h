/* filePath - stuff to handle file name parsing. */
#ifndef FILEPATH_H
#define FILEPATH_H

#include "common.h"

void splitPath(char *path, char dir[PATH_LEN], char name[FILENAME_LEN],
	       char extension[FILEEXT_LEN]);
/* Split a full path into components.  The dir component will include the
 * trailing / if any.  The extension component will include the starting
 * . if any.   Pass in NULL for dir, name, or extension if you don't care about
 * that part. */

char *expandRelativePath(char *baseDir, char *relPath);
/* Expand relative path to more absolute one. */

char *pathRelativeToFile(char *baseFile, char *relPath);
/* Given a base file name and a path relative to that, return
 * relative path interpreted as if it were seen from the
 * same directory holding the baseFile.  
 *   An example of using this would be in processing include
 * files.  In this case the baseFile would be the current
 * source file, and the relPath would be from the include
 * statement.  The returned result could then be used to
 * open the include file. */

void undosPath(char *path);
/* Convert '\' to '/' in path. (DOS/Windows is typically ok with
 * this actually.) */

#endif /* FILEPATH_H */
