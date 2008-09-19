/* info - UCSC Known Genes Model Info section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "web.h"
#include "txInfo.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: info.c,v 1.4 2008/09/19 20:21:31 kuhn Exp $";

void doTxInfoDescription(struct sqlConnection *conn)
/* Put up info on fields in txInfo table. */
{
cartWebStart(cart, database, "Gene Model Information Table Fields");
printf("%s",
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
"(CDS) is a coincidental open reading frame  rather than a true indication \n"
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
"that the genome sequence is in error, or that the anonymous DNA donor carried\n"
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
cartWebEnd();
}


static void showInfoTable(struct sqlConnection *conn, char *geneName, char *txInfoTable)
/* Print out stuff from txInfo table. */
{
if (!sqlTableExists(conn, txInfoTable))
    return;
char query[512];
safef(query, sizeof(query), "select * from %s where name='%s'", txInfoTable, geneName);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct txInfo *info = txInfoLoad(row);
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

static void infoPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out UCSC KG info. */
{
showInfoTable(conn, geneId, "kgTxInfo");
hPrintf("Click ");
hPrintf("<A HREF=\"../cgi-bin/hgGene?%s=1&%s\">", 
	hggDoTxInfoDescription, cartSidUrlString(cart));
hPrintf("here</A>\n");
hPrintf(" for a detailed description of the fields of the table above.<BR>");
}

static boolean infoExists(struct section *section,
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if info exists and has data. */
{
return sqlTablesExist(conn, "kgTxInfo");
}

struct section *infoSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create UCSC KG Model Info section. */
{
struct section *section = sectionNew(sectionRa, "info");
section->exists = infoExists;
section->print = infoPrint;
return section;
}

