#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "cdsEvidence.h"
#include "txInfo.h"
#include "trackDb.h"
#include "hgc.h"

static char const rcsid[] = "$Id: txCdsInfo.c,v 1.1 2007/02/28 07:12:53 kent Exp $";

void showTxInfo(char *geneName, struct trackDb *tdb, char *txInfoTable)
/* Print out stuff from txInfo table. */
{
struct sqlConnection *conn = hAllocConn();
if (sqlTableExists(conn, txInfoTable))
    {
    char query[512];
    safef(query, sizeof(query), "select * from %s where name='%s'", txInfoTable, geneName);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    if ((row = sqlNextRow(sr)) != NULL)
        {
	struct txInfo *info = txInfoLoad(row);
	webNewSection("Transcript Information");
	webPrintLinkTableStart();

	webPrintLinkCell("<B>category:</B>");
	webPrintLinkCell(info->category);
	webPrintLinkCell("<B>nonsense-mediated-decay:</B>");
	webPrintLinkCell(info->nonsenseMediatedDecay  ? "yes" : "no");
	webPrintLinkCell("<B>RNA accession:</B>");
	webPrintLinkCell(info->sourceAcc);
	webPrintLinkTableNewRow();

	webPrintLinkCell("<B>exon count:</B>");
	webPrintIntCell(info->exonCount);
	webPrintLinkCell("<B>CDS single in 3' UTR:</B>");
	webPrintLinkCell(info->cdsSingleInUtr3 ? "yes" : "no");
	webPrintLinkCell("<B>RNA size:</B>");
	webPrintIntCell(info->sourceSize);
	webPrintLinkTableNewRow();

	webPrintLinkCell("<B>ORF size:</B>");
	webPrintIntCell(info->orfSize);
	webPrintLinkCell("<B>CDS single in intron:</B>");
	webPrintLinkCell(info->cdsSingleInIntron ? "yes" : "no");
	webPrintLinkCell("<B>Alignment % ID:</B>");
	webPrintDoubleCell(info->aliIdRatio*100);
	webPrintLinkTableNewRow();
	
	webPrintLinkCell("<B>bestorf score:</B>");
	webPrintDoubleCell(info->bestorfScore);
	webPrintLinkCell("<B>frame shift in genome:</B>");
	webPrintLinkCell(info->genomicFrameShift  ? "yes" : "no");
	webPrintLinkCell("<B>% Coverage:</B>");
	webPrintDoubleCell(info->aliCoverage*100);
	webPrintLinkTableNewRow();

	webPrintLinkCell("<B>has start codon:</B>");
	webPrintLinkCell(info->startComplete ? "yes" : "no");
	webPrintLinkCell("<B>stop codon in genome:</B>");
	webPrintLinkCell(info->genomicStop ? "yes" : "no");
	webPrintLinkCell("<B># of Alignments:</B>");
	webPrintIntCell(info->genoMapCount);
	webPrintLinkTableNewRow();

	webPrintLinkCell("<B>has end codon:</B>");
	webPrintLinkCell(info->endComplete ? "yes" : "no");
	webPrintLinkCell("<B>retained intron:</B>");
	webPrintLinkCell(info->retainedIntron ? "yes" : "no");
	webPrintLinkCell("<B># AT/AC introns</B>");
	webPrintIntCell(info->atacIntrons);
	webPrintLinkTableNewRow();

	webPrintLinkCell("<B>selenocysteine:</B>");
	webPrintLinkCell(info->selenocysteine  ? "yes" : "no");
	webPrintLinkCell("<B>end bleed into intron:</B>");
	webPrintIntCell(info->bleedIntoIntron);
	webPrintLinkCell("<B># strange splices:</B>");
	webPrintIntCell(info->strangeSplice);

	webPrintLinkTableEnd();

	txInfoFree(&info);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void showCdsEvidence(char *geneName, struct trackDb *tdb, char *evTable)
/* Print out stuff from cdsEvidence table. */
{
struct sqlConnection *conn = hAllocConn();
double bestScore = 0;
if (sqlTableExists(conn, evTable))
    {
    webNewSection("CDS Prediction Information");
    char query[512];
    safef(query, sizeof(query), 
	    "select count(*) from %s where name='%s'", evTable, geneName);
    if (sqlQuickNum(conn, query) > 0)
	{
	safef(query, sizeof(query), 
		"select * from %s where name='%s' order by score desc", evTable, geneName);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;

	webPrintLinkTableStart();
	webPrintLabelCell("ORF<BR>size");
	webPrintLabelCell("start in<BR>transcript");
	webPrintLabelCell("end in<BR>transcript");
	webPrintLabelCell("source");
	webPrintLabelCell("accession");
	webPrintLabelCell("ad-hoc<BR>score");
	webPrintLabelCell("start<BR>codon");
	webPrintLabelCell("end<BR>codon");
	webPrintLabelCell("piece<BR>count");
	webPrintLabelCell("piece list");
	webPrintLabelCell("frame");
	webPrintLinkTableNewRow();

	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct cdsEvidence *ev = cdsEvidenceLoad(row);
	    webPrintIntCell(ev->end - ev->start);
	    int i;
	    webPrintIntCell(ev->start+1);
	    webPrintIntCell(ev->end);
	    webPrintLinkCell(ev->source);
	    webPrintLinkCell(ev->accession);
	    webPrintLinkCellRightStart();
	    printf("%3.2f", ev->score);
	    bestScore = max(ev->score, bestScore);
	    webPrintLinkCellEnd();
	    webPrintLinkCell(ev->startComplete ? "yes" : "no");
	    webPrintLinkCell(ev->endComplete ? "yes" : "no");
	    webPrintIntCell(ev->cdsCount);
	    webPrintLinkCellRightStart();
	    for (i=0; i<ev->cdsCount; ++i)
		{
		int start = ev->cdsStarts[i];
		int end = start + ev->cdsSizes[i];
		printf("%d-%d ", start+1, end);
		}
	    webPrintLinkCellEnd();
	    webPrintLinkCellRightStart();
	    for (i=0; i<ev->cdsCount; ++i)
	        {
		if (i>0) printf(",");
	        printf("%d", ev->cdsStarts[i]%3 + 1);
		}
	    webPrintLinkCellEnd();
	    webPrintLinkTableNewRow();
	    }
	sqlFreeResult(&sr);
	webPrintLinkTableEnd();
	printf("This table shows CDS predictions for this transcript from a number of "
	    "sources including alignments against UniProt proteins, alignments against Genbank "
	    "mRNAs with CDS regions annotated by the sequence submitter, and "
	    "Victor Solovyev's bestorf program. Each prediction is assigned an ad-hoc score "
	    "score is based on several factors including the quality of "
	    "any associated alignments, the quality of the source, and the length of the "
	    "prediction.  For RefSeq transcripts with annotated CDSs the ad-hoc score "
	    "is over a million unless there are severe problems mapping the mRNA to the "
	    "genome.  In other cases the score generally ranges from 0 to 50,000. "
	    "The highest scoring prediction in this table is used to define the CDS "
	    "boundaries for this transcript.<P>If no score is 2000 or more, the transcript "
	    "is considered non-coding. In cases where the CDS is subject to "
	    "nonsense-mediated decay the CDS is removed.  The CDS is also removed "
	    "from transcripts when evidence points to it being in an artifact of an "
	    "incompletely processed transcript.  Specifically if the CDS is entirely "
	    "enclosed in the 3' UTR or an intron of a refSeq or other high quality "
	    "transcript, the CDS is removed.");
	}
    else
        {
	printf("no significant CDS prediction found, likely %s is noncoding",
		geneName);
	}
    }
hFreeConn(&conn);
}

