/* vgPrepImage - Create thumbnail and image pyramid scheme for image, also link in full sized image. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"

#include "jpgTilesX.h"
#include "jpgDec.h"
#include "jp2Dec.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgPrepImage - Create thumbnail and image pyramid scheme for image, \n"
  "also link in full sized image\n"
  "usage:\n"
  "   vgPrepImage sourceDir thumbDir fullDir image.jpg\n"
  "options:\n"
  "   -noLink - don't link in original image to fullDir\n"
  );
}

boolean noLink = FALSE;

static struct optionSpec options[] = {
   {"noLink", OPTION_BOOLEAN},
   {NULL, 0},
};

void execute(char *command, int okErr)
/* An error-detecting wrapper for system. */
{
int err;
verbose(2, "%s = ", command);
err = system(command);
verbose(2, "%d\n", err);
if (err != 0 && err != okErr)
    errAbort("err %d from system(%s)", err, command);
}

void makeDirForFile(char *path)
/* Create directory for path if it doesn't already exist. */
{
char dir[PATH_LEN];
char command[PATH_LEN+32];
splitPath(path, dir, NULL, NULL);
safef(command, sizeof(command), "mkdir -p %s", dir);
execute(command, 0);
}

void makeLink(char *source, char *link)
/* Create symbolic link. */
{
struct dyString *dy = dyStringNew(0);
dyStringClear(dy);
dyStringPrintf(dy, "ln -s %s %s", source, link);
execute(dy->string, 0);
dyStringFree(&dy);
}

void vgPrepImage(char *sourceDir, char *thumbDir, char *fullDir, char *fileName)
/* vgPrepImage - Create thumbnail and image pyramid scheme for image, 
 * also link in full sized image. */
{
char source[PATH_LEN], thumb[PATH_LEN], full[PATH_LEN], pyramid[PATH_LEN];
char outFullDir[PATH_LEN],  outFullRoot[PATH_LEN]; 
int nWidth, nHeight;
int quality[7];
boolean makeFullSize = FALSE;
unsigned char *(*readScanline)() = NULL;

/* Figure out full paths. */
safef(source, sizeof(source), "%s/%s", sourceDir, fileName);
safef(thumb, sizeof(thumb), "%s/%s", thumbDir, fileName);
safef(full, sizeof(full), "%s/%s", fullDir, fileName);
splitPath(full, outFullDir, outFullRoot, NULL);
outFullDir[strlen(outFullDir)-1]=0;  /* knock off trailing slash */
strcpy(pyramid,full);
chopSuffix(pyramid);

makeDirForFile(thumb);
makeDirForFile(full);
makeDir(pyramid);

if (endsWith(source,".jp2"))
    {
    thumb[strlen(thumb)-1]='g';  /* convert the extension */
    noLink = TRUE;   /* for jp2, symlink to source automatically suppressed */
    makeFullSize = TRUE;  /* instead we'll have the tile-maker make a fullsize jpg for us */
    quality[0] = 50;
    quality[1] = 60;
    quality[2] = 70;
    quality[3] = 80;
    quality[4] = 85;
    quality[5] = 85;
    quality[6] = 85;
    jp2DecInit(source, &nWidth, &nHeight);
    readScanline = &jp2ReadScanline;
    jpgTiles(nWidth, nHeight, outFullDir, outFullRoot, thumb, readScanline, quality, makeFullSize);
    jp2Destroy();
    }
else if (endsWith(source,".jpg"))
    {
    jpgDecInit(source, &nWidth, &nHeight);
    readScanline = &jpgReadScanline;
    jpgTiles(nWidth, nHeight, outFullDir, outFullRoot, thumb, readScanline, NULL, makeFullSize);
	/* NULL quality will default to 75 for all */
    jpgDestroy();
    }

if (!noLink)
    makeLink(source, full);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
noLink = optionExists("noLink");
if (argc != 5)
    usage();
vgPrepImage(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
