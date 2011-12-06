/* filePath - stuff to handle file name parsing. */
#include "common.h"
#include "filePath.h"


void undosPath(char *path)
/* Convert '\' to '/' in path. */
{
subChar(path, '\\', '/');
}

void splitPath(char *path, char dir[PATH_LEN], char name[FILENAME_LEN],
	       char extension[FILEEXT_LEN])
/* Split a full path into components.  The dir component will include the
 * trailing / if any.  The extension component will include the starting
 * . if any.   Pass in NULL for dir, name, or extension if you don't care about
 * that part. */
{
char *dirStart, *nameStart, *extStart, *extEnd;
int dirSize, nameSize, extSize;

undosPath(path);
dirStart = path;
nameStart = strrchr(path,'/');
if (nameStart == NULL)
    nameStart = path;
else
    nameStart += 1;
extStart = strrchr(nameStart, '.');
if (extStart == NULL)
    extStart = nameStart + strlen(nameStart);
extEnd = extStart + strlen(extStart);
if ((dirSize = (nameStart - dirStart)) >= PATH_LEN)
    errAbort("Directory too long in %s", path);
if ((nameSize = (extStart - nameStart)) >= FILENAME_LEN)
    errAbort("Name too long in %s", path);
if ((extSize = (extEnd - extStart)) >= FILEEXT_LEN)
    errAbort("Extension too long in %s", path);
if (dir != NULL)
    {
    memcpy(dir, dirStart, dirSize);
    dir[dirSize] = 0;
    }
if (name != NULL)
    {
    memcpy(name, nameStart, nameSize);
    name[nameSize] = 0;
    }
if (extension != NULL)
    {
    memcpy(extension, extStart, extSize);
    extension[extSize] = 0;
    }
}

static char *findSlashBefore(char *start, char *e)
/* Return first slash before s (but not before start) */
{
while (--e >= start)
    {
    if (*e == '/')
         return e;
    }
return start;
}

char *expandRelativePath(char *baseDir, char *relPath)
/* Expand relative path to more absolute one. */
{
if (relPath[0] == '/')
   // hey, it's absolute actually... 
   return cloneString(relPath);

char *e = baseDir + strlen(baseDir);
int slashCount;
char *rel = relPath;
char *result;
int size, baseSize;
undosPath(baseDir);
undosPath(relPath);
slashCount = countChars(baseDir, '/');
if (baseDir[0] == 0)
    slashCount = -1;
while (startsWith("../", rel))
    {
    if (slashCount < 0)
        {
	warn("More ..'s in \"%s\" than directories in \"%s\"", relPath, baseDir);
	return NULL;
	}
    else if (slashCount == 0)
        e = baseDir;
    else
        e = findSlashBefore(baseDir, e);
    slashCount -= 1;
    rel += 3;
    }
baseSize = e - baseDir;
size = strlen(rel) + 1;
if (baseSize > 0)
    size += baseSize + 1;
if (baseSize > 0)
    {
    result = needMem(size);
    memcpy(result, baseDir, baseSize);
    result[baseSize] = '/';
    strcpy(result + baseSize + 1, rel);
    }
else
    result = cloneString(rel);
return result;
}

char *pathRelativeToFile(char *baseFile, char *relPath)
/* Given a base file name and a path relative to that, return
 * relative path interpreted as if it were seen from the
 * same directory holding the baseFile.  
 *   An example of using this would be in processing include
 * files.  In this case the baseFile would be the current
 * source file, and the relPath would be from the include
 * statement.  The returned result could then be used to
 * open the include file. */
{
char dir[PATH_LEN];
splitPath(baseFile, dir, NULL, NULL);
int dirLen = strlen(dir);
if (dirLen > 0 && dir[dirLen-1] == '/')
     dir[dirLen-1] = 0;
return expandRelativePath(dir, relPath);
}
