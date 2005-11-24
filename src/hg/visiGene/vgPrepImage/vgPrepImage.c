/* vgPrepImage - Create thumbnail and image pyramid scheme for image, also link in full sized image. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: vgPrepImage.c,v 1.2 2005/11/24 03:34:50 kent Exp $";

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

int thumbSize=200;

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

void makeThumbnail(char *source, char *dest)
/* Make a thumbnail-sized copy of source in dest. */
{
struct dyString *command = dyStringNew(0);
dyStringPrintf(command, "convert %s -resize %d %s", source, thumbSize, dest);
execute(command->string, 0);
dyStringFree(&command);
}

void pyramid1(char *source, char *subdir, char *name, struct dyString *dy,
	char *sizeFlag, int level)
/* Call image magick convert program to produce scaled tiles. */
{
dyStringClear(dy);
dyStringPrintf(dy, 
	"convert %s %s -crop 512x512 -quality 75 %s/%s_%d_%%03d.jpg",
	source, sizeFlag, subdir, name, level);
execute(dy->string, 139);  /* convert will return 139 when ok.  Weird. */
}

void makePyramid(char *source, char *destDir)
/* Make a directory in dest full of tiled versions of
 * source image at various levels of zoom. */
{
struct dyString *dy = dyStringNew(0);
char name[PATH_LEN];

splitPath(destDir, NULL, name, NULL);
makeDir(destDir);
pyramid1(source, destDir, name, dy, "", 0);
pyramid1(source, destDir, name, dy, "-resize 50%", 1);
pyramid1(source, destDir, name, dy, "-resize 25%", 2);
pyramid1(source, destDir, name, dy, "-resize 12.5%", 3);
pyramid1(source, destDir, name, dy, "-resize 6.25%", 4);
dyStringFree(&dy);
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

/* Figure out full paths. */
safef(source, sizeof(source), "%s/%s", sourceDir, fileName);
safef(thumb, sizeof(thumb), "%s/%s", thumbDir, fileName);
safef(full, sizeof(full), "%s/%s", fullDir, fileName);
strcpy(pyramid, full);
chopSuffix(pyramid);

makeDirForFile(thumb);
makeDirForFile(full);

/* Make copies at various sizes. */
makeThumbnail(source, thumb);
makePyramid(source, pyramid);
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
