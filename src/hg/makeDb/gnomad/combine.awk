#!/usr/bin/awk -f
### # re-organizes the output of a join command into a valid bed12+
# should be run in a pipe, for example:
# join -t $'\t' -1 4 -2 7 gencode.bed12 pliByTranscript.trimmed | combineGencodePli.awk -v doTranscripts=true | sort -k1,1 -k2,2n > pliByTranscript.bed
#
# The doTranscripts argument tells the script which accession to use as the name field
# in the final bed, the ENST accession for by_transcript, and the ENSG accession for
# by_gene.
###
BEGIN {
    FS="\t";
    OFS="\t";
    isTranscripts=doTranscripts
}

{
chrom=$2
gnomadChrom = sprintf("chr%s", $13)
if (chrom != gnomadChrom) {
    # so far just the multiple mapping PAR regions
    printf "bad join: %s\n", $0 > "/dev/stderr"
    next
}

chromStart=$3
chromEnd=$4
if (isTranscripts == "true")
    name=$1
else
    name=$16

if ($29 == "NA") {
    score = -1
    if ($28 != "NA" && $27 != "NA" && $29 != "NA" && $30 != "NA" && $35 != "NA" && $36 != "NA") {
        # doesn't come up with this version but you never know
        printf "error: 'NA' value for pLI but not other metrics, line: %d\n", NR > "/dev/stderr"
        next
    }
    pLof=sprintf("pLoF exp: NA, obs: NA, pLI = NA, o/e = NA (NA)")
    mouseOver=sprintf("LOEUF: NA, pLI: NA")
} else {
    score=$29
    pLof=sprintf("pLoF exp: %.1f, obs: %d, pLI = %.2f, o/e = %.2f (%.2f - %.2f)", $28,$27,$29,$30,$35,$36)
    mouseOver=sprintf("LOEUF: %.2f, pLI: %.2f", $36, $29)
}
strand=$6
thickStart=$7
thickEnd=$8
itemRgb=""
if (score == -1) {itemRgb = "160,160,160"}
else if (score >= 0 && score < 0.1) {itemRgb = "0,244,153"}
else if (score >= 0.1 && score < 0.2) {itemRgb = "74,240,94"}
else if (score >= 0.2 && score < 0.3) {itemRgb = "127,233,58"}
else if (score >= 0.3 && score < 0.4) {itemRgb = "165,224,26"}
else if (score >= 0.4 && score < 0.5) {itemRgb = "191,210,22"}
else if (score >= 0.5 && score < 0.6) {itemRgb = "210,191,13"}
else if (score >= 0.6 && score < 0.7) {itemRgb = "224,165,8"}
else if (score >= 0.7 && score < 0.8) {itemRgb = "233,127,5"}
else if (score >= 0.8 && score < 0.9) {itemRgb = "240,74,3"}
else if (score >= 0.9 && score <= 1) {itemRgb = "244,0,2"}
else {
    printf "error: score '%s' out of range for gene/transcript: %s\n", score, name > "/dev/stderr"
}

if (score == -1)
    bedScore = 0
else {
    score=sprintf("%.2f", $29)
    bedScore=sprintf("%d",score*1000)
}

blockCount=$10
blockSizes=$11
blockStarts=$12
geneName=$18
missense=sprintf("Missense exp: %.1f, obs: %d, Z = %.2f, o/e = %.2f (%.2f - %.2f)", $20,$19,$22,$21,$33,$34)
synonymous=sprintf("Synonymous exp: %.1f, obs: %d, Z = %.2f, o/e = %.2f (%.2f - %.2f)", $24,$23,$26,$25,$31,$32)

print chrom, chromStart, chromEnd, name, bedScore, strand, thickStart, thickEnd, itemRgb, blockCount, blockSizes, blockStarts, mouseOver, score, geneName, synonymous, missense, pLof
}
