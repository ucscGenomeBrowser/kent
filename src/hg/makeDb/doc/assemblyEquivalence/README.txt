#############################################################################
Procedure to determine equivalence between assemblies

The process is to compare md5sums of each assembly to determine
exact matches.  When there isn't an exact match, them compare
the md5sums of each sequence to determine how many match with
each other.  With enough matches, a partial equivalence can be
established.  There are sometimes small differences due to:
  1. different or non-existing chrM/MT sequence between assemblies
  2. duplicated sequences are usually eliminated in UCSC assemblies
  3. elimination of sequences under a size limit, for example,
     do not use any sequences under 100 bases in length

The md5sums are calculated with the automatic script: doIdKeys.pl
which only needs a 2bit file to perform the calculation.  There are
two files created by this process:
  1. <assemblyName>.idKeys.txt - individual md5sum of each sequence
  2. <assemblyName>.keySignature.txt - md5sum of all the individual idKeys sums
                    This is a single md5sum for the whole assembly

There are five different sources of sequence that will be compared:
1. UCSC database browser genomes
2. NCBI RefSeq assemblies
3. NCBI Genbank assemblies
4. Ensembl assemblies
XXX 5. UCSC assembly hub genomes - maybe

Priority will go to exact matches.  If there is an exact match,
the partial matches will not be checked.  When there is no exact match,
the closest partial match will be used.

#############################################################################
Working directory:

/hive/data/inside/assemblyEquivalence/

#############################################################################

Ensembl version 99 sequences were downloaded to:
  
  /hive/data/outside/ensembl/genomes/release-99/fasta/

The 'idKeys' signatures for these sequences were calculated in:

  /hive/data/outside/ensembl/genomes/release-99/idKeys/

A typical run of idKeys in one of those directories, for
example:

  cat /hive/data/outside/ensembl/genomes/release-99/idKeys/Vicugna_pacos/run.sh

set -beEu -o pipefail
cd /hive/data/outside/ensembl/genomes/release-99/idKeys/Vicugna_pacos
doIdKeys.pl -buildDir=`pwd` -twoBit=`pwd`/Vicugna_pacos.vicPac1.2bit "Vicugna_pacos.vicPac1"

All of these were set up with scripts to run them in parallel,
the 2bit files was constructed from the fasta, then this
doIdKeys.pl procedure was run which creates the two files of interest:

# -rw-rw-r-- 1  14164089 Jan 21 21:58 Vicugna_pacos.vicPac1.idKeys.txt
# -rw-rw-r-- 1        33 Jan 21 21:58 Vicugna_pacos.vicPac1.keySignature.txt

The keySignature.txt is a single md5sum of all the individual sequence
md5sums in the idKeys.txt file.  For example:

cat Vicugna_pacos.vicPac1.keySignature.txt
84f72c82109632aa712d85fe9d94d237

head Vicugna_pacos.vicPac1.idKeys.txt
000052eec7a560c419098fe75ce59ccd        scaffold_62780
00008c4565756b4429fabffa2e374b2c        scaffold_15650
00012aaa1678d5651863671f53c66e3e        scaffold_138339
00014ee45a4daceca84ba8d39c05fa93        scaffold_24830
00016bdfe2b6c9cd9d3145bc59057443        scaffold_195552
... etc ...

##### Gather the Ensembl keySignatures  ####################################
mkdir /hive/data/inside/assemblyEquivalence/ensembl
cd /hive/data/inside/assemblyEquivalence/ensembl

# There is a single file with all the name correspondences that will be useful:

time  rsync -av --stats rsync://ftp.ensembl.org/ensembl/pub/release-99/species_EnsemblVertebrates.txt ./

real    0m1.644s

find /hive/data/outside/ensembl/genomes/release-99/idKeys -type f \
  | grep keySignature.txt | while read K
do
  idKeysDir=`dirname "${K}"`
  id=`basename "${K}" | sed -e 's/.keySignature.txt//;'`
  idKeys="$idKeysDir/$id.idKeys.txt"
  fullCount=`cat $idKeys | wc -l`
  uniqueCount=`cut -f1 $idKeys | sort -u | wc -l`
  keySig=`cat "${K}"`
  printf "%s\t%s\t%d\t%d\n" "${keySig}" "${id}" "${fullCount}" "${uniqueCount}"
done | sort -k1,1 > ensembl.keySignatures.txt

##### Gather the UCSC database genomes keySignatures  ######################

mkdir /hive/data/inside/assemblyEquivalence/ucscDb
cd /hive/data/inside/assemblyEquivalence/ucscDb

### Avoiding an expensive 'find' operation on the hive directories, just
### check for specific named files:

ls -d /hive/data/genomes/* | while read dbDir
do
  db=`basename "${dbDir}"`
  keySig="${dbDir}/bed/idKeys/${db}.keySignature.txt"
  idKeysDir=`dirname "${keySig}"`
  if [ -s "${keySig}" ]; then
     idKeys="$idKeysDir/$db.idKeys.txt"
     fullCount=`cat $idKeys | wc -l`
     uniqueCount=`cut -f1 $idKeys | sort -u | wc -l`
     keySig=`cat "${keySig}"`
   printf "%s\t%s\t%d\t%d\n" "${keySig}" "${db}" "${fullCount}" "${uniqueCount}"
  fi
done | sort -k1,1 > ucscDb.keySignatures.txt

##### Gather the NCBI Refseq keySignatures  ######################
##### There are two types of 'idKeys' in the NCBI assemblies
##### one at the top level: GCx/012/345/678/accessionId/idKeys/
##### one at the build level: GCx/012/345/678/accessionId/trackData/idKeys/
##### the top level one is for the 'original' assembly from NCBI,
##### the build level one is for the assembly hub which has duplicate
##### contigs removed.  These are different key signatures when
##### duplicates have been removed.

mkdir /hive/data/inside/assemblyEquivalence/refseq
cd /hive/data/inside/assemblyEquivalence/refseq

### the maxdepth and mindepth seems to help speed up the find a lot, rather
### then letting run it without any limits:
find /hive/data/genomes/asmHubs/refseqBuild/GCF \
    -maxdepth 4 -mindepth 4 -type d | while read buildDir
do
  asmId=`basename "${buildDir}"`
  keySig="${buildDir}/idKeys/${asmId}.keySignature.txt"
  idKeysDir=`dirname "${keySig}"`
  if [ -s "${keySig}" ]; then
     idKeys="$idKeysDir/$asmId.idKeys.txt"
     fullCount=`cat $idKeys | wc -l`
     uniqueCount=`cut -f1 $idKeys | sort -u | wc -l`
     keySig=`cat "${keySig}"`
  printf "%s\t%s\t%d\t%d\n" "${keySig}" "${asmId}" "${fullCount}" "${uniqueCount}"
  fi
done | sort -k1,1 > refseq.keySignatures.txt

##### Gather the NCBI genbank keySignatures  ######################

mkdir /hive/data/inside/assemblyEquivalence/genbank
cd /hive/data/inside/assemblyEquivalence/genbank

find /hive/data/genomes/asmHubs/genbankBuild/GCA \
    -maxdepth 4 -mindepth 4 -type d | while read buildDir
do
  asmId=`basename "${buildDir}"`
  keySig="${buildDir}/idKeys/${asmId}.keySignature.txt"
  idKeysDir=`dirname "${keySig}"`
  if [ -s "${keySig}" ]; then
     idKeys="$idKeysDir/$asmId.idKeys.txt"
     fullCount=`cat $idKeys | wc -l`
     uniqueCount=`cut -f1 $idKeys | sort -u | wc -l`
     keySig=`cat "${keySig}"`
 printf "%s\t%s\t%d\t%d\n" "${keySig}" "${asmId}" "${fullCount}" "${uniqueCount}"
  fi
done | sort -k1,1 > genbank.keySignatures.txt

########################################################################
### construct exact matches
########################################################################

cd /hive/data/inside/assemblyEquivalence

~/kent/src/hg/makeDb/doc/assemblyEquivalence/exact.sh

## this exact.sh script constructs the files:
## ensembl.genbank.exact.txt  genbank.refseq.exact.txt  refseq.ucsc.exact.txt
## ensembl.refseq.exact.txt   genbank.ucsc.exact.txt    ucsc.ensembl.exact.txt
## ensembl.ucsc.exact.txt     refseq.ensembl.exact.txt  ucsc.genbank.exact.txt
## genbank.ensembl.exact.txt  refseq.genbank.exact.txt  ucsc.refseq.exact.txt

~/kent/src/hg/makeDb/doc/assemblyEquivalence/exactTableTsv.pl \
  | sort > table.2020-03-20.tsv

# for a first peek at the functioning of this table, load these
# exact matches:
hgLoadSqlTab hgFixed asmEquivalent \
   ~/kent/src/hg/lib/asmEquivalent.sql table.2020-03-20.tsv

# what does this look like:
hgsql -e 'desc asmEquivalent' hgFixed
# Field   Type    Null    Key     Default Extra
# source  varchar(255)    NO      MUL     NULL
# destination     varchar(255)    NO      MUL     NULL
# sourceAuthority enum('ensembl','ucsc','genbank','refseq')   NO        NULL
# destinationAuthority    enum('ensembl','ucsc','genbank','refseq')   NO  NULL
# matchCount      int(10) unsigned        NO              NULL
# sourceCount     int(10) unsigned        NO              NULL
# destinationCount        int(10) unsigned        NO              NULL

hgsql -e 'select count(*) from asmEquivalent' hgFixed
# +----------+
# | count(*) |
# +----------+
# |      858 |
# +----------+

hgsql -e 'show indexes from asmEquivalent' hgFixed

# list of UCSC databases exactly equivalent to RefSeq assemblies
hgsql -e 'select destination from asmEquivalent where sourceAuthority="refseq" and destinationAuthority="ucsc";' hgFixed

########################################################################
### construct close matches
########################################################################

mkdir /hive/data/inside/assemblyEquivalence/notExact
cd /hive/data/inside/assemblyEquivalence/notExact
~/kent/src/hg/makeDb/doc/assemblyEquivalence/allByAll.sh
### the output of this script is summarized in ./results/
### into two categories: match and notMatch.

### This allByAll.sh script eliminates all the 'exact' matches as found
### above.  For those assemblies remaining in each category that do not
### have an exact match:
###     ensembl refseq genbank ucsc
### and then pairs them up according to how many unique sequences they
### have, where 'unique' means without duplicate contigs.  If the number of
### unique sequences are close within +- 10 to each other, it runs a
### comparison procedure that checks how many of the sequences in each assembly
### match to each other.  If the match is within %10 of the number of sequences,
### it calls that a 'match', anything outside %10 in common are
### called 'notMatch'

### Finally, to get the lines that belong in the final asmEquivalent
### table in hgFixed:

sed -e 's/\tcount A .*//;' results/match.*.txt > notExact.table.2020-03-30.tsv

To load up both the exact and these notExact matches into the hgFixed table:

sort notExact.table.2020-03-30.tsv ../table.2020-03-30.tsv \
 | hgLoadSqlTab hgFixed asmEquivalent ~/kent/src/hg/lib/asmEquivalent.sql stdin

hgsql -e 'select count(*) from asmEquivalent;' hgFixed
+----------+
| count(*) |
+----------+
|     2118 |
+----------+

### There shouldn't be any duplicate entries:

hgsql -N -e 'select source,destination from asmEquivalent;' hgFixed \
    | sort | uniq -c | sort -rn | head
      1 zonAlb1 Zonotrichia_albicollis.Zonotrichia_albicollis-1.0.1
      1 zonAlb1 GCF_000385455.1_Zonotrichia_albicollis-1.0.1
      1 zonAlb1 GCA_000385455.1_Zonotrichia_albicollis-1.0.1
      1 xipMac1 GCA_000241075.1_Xiphophorus_maculatus-4.4.2
      1 xenTro9 Xenopus_tropicalis.Xenopus_tropicalis_v9.1
      1 xenTro9 GCA_000004195.3_Xenopus_tropicalis_v9.1
      1 xenLae2 GCF_001663975.1_Xenopus_laevis_v2
      1 xenLae2 GCA_001663975.1_Xenopus_laevis_v2
      1 wuhCor1 GCF_009858895.2_ASM985889v3
      1 vicPac2 GCA_000164845.3_Vicugna_pacos-2.0.2
hgsql -N -e 'select source,destination from asmEquivalent;' hgFixed \
   | sort | uniq -c | sort -rn | tail

      1 Anabas_testudineus.fAnaTes1.1   GCF_900324465.1_fAnaTes1.1
      1 Amphiprion_percula.Nemo_v1      GCA_003047355.1_Nemo_v1
      1 Amphiprion_ocellaris.AmpOce1.0  GCF_002776465.1_AmpOce1.0
      1 Amphiprion_ocellaris.AmpOce1.0  GCA_002776465.1_AmpOce1.0
      1 Amphilophus_citrinellus.Midas_v5        GCA_000751415.1_Midas_v5
      1 Ailuropoda_melanoleuca.ailMel1  ailMel1
      1 Ailuropoda_melanoleuca.ailMel1  GCF_000004335.2_AilMel_1.0
      1 Ailuropoda_melanoleuca.ailMel1  GCA_000004335.1_AilMel_1.0
      1 Acanthochromis_polyacanthus.ASM210954v1 GCF_002109545.1_ASM210954v1
      1 Acanthochromis_polyacanthus.ASM210954v1 GCA_002109545.1_ASM210954v1

# How many ensembl assemblies map to genbank or refseq assemblies:
hgsql -N -e 'select source,destination from asmEquivalent where
  sourceAuthority="ensembl" AND (destinationAuthority="genbank" OR
    destinationAuthority="refseq");' hgFixed | wc -l
# 354

# How many ucsc assemblies map to genbank or refseq assemblies:
hgsql -N -e 'select source,destination from asmEquivalent where
  sourceAuthority="ucsc" AND (destinationAuthority="genbank" OR
    destinationAuthority="refseq");' hgFixed | wc -l
# 350

# ucsc to only refseq:
hgsql -N -e 'select source,destination from asmEquivalent where
  sourceAuthority="ucsc" AND destinationAuthority="refseq";' hgFixed | wc -l
# 146

# ucsc to only genbank
hgsql -N -e 'select source,destination from asmEquivalent where
  sourceAuthority="ucsc" AND destinationAuthority="genbank";' hgFixed | wc -l
# 204

# How many ucsc assemblies map to ensembl assemblies:
hgsql -N -e 'select source,destination from asmEquivalent where
  sourceAuthority="ucsc" AND destinationAuthority="ensembl";' hgFixed | wc -l
# 85

# These counts should always be symmetrical, e.g. ensembl to ucsc
 hgsql -N -e 'select source,destination from asmEquivalent where
  sourceAuthority="ensembl" AND destinationAuthority="ucsc";' hgFixed | wc -l
# 85

# To see some of the highest differences, sort by column 1 and by column 2
# and check head or tail of such sorts, there shouldn't be any '0 0' columns
# as that would be an 'exact' match and that has already been ruled
# out:  (this could be done with the loaded table too)

awk -F$'\t' '{printf "%4d %4d\t%s\t%s\n", $6-$5,$7-$5,$1,$2}' \
   notExact.table.2020-03-30.tsv | sort -k1,1n -k3,3 | head
   0    1       Anabas_testudineus.fAnaTes1.1   GCF_900324465.1_fAnaTes1.1
   0    1       Aotus_nancymaae.Anan_2.0        GCF_000952055.2_Anan_2.0
   0    1       Aotus_nancymaae.Anan_2.0        aotNan1
   0    1       Astatotilapia_calliptera.fAstCal1.2     GCF_900246225.1_fAstCal1.2
   0    1       Astyanax_mexicanus_pachon.Astyanax_mexicanus-1.0.2      astMex1
   0    1       Betta_splendens.fBetSpl5.2      GCF_900634795.2_fBetSpl5.2
   0    1       Bos_indicus_hybrid.UOA_Brahman_1        GCA_003369695.2_UOA_Brahman_1
   0    1       CHM1    GCA_000306695.2_CHM1_1.1
   0    1       Canis_lupus_familiarisgreatdane.UMICH_Zoey_3.1  GCA_005444595.1_UMICH_Zoey_3.1
   0    1       Cercocebus_atys.Caty_1.0        cerAty1

awk -F$'\t' '{printf "%4d %4d\t%s\t%s\n", $6-$5,$7-$5,$1,$2}' \
   notExact.table.2020-03-30.tsv | sort -k1,1n | tail
 453  454       GCA_000231765.1_GadMor_May2010  gadMor1
 454  453       gadMor1 GCA_000231765.1_GadMor_May2010
 754  756       GCA_000708225.1_ASM70822v1      nipNip0
 756  754       nipNip0 GCA_000708225.1_ASM70822v1
1233 1234       GCA_900497805.2_bare-nosed_wombat_genome_assembly       GCF_900497805.2_bare-nosed_wombat_genome_assembly
1233 1234       GCA_900497805.2_bare-nosed_wombat_genome_assembly       Vombatus_ursinus.bare-nosed_wombat_genome_assembly
1234 1233       GCF_900497805.2_bare-nosed_wombat_genome_assembly       GCA_900497805.2_bare-nosed_wombat_genome_assembly
1234 1233       Vombatus_ursinus.bare-nosed_wombat_genome_assembly      GCA_900497805.2_bare-nosed_wombat_genome_assembly
1473 1473       GCA_000768395.1_Cpor_2.0        croPor0
1473 1473       croPor0 GCA_000768395.1_Cpor_2.0

# This could be done with the loaded table too:

hgsql -N -e 'select sourceCount-matchCount,destinationCount-matchCount,source,destination from asmEquivalent where
     matchCount!=sourceCount OR matchCount!=destinationCount;' hgFixed \
       | sort -k1,1n -k3,3 | head
0       1       Anabas_testudineus.fAnaTes1.1   GCF_900324465.1_fAnaTes1.1
0       1       Aotus_nancymaae.Anan_2.0        GCF_000952055.2_Anan_2.0
0       1       Aotus_nancymaae.Anan_2.0        aotNan1
0       1       Astatotilapia_calliptera.fAstCal1.2     GCF_900246225.1_fAstCal1.2
0       1       Astyanax_mexicanus_pachon.Astyanax_mexicanus-1.0.2      astMex1
0       1       Betta_splendens.fBetSpl5.2      GCF_900634795.2_fBetSpl5.2
0       1       Bos_indicus_hybrid.UOA_Brahman_1        GCA_003369695.2_UOA_Brahman_1
0       1       CHM1    GCA_000306695.2_CHM1_1.1
0       1       Canis_lupus_familiarisgreatdane.UMICH_Zoey_3.1  GCA_005444595.1_UMICH_Zoey_3.1
0       1       Cercocebus_atys.Caty_1.0        cerAty1

hgsql -N -e 'select sourceCount-matchCount,destinationCount-matchCount,source,destination from asmEquivalent where
     matchCount!=sourceCount OR matchCount!=destinationCount;' hgFixed \
       | sort -k1,1n -k3,3 | tail
453     454     GCA_000231765.1_GadMor_May2010  gadMor1
454     453     gadMor1 GCA_000231765.1_GadMor_May2010
754     756     GCA_000708225.1_ASM70822v1      nipNip0
756     754     nipNip0 GCA_000708225.1_ASM70822v1
1233    1234    GCA_900497805.2_bare-nosed_wombat_genome_assembly       GCF_900497805.2_bare-nosed_wombat_genome_assembly
1233    1234    GCA_900497805.2_bare-nosed_wombat_genome_assembly       Vombatus_ursinus.bare-nosed_wombat_genome_assembly
1234    1233    GCF_900497805.2_bare-nosed_wombat_genome_assembly       GCA_900497805.2_bare-nosed_wombat_genome_assembly
1234    1233    Vombatus_ursinus.bare-nosed_wombat_genome_assembly      GCA_900497805.2_bare-nosed_wombat_genome_assembly
1473    1473    GCA_000768395.1_Cpor_2.0        croPor0
1473    1473    croPor0 GCA_000768395.1_Cpor_2.0
