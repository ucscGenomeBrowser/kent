/* Split up a SQL input file into pieces.  Checks to make
 * sure the break is on a boundary between insert statements. */

#include "common.h"
#include "linefile.h"
#include "portable.h"

int main(int argc, char *argv[])
{
int fileCount;
char *sourceName;
char *destName;
int i;
long start = 0;
long end;
long targetSize;
char *splitter = "INSERT";
struct lineFile *slf;
FILE *d;
int count;
int lineCount = 0;
char *lineStart;
int lineSize;
long fsize;
long fileOffset;

if (argc < 3)
    {
    errAbort("splitSql - split file into pieces\n"
	     "usage\n"
	     "   splitSql in out1 out2 ... outN\n");
    }
sourceName = argv[1];
fileCount = argc-2;
fsize = fileSize(sourceName);
targetSize = (fsize + fileCount-1)/fileCount;
slf = lineFileOpen(sourceName, FALSE);

for (i = 0; i<fileCount; ++i)
    {
    long endOffset = (i+1)*fsize/fileCount;
    destName = argv[i+2];
    d = mustOpen(destName, "w");

    /* Copy up until target size is reached. */
    while (fileOffset < endOffset)
	{
	if (!lineFileNext(slf, &lineStart, &lineSize))
	    break;
	fileOffset += lineSize;
	mustWrite(d, lineStart, lineSize);
	}

    /* Keep copying until get line that starts with
     * our pattern. */
    for (;;)
	{
	if (!lineFileNext(slf, &lineStart, &lineSize))
	    break;
	if (startsWith(splitter, skipLeadingSpaces(lineStart)))
	    {
	    lineFileReuse(slf);
	    break;
	    }
	fileOffset += lineSize;
	mustWrite(d, lineStart, lineSize);
	}
    fclose(d);
    }
lineFileClose(&slf);
}
