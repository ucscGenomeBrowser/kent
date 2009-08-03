/* bam -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifdef USE_BAM

#include "common.h"
#include "hdb.h"
#include "bamFile.h"

static char const rcsid[] = "$Id: bamFile.c,v 1.3 2009/08/03 22:14:39 angie Exp $";

static char *bbiNameFromTable(struct sqlConnection *conn, char *table)
/* Return file name from little table. */
/* This should be libified somewhere sensible -- copied from hgTracks/bedTrack.c. */
{
char query[256];
safef(query, sizeof(query), "select fileName from %s", table);
char *fileName = sqlQuickString(conn, query);
if (fileName == NULL)
    errAbort("Missing fileName in %s table", table);
return fileName;
}

void bamFetch(char *db, char *table, char *position, bam_fetch_f callbackFunc, void *callbackData)
/* Open the .bam file given in db.table, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData. 
 * Note: if sequences in .bam file don't begin with "chr" but cart position does, pass in 
 * cart position + strlen("chr") to match the .bam file sequence names. */
{
// TODO: if bamFile is URL, convert URL to path a la UDC, but under udcFuse mountpoint.
// new hg.conf setting for udcFuse mountpoint/support.
struct sqlConnection *conn = hAllocConn(db);
char *bamFileName = bbiNameFromTable(conn, table);
hFreeConn(&conn);

samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh == NULL)
    errAbort("samopen(%s, \"rb\") returned NULL", bamFileName);

int chromId, start, end;
int ret = bam_parse_region(fh->header, position, &chromId, &start, &end);
if (ret != 0)
    errAbort("bam_parse_region(%s) failed (%d)", position, ret);
//?? Could this happen if there is no data on some _random?  can avoid with tdb chromosomes...

bam_index_t *idx = bam_index_load(bamFileName);
ret = bam_fetch(fh->x.bam, idx, chromId, start, end, callbackData, callbackFunc);
if (ret != 0)
    errAbort("bam_fetch(%s, %s (chromId=%d) failed (%d)", bamFileName, position, chromId, ret);
samclose(fh);
}

char *bamGetQuerySequence(const bam1_t *bam)
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so here we rev-comp it back to restore the original query sequence. */
{
const bam1_core_t *core = &bam->core;
char *qSeq = needMem(core->l_qseq + 1);
uint8_t *s = bam1_seq(bam);
int i;
for (i = 0; i < core->l_qseq; i++)
    qSeq[i] = bam_nt16_rev_table[bam1_seqi(s, i)];
if ((core->flag & BAM_FREVERSE))
    reverseComplement(qSeq, core->l_qseq);
return qSeq;
}

char *bamGetCigar(const bam1_t *bam)
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
struct dyString *dyCigar = dyStringNew(min(8, core->n_cigar*4));
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    dyStringPrintf(dyCigar, "%d", n);
    dyStringAppendC(dyCigar, op);
    }
return dyStringCannibalize(&dyCigar);
}

int bamGetTargetLength(const bam1_t *bam)
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
int tLength=0;
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    tLength += n;
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tLength += n;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamGetTargetLength: unrecognized CIGAR op %c -- update me", op);
	}
    }
return tLength;
}

#endif//def USE_BAM
