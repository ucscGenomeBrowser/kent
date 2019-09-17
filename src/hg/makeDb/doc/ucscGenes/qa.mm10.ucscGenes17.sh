# get new list of transcripts
hgsql mm10 -Ne "select name from knownGene" | sort > knownGene.lst
# get old list of transcripts
hgsql mm10 -Ne "select name from knownGeneOld9" | sort > knownGeneOld9.lst

hgsql mm10 -Ne "select chrom, txStart, txEnd,name from knownGene" | sort > knownGeneRange.bed
HGDB_CONF=$HOME/.hg.conf.beta hgsql mm10 -Ne "select chrom, txStart, txEnd,name from knownGene" | sort > oldKnownGeneRange.bed

# strip off version numbers
sed -e 's/\..*//' knownGene.lst > knownGeneNoVer.lst
sed -e 's/\..*//' knownGeneOld9.lst > knownGeneOld9NoVer.lst
sed -e 's/\..*//' knownGeneRange.bed > knownGeneRangeNoVer.bed
sed -e 's/\..*//' oldKnownGeneRange.bed > oldKnownGeneRangeNoVer.bed

featureBits mm10 oldKnownGeneRange.bed \!knownGeneRange.bed -bed=newCoverage.bed -enrichment
# oldKnownGeneRange.bed 40.961%, !knownGeneRange.bed 62.180%, both 0.402%, cover 0.98%, enrich 0.02x

featureBits mm10 knownGeneRange.bed \!oldKnownGeneRange.bed -bed=newCoverage.bed -enrichment
#knownGeneRange.bed 40.764%, !oldKnownGeneRange.bed 61.982%, both 0.204%, cover 0.50%, enrich 0.01x


# number of transcripts unscathed
join knownGene.lst knownGeneOld9.lst | wc -l
# 57239

# number of new transcripts (including compatible)
join -v 1 knownGene.lst knownGeneOld9.lst | wc -l
# 6575 

# number of lost transcripts (including compatible)
join -v 2 knownGene.lst knownGeneOld9.lst | wc -l
# 6520

# transcripts migrated  (same or version number increment)
join  knownGeneNoVer.lst knownGeneOld9NoVer.lst | wc -l
# 62512

# new transcripts 
join -v 1  knownGeneNoVer.lst knownGeneOld9NoVer.lst > new.lst
wc -l new.lst
# 1302
calc 1302/62512
# 0.020828

HGDB_CONF=$HOME/.hg.conf.beta hgsql mm10 -Ne "select name, category from kgTxInfo" | sed 's/\.[0-9]//g'  | sort > oldNameToCategory.tab
join lost.lst oldNameToCategory.tab | awk '{print $2}' | sort | uniq -c
#       3 antibodyParts
# 30 antisense
# 621 coding
# 179 nearCoding
# 414 noncoding

hgsql mm10 -Ne "select name, category from kgTxInfo" | sed 's/\.[0-9]//g'  | sort > nameToCategory.tab
join new.lst nameToCategory.tab | awk '{print $2}' | sort | uniq -c
# 3 antibodyParts
# 27 antisense
# 723 coding
# 65 nearCoding
# 484 noncoding


sort -k 4 knownGeneRangeNoVer.tab | join -1 4 -2 1 /dev/stdin new.lst | cut -f 2- -d ' ' > newRange.bed

# lost transcripts 
join -v 2  knownGeneNoVer.lst knownGeneOld9NoVer.lst > lost.lst
wc -l lost.lst
# 1247

sort -k 4 oldKnownGeneRangeNoVer.tab | join -1 4 -2 1 /dev/stdin lost.lst | cut -f 2- -d ' ' > lostRange.bed


# get the source acc from the new knownGene
hgsql mm10 -Ne "select name, sourceAcc from kgTxInfo" | sort > newSourceAcc.tab
sed 's/\.[0-9]//g' newSourceAcc.tab | sort > newSourceAccNoVer.tab

HGDB_CONF=$HOME/.hg.conf.beta hgsql mm10 -Ne "select name, sourceAcc from kgTxInfo" | sort > oldSourceAcc.tab
sed 's/\.[0-9]//g' oldSourceAcc.tab | sort > oldSourceAccNoVer.tab

join new.lst newSourceAccNoVer.tab > newAcc.lst
join lost.lst oldSourceAccNoVer.tab > lostAcc.lst
awk '{print $2}' lostAcc.lst | sort -u > lostSource.lst
stripPatentIds lostSource.lst lostNoPatents.lst


hgsql mm10 -Ne "select qName,tName, tStart, tEnd from ncbiRefSeqPsl"  | sort  > ncbiRefSeqRange.tab
sed 's/\.[0-9]//g' ncbiRefSeqRange.tab > ncbiRefSeqRangeNoVer.tab
hgsql mm10 -Ne "select qName,tName, tStart, tEnd from refSeqAli"  | sort  > refSeqRange.tab
sed 's/\.[0-9]//g' refSeqRange.tab > refSeqRangeNoVer.tab

join ncbiRefSeqRangeNoVer.tab refSeqRangeNoVer.tab | awk '{overlap=1;if ($2 == $5) {if ($3> $6) s=$3; else s=$6; if ($4 > $7) e = $7; else e = $4; overlap = e - s; if (overlap <= 0 ) print;}}' | awk '{print $1}' | uniq > tmp
join ncbiRefSeqRangeNoVer.tab refSeqRangeNoVer.tab -v 1 | awk '{print $1}' >> tmp
join ncbiRefSeqRangeNoVer.tab refSeqRangeNoVer.tab -v 2 | awk '{print $1}' >> tmp
sort tmp > refSeqDiffPosition.lst

join -v 1 lostNoPatents.lst refSeqDiffPosition.lst > lostNoRefDiff.lst
wc -l lostNoRefDiff.lst
#782

hgsql mm10 -Ne "select qName,qSize,matches+repMatches from ncbiRefSeqPsl" | sort > ncbiRefSizes.tab
sed 's/\.[0-9]//g' ncbiRefSizes.tab | sort > ncbiRefSizesNoVer.tab


sort -k 2 oldSourceAccNoVer.tab | join -1 2 -2 1 /dev/stdin lostNoRefDiff.lst  > lostUcIds.lst
wc -l lostUcIds.lst
# 894

hgsql hgFixed -Ne "select acc, moddate from gbCdnaInfo" | sort > moddate.tab
sort -k 2 newAcc.lst | join -1 2 -2 1 /dev/stdin moddate.tab > newAccDate.tab


 hgsql mm10 -Ne "select transcript from knownCanonical" | sort > knownCanonical.lst
  HGDB_CONF=$HOME/.hg.conf.beta hgsql mm10 -Ne "select transcript from knownCanonical" | sort > knownCanonicalOld.lst
wc knownCanonical.lst knownCanonicalOld.lst 
# 32989  32989 362879 knownCanonical.lst
# 33079  33079 363869 knownCanonicalOld.lst
join -v 1 knownCanonical.lst knownCanonicalOld.lst  | wc
# 3470
join -v 2 knownCanonical.lst knownCanonicalOld.lst  | wc
# 3560
calc 3560/33079
# 0.107621

sed -e 's/\..*//' knownCanonical.lst > knownCanonicalNoVer.lst
sed -e 's/\..*//' knownCanonicalOld.lst > knownCanonicalOldNoVer.lst
join -v 2 knownCanonical*NoVer.lst  | wc
# 1060
join -v 1 knownCanonical*NoVer.lst  | wc
# 970

calc 1060/33079
# 0.032044



