/* filePath - stuff to handle file name parsing. */
#ifndef FILEPATH_H
#define FILEPATH_H

void splitPath(char *path, char dir[256], char name[128], char extension[64]);
/* Split a full path into components.  The dir component will include the
 * trailing / if any.  The extension component will include the starting
 * . if any.   Pass in NULL for dir, name, or extension if you don't care about
 * that part. */

char *expandRelativePath(char *baseDir, char *relPath);
/* Expand relative path to more absolute one. */

void undosPath(char *path);
/* Convert '\' to '/' in path. (DOS/Windows is typically ok with
 * this actually.) */

#endif /* FILEPATH_H */
