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

static char const rcsid[] = "$Id: txCdsInfo.c,v 1.8 2008/09/03 19:19:08 markd Exp $";

void showTxInfo(char *geneName, struct trackDb *tdb, char *txInfoTable)
/* Print out stuff from txInfo table. */
{
struct sqlConnection *conn = hAllocConn(database);
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
	
	webPrintLinkCell("<B>txCdsPredict score:</B>");
	webPrintDoubleCell(info->cdsScore);
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

printf("%s",
"The table above summarizes many aspects of this transcripts.  Here is a more \n"
"detailed description of each of the fields than can fit in the label.  Also\n"
"see the CDS Prediction Information table below for additional information\n"
"relevant to the predicted protein product if any.\n"
"\n"
"<UL>\n"
"<LI><B>category</B> - This is either <i>coding</i>, <i>noncoding</i>,\n"
"<i>antisense</i> or \n"
"<i>nearCoding</i>. A coding transcript is one where the evidence is relatively\n"
"good that it produces a protein. The nearCoding transcripts overlap coding\n"
"transcripts by at least 20 bases on the same strand, but themselves do not seem to produce \n"
"protein products.  In many cases this is because they are splicing varients\n"
"with introns after the stop codon, that therefore undergo nonsense mediated decay. \n"
"Antisense transcripts overlap coding transcripts by at least 20 bases on the oppposite\n"
"strand.\n"
"The other transcripts, which are neither coding, nor overlapping coding,\n"
"are categorized as noncoding.</LI>\n"
"<LI><B>exon count</B> - The number of exons in the gene. Single exon genes are\n"
"generally somewhat less reliable than multi-exon genes, though there are\n"
"many well-known genuine single exon genes such as the Histones and the Sox \n"
"family.</LI>\n"
"<LI><B>ORF size</B> - The size of the open reading frame in the mRNA. Divide by\n"
"three to get the size of the protein.</LI>\n"
"<LI><B>txCdsPredict score</B> - The score from the txCdsPredict program. This\n"
"program weighs a variety of evidence including the presence of a Kozak consensus\n"
"sequence at the start codon, the length of the ORF, the presense of upstream\n"
"ORFs, homology in other species, and nonsense mediated decay.  In general\n"
"a score over 1000 is almost certain to be a protein, and scores over 800 have\n"
"about a 90% chance of being a protein.</LI>\n"
"<LI><B>has start codon</B> - Indicates if the initial codon is an ATG.</LI>\n"
"<LI><B>has end codon</B> - Indicates if the last codon is TAA, TAG, or TGA.</LI>\n"
"<LI><B>selenocysteine</B> - Indicates if this is one of the special proteins where\n"
"TGA encodes the animo acid selenocysteine rather than encoding a stop codon.</LI>\n"
"<LI><B>nonsense-mediated-decay</B> - Indicates whether the final intron is more than\n"
"55 bases after the stop codon.  If true, then generally the mRNA will be degraded\n"
"before it can produce a detectable amount of protein. Therefore when this condition\n"
"is true we remove the predicted coding region  from the transcript.</LI>\n"
"<LI><B>CDS single in 3' UTR</B> - This is a strong indicator that the coding region\n"
"(CDS) is a coincedental open reading frame  rather than a true indication \n"
"that the transcript codes for protein.  This indicates that the coding sequence \n"
"resides in a single exon, and that this exon is located entirely in the 3' UTR \n"
"of another transcript that codes for a different protein not overlapping the \n"
"ORF in the same frame. We remove the CDS from non-refSeq transcripts that meet\n"
"this condition, which often results from a retained intron or from missing the\n"
"initial parts of a transcript.</LI>\n"
"<LI><B>CDS single in intron</B> - This is another strong indicator that the ORF is\n"
"not real. Here the coding region (CDS) lies entirely in the intron of another\n"
"transcript which has strong evidence of coding for a protein. We remove the CDS\n"
"from non-refSeq transcripts that meet this condition, which generally results\n"
"from a retained intron.</LI>\n"
"<LI><B>frame shift in genome</B> - This only occurs for RefSeq transcripts. Here\n"
"a frame shift is detected in the coding region when aligning the transcript against\n"
"the genome. Since RefSeq does examine these cases carefully, it is strong evidence\n"
"that the genome sequence is in error, or that the anonymous DNA doner carried\n"
"a frame-shift mutation in this gene.  In general there will be multiple independent\n"
"cDNA clones supporting the RefSeq over the genome.  In the gene display on the\n"
"browser, one or two bases will be removed from the gene to keep frame intact.</LI>\n"
"<LI><B>stop codon in genome</B> - This also only occurs for RefSeq transcripts, and\n"
"as with the frame shifts, there is generally multiple lines of evidence suggesting\n"
"sequencing error or mutation in the reference genome. In the gene display on the\n"
"browser three bases will be removed from the gene to avoid the stop.</LI>\n"
"<LI><B>retained intron</B> - The transcript contains what is an intron in \n"
"an overlapping transcript on the same strand.  In many cases this indicates\n"
"that the transcript was not completely processed. Unless specific steps are\n"
"taken to isolate cytoplasmic rather than nuclear RNA, a certain fraction of the\n"
"RNA isolated for sequencing will be incompletely processed.  Transcripts with\n"
"retained introns should be viewed suspiciously, especially if they are long.\n"
"However there are cases where fully mature mRNA transcripts are made with \n"
"and without a particular intron, so transcripts with retained introns are not\n"
"eliminated from this gene set.</LI>\n"
"<LI><B>end bleed into intron</B> - Very often when an intron is retained, it is so\n"
"long that the next exon is not reached and sequenced. In this case the retained\n"
"intron can't be detected directly.  However high values of \"end bleed\" are\n"
"strongly suggestive of a retained intron. End bleed measures how far the end of a transcript extends into an intron defined by another overlapping transcript. Note\n"
"however that alternative promoters and alternative polyadenylation sites can\n"
"create end bleeds in fully mature transcripts.</LI>\n"
"<LI><B>RNA accession</B> - The RefSeq or Genbank/EMBL/DJJ accession on which\n"
"this transcript is most closely based. Note that the splice sites when possible\n"
"are taken from a consensus of RNA alignments rather than just from a single RNA.\n"
"For non-RefSeq genes the bases are taken from the genome rather than the RNA.\n"
"However the transcript does define which introns and exons are used to build\n"
"the transcript.</LI>\n"
"<LI><B>RNA size</B> - The size of the RNA on which this transcript is most\n"
"closely based, including the poly-A tail.</LI>\n"
"<LI><B>Alignment % ID</B> - Percentage identity within of alignment of RNA\n"
"to genome.</LI>\n"
"<LI><B>% Coverage</B> - The percentage of the RNA covered by the alignment to\n"
"genome. This excludes the poly-A tail.</LI>\n"
"<LI><B># of Alignments:</B> - The number of times this RNA aligns to the genome\n"
"at very high stringency.  More care must be taken in interpreting genes based\n"
"on transcripts with multiple alignments. We do substantial filtering to avoid\n"
"pseudogenes, but extremely recent, extremely complete pseudogenes may still\n"
"pass these filters and cause multiple alignments.</LI>\n"
"<LI><B># of AT/AC introns</B> - The number of introns in this transcript with\n"
"AT/AC rather than the usual GT/AG ends. There are roughly 300 genes with\n"
"legitimate AT/AC  introns.</LI>\n"
"<LI><B># of strange splices</B> - The number of introns that have ends which are\n"
"neither GT/AG, GC/AG, nor AT/AC. Many of these are the result of sequencing\n"
"errors, or polymorphisms between the DNA donors and the RNA donors.</LI>\n"
"</UL>\n");
}

void showCdsEvidence(char *geneName, struct trackDb *tdb, char *evTable)
/* Print out stuff from cdsEvidence table. */
{
struct sqlConnection *conn = hAllocConn(database);
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

