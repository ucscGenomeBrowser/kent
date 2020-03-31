# This doc assumes that the gencode* tables have been built on $db
db=hg38
release=V32
dir=/hive/data/genomes/$db/bed/mergeGene$release

mkdir -p $dir
cd $dir

# calculate score field with bitfields
hgsql $db -Ne "select * from gencodeAnnot$release" | cut -f 2- | sort > gencodeAnnot$release.txt
hgsql $db -Ne "select name,2 from gencodeAnnot$release" | sort  > knownCanon.txt
hgsql $db -Ne "select * from gencodeTag$release" | grep basic |  sed 's/basic/1/' | sort > knownTag.txt
hgsql $db -Ne "select transcriptId,transcriptClass from gencodeAttrs$release where transcriptClass='pseudo'" |  sed 's/pseudo/4/' > knownAttrs.txt
sort knownCanon.txt knownTag.txt knownAttrs.txt | awk '{if ($1 != last) {print last, sum; sum=$2; last=$1}  else {sum += $2; }} END {print last, sum}' | tail -n +2  > knownScore.txt

#ifdef NOTNOW
cat  << __EOF__  > colors.sed
s/coding/12, 12, 120/
s/nonCoding/0, 100, 0/
s/pseudo/255,51,255/
s/other/254, 0, 0/
__EOF__ 
#endif

hgsql $db -Ne "select * from gencodeAttrs$release" | tawk '{print $4,$10}' | sed -f colors.sed > colors.txt

#ifdef NOTNOW
hgsql $db -Ne "select * from gencodeToUniProt$release" | tawk '{print $1,$2}'|  sort > uniProt.txt
hgsql $db -Ne "select * from gencodeAnnot$release" | tawk '{print $1,$12}' | sort > gene.txt
join -a 1 gene.txt uniProt.txt > geneNames.txt
#endif

//genePredToBigGenePred -score=knownScore.txt  -colors=colors.txt -geneNames=geneNames.txt  -known gencodeAnnot$release.txt  gencodeAnnot$release.bgpInput
genePredToBigGenePred -score=knownScore.txt  -colors=colors.txt gencodeAnnot$release.txt stdout | sort -k1,1 -k2,2n >  gencodeAnnot$release.bgpInput

bedToBigBed -type=bed12+8 -tab -as=$HOME/kent/src/hg/lib/mergedBGP.as gencodeAnnot$release.bgpInput /cluster/data/$db/chrom.sizes $db.mergedBGP.bb

mkdir -p /gbdb/$db/mergedBGP
ln -s `pwd`/$db.mergedBGP.bb /gbdb/$db/mergedBGP/mergedBGP.bb
