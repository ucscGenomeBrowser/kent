This is the automated pipeline to update uniprot tracks from UniProt.org

UniProt updates its files every month, see http://www.uniprot.org/news/

Our process is this (doUniprot):

- compare local date against ftp and do nothing is not new
- if the file has not been updated on the uniprot.org server, exit
- parse the uniprot data from XML to tab files, one per organism
- create an alignment uniprotId -> genome with MarkD's pslMap and pslSelect
- run uniprotLift and use this tab-sep file and the alignment to create the bigBed files

- for genomes with no knownGene track, we cannot use the UniProt <-> gene mapping we already have.
So for protein sequence that match multiple times exactly identical, we do not know where to map them.
I did a comparison using hg38 and knownGene, once with the table, once without:

# the worst placements:
less unipToKnownGenesHg38NoPairsBeforeVarFix.psl | cut -f10 | tabUniq -rs | awk '($2>1)' | wc -l
# 2385 out of 38931 are multi-mapping, up to 35 times
# most of them are mapping twice, but some 35 times
#   2 ************************************************************ 1477
#   3 ********** 238
#   4 ***** 126
#   5 **** 109
#   6 *** 84
#   7 ****** 142
#   8 *** 82
#   9  10
#  10 *** 66
#  11  1
#  12  3
#  13  6
#  14  6
#  15  6
#  16  6
#  17  7
#  18  3
#  19  3
#  20  3
# <minVal or >= 21  7

#A6NER0  20      0.042%
#Q99706-6        20      0.042%
#P43629-2        20      0.042%
#P43630-2        28      0.059%
#P43630-1        28      0.059%
#Q99706-3        30      0.063%
#Q99706-4        30      0.063%
#Q99706-2        30      0.063%
#Q99706-1        30      0.063%
#Q8N743  32      0.067%

