/* txCdsEvFromBed - Make a cds evidence file (.tce) from an existing bed file.  
 * Used mostly in transferring CCDS coding regions currently.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txRnaAccs.h"

static char const rcsid[] = "$Id: txCdsEvFromBed.c,v 1.1 2008/03/18 16:42:48 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsEvFromBed - Make a cds evidence file (.tce) from an existing bed file.  Used mostly \n"
  "in transferring CCDS coding regions currently.\n"
  "usage:\n"
  "   txCdsEvFromBed cds.bed type tx.bed tx.ev output.tce\n"
  "where:\n"
  "   cds.bed is a bed12 format file with thickStart/thickEnd defining coding region\n"
  "   type is the name that will appear in the type column of output.tce\n"
  "   tx.bed is a bed12 format file containing the transcripts we're annotating\n"
  "   tx.ev is an evidence file (from txWalk) which tells us which cds.bed items to apply\n"
  "      to which tx.bed items\n"
  "   output.tce is the tab-delimited output in the same format used by txCdsEvFromRna\n"
  "      txCdsEvFromBed, txCdsPick and txCdsToGene.  It mostly defines the CDS in terms of\n"
  "      the virtual transcript\n"
  "example:\n"
  "    txCdsEvFromBed ccds.bed ccds txWalk.bed txWalk.ev ccds.tce\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

#ifdef SOON
		fprintf(f, "%s\t", psl->tName);
		fprintf(f, "%d\t", mappedStart);
		fprintf(f, "%d\t", mappedEnd);
		fprintf(f, "%s\t", source);
		fprintf(f, "%s\t", psl->qName);
		fprintf(f, "%d\t", score);
		fprintf(f, "%d\t", startsWith("atg", s));
		fprintf(f, "%d\t", isStopCodon(e-3));
		fprintf(f, "1\t");	/* Block count */
		fprintf(f, "%d,\t", mappedStart);
		fprintf(f, "%d,\n", mappedEnd - mappedStart);
#endif /* SOON  */

void *findFirstEvInHash(struct txRnaAccs *ev, struct hash *hash)
/* Find first piece of evidence in hash table. */
{
/* TODO - make this only find compatable CDSs, where the CDS is entirely inside, and shares
 * all splice sites. */
int i;
void *val = NULL;
for (i=0; i<ev->accCount; ++i)
    {
    char *acc = ev->accs[i];
    val = hashFindVal(hash, acc);
    if (val != NULL)
        break;
    }
return val;
}

void txCdsEvFromBed(char *cdsBedFile, char *tceType, char *txBedFile, char *txEvFile, 
	char *outFile)
/* txCdsEvFromBed - Make a cds evidence file (.tce) from an existing bed file.  Used mostly in transferring CCDS coding regions currently.. */
{
/* Load CDS beds into hash */
struct hash *cdsBedHash = newHash(18);
struct bed *cdsBed, *cdsBedList = bedLoadNAll(cdsBedFile, 12);
for (cdsBed = cdsBedList; cdsBed != NULL; cdsBed = cdsBed->next)
    hashAdd(cdsBedHash, cdsBed->name, cdsBed);
verbose(1, "Loaded %d CDS from %s\n", cdsBedHash->elCount, cdsBedFile);

/* Load evidence into hash */
struct hash *evHash = newHash(18);
struct txRnaAccs *ev, *evList = txRnaAccsLoadAll(txEvFile);
for (ev = evList; ev != NULL; ev = ev->next)
    hashAdd(evHash, ev->name, ev);
verbose(1, "Loaded evidence on %d transcripts from %s\n", evHash->elCount, txEvFile);

/* Stream through main BED file, outputting CCDS annotation if there is an associated CCDS
 * in evidence. */
FILE *f = mustOpen(outFile, "w");
struct bed *bed, *bedList = bedLoadNAll(txBedFile, 12);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct txRnaAccs *ev = hashMustFindVal(evHash, bed->name);
    struct bed *cdsBed = findFirstEvInHash(ev, cdsBedHash);
    if (cdsBed != NULL)
        {
	fprintf(f, "%s\t%s\n", bed->name, cdsBed->name);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txCdsEvFromBed(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
