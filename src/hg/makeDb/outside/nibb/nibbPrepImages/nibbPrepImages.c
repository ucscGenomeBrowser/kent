/* nibbPrepImages - Set up NIBB frog images for VisiGene virtual 
 * microscope - copying them to a directory and makeing up pyramid scheme.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dystring.h"


int thumbSize = 200;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibbPrepImages - Set up NIBB frog images for VisiGene virtual\n"
  "microscope - copying them to a directory and makeing up pyramid scheme.\n"
  "usage:\n"
  "   nibbPrepImages sourceDir tabFile destThumb destPyramid\n"
  "Here sourceDir is directory full of images from NIBB.\n"
  "The tabFile is created with nibbParseImageDir.\n"
  "The destThumb is where to put the resulting thumbnail images,\n"
  "and destPyramid is where to put the pyramid scheme images.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void execute(char *command)
/* Make system call - if there's a problem abort. */
{
int result;
verbose(1, "%s\n", command);
result = system(command);
if (result < 0)
    errAbort("program failed code %d on:\n\t%s\n", result, command);
}

void makeThumbnail(char *source, char *dest)
/* Make a thumbnail-sized copy of source in dest. */
{
struct dyString *command = dyStringNew(0);
dyStringPrintf(command, "convert %s -resize %d %s", source, thumbSize, dest);
execute(command->string);
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
execute(dy->string);
}

void makePyramid(char *source, char *destDir,  char *name)
/* Make a directory in dest full of tiled versions of
 * source image at various levels of zoom. */
{
struct dyString *dy = dyStringNew(0);
char subdir[PATH_LEN];

safef(subdir, sizeof(subdir), "%s/%s", destDir, name);
makeDir(subdir);
pyramid1(source, subdir, name, dy, "", 0);
pyramid1(source, subdir, name, dy, "-resize 50%", 1);
pyramid1(source, subdir, name, dy, "-resize 25%", 2);
pyramid1(source, subdir, name, dy, "-resize 12.5%", 3);
pyramid1(source, subdir, name, dy, "-resize 6.25%", 4);
dyStringClear(dy);
dyStringPrintf(dy, "ln -s %s %s/%s.jpg", source, destDir, name);
execute(dy->string);
}

void nibbPrepImages(char *sourceDir, char *tabFile, 
	char *destThumb, char *destPyramid)
/* nibbPrepImages - Set up NIBB frog images for VisiGene virtual 
 * microscope - copying them to a directory and makeing up pyramid scheme.. */
{
struct lineFile *lf = lineFileOpen(tabFile, TRUE);
char *row[6];
struct hash *subdirHash = hashNew(0);

makeDir(destThumb);
makeDir(destPyramid);

while (lineFileRow(lf, row))
    {
    char *dir = row[3];
    char *subdir = row[4];
    char *file = row[5];
    char source[PATH_LEN], thumb[PATH_LEN], pyramidDir[PATH_LEN];

    /* Create full paths to input and output. */
    safef(source, sizeof(source), "%s/%s/%s/%s", sourceDir, dir, subdir, file);
    safef(thumb, sizeof(thumb), "%s/%s/%s", destThumb, subdir, file);
    safef(pyramidDir, sizeof(pyramidDir), "%s/%s", destPyramid, subdir);

    /* Make new image directories if need be. */
    if (!hashLookup(subdirHash, subdir))
        {
	char newDir[PATH_LEN];
	safef(newDir, sizeof(newDir), "%s/%s", destThumb, subdir);
	makeDir(newDir);
	safef(newDir, sizeof(newDir), "%s/%s", destPyramid, subdir);
	makeDir(newDir);
	uglyf("Creating %s\n", subdir);
	hashAdd(subdirHash, subdir, NULL);
	}

    makeThumbnail(source, thumb);
    chopSuffix(file);
    makePyramid(source, pyramidDir, file);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
nibbPrepImages(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
