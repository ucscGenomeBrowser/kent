/* encode2Manifest - manipulate a line of a manifest file of the type
 * used for importing data from encode2 into encode3 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "net.h"
#include "encode3/encode2Manifest.h"


struct encode2Manifest *encode2ManifestLoad(char **row)
/* Load a encode2Manifest from row fetched with select * from encode2Manifest
 * from database.  Dispose of this with encode2ManifestFree(). */
{
struct encode2Manifest *ret;

AllocVar(ret);
ret->fileName = cloneString(row[0]);
ret->format = cloneString(row[1]);
ret->outputType = cloneString(row[2]);
ret->experiment = cloneString(row[3]);
ret->replicate = cloneString(row[4]);
ret->enrichedIn = cloneString(row[5]);
ret->md5sum = cloneString(row[6]);
ret->size = sqlLongLong(row[7]);
ret->modifiedTime = sqlLongLong(row[8]);
ret->validKey = cloneString(row[9]);
return ret;
}

void encode2ManifestTabOut(struct encode2Manifest *mi, FILE *f)
/* Write tab-separated version of encode2Manifest to f */
{
fprintf(f, "%s\t", mi->fileName);
fprintf(f, "%s\t", mi->format);
fprintf(f, "%s\t", mi->outputType);
fprintf(f, "%s\t", mi->experiment);
fprintf(f, "%s\t", mi->replicate);
fprintf(f, "%s\t", mi->enrichedIn);
fprintf(f, "%s\t", mi->md5sum);
fprintf(f, "%lld\t", mi->size);
fprintf(f, "%lld\t", mi->modifiedTime);
fprintf(f, "%s\n", mi->validKey);
}

void encode2ManifestShortTabOut(struct encode2Manifest *mi, FILE *f)
/* Write tab-separated version of encode2Manifest to f */
{
fprintf(f, "%s\t", mi->fileName);
fprintf(f, "%s\t", mi->format);
fprintf(f, "%s\t", mi->outputType);
fprintf(f, "%s\t", mi->experiment);
fprintf(f, "%s\t", mi->replicate);
fprintf(f, "%s\n", mi->enrichedIn);
}

struct encode2Manifest *encode2ManifestLoadAll(char *fileName)
/* Load all encode2Manifests from file. */
{
struct lineFile *lf = netLineFileOpen(fileName);
char *row[ENCODE2MANIFEST_NUM_COLS];
struct encode2Manifest *list = NULL, *fi;
while (lineFileRow(lf, row))
   {
   fi = encode2ManifestLoad(row);
   slAddHead(&list, fi);
   }
slReverse(&list);
return list;
}

struct encode2Manifest *encode2ManifestShortLoadAll(char *fileName)
/* Read a short (just first 6 columns) manifest file. */
{
struct encode2Manifest *miList = NULL, *mi;
char *row[6];
struct lineFile *lf = netLineFileOpen(fileName);
while (lineFileRow(lf, row))
    {
    AllocVar(mi);
    mi->fileName = cloneString(row[0]);
    mi->format = cloneString(row[1]);
    mi->outputType = cloneString(row[2]);
    mi->experiment = cloneString(row[3]);
    mi->replicate = cloneString(row[4]);
    mi->enrichedIn = cloneString(row[5]);
    slAddHead(&miList, mi);
    }
lineFileClose(&lf);
slReverse(&miList);
return miList;
}

void encode2ManifestPrintHeader(FILE *f)
/* Write out header line. */
{
fputs("#file_name\tformat\toutput_type\texperiment\treplicate\tenriched_in", f);
fputs("\tmd5_sum\tsize\tmodified\tvalid_key\n", f);
}
