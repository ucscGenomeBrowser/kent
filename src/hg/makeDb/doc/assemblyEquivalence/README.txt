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
XXX 5. UCSC assembly hub genomes - maybe, this is the same ID as RefSeq/Genbank

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
mkdir /hive/data/inside/assemblyEquivalence/Ensembl
cd /hive/data/inside/assemblyEquivalence/Ensembl

# There is a single file with all the name correspondences that will be useful:

time  rsync -av --stats rsync://ftp.ensembl.org/ensembl/pub/release-99/species_EnsemblVertebrates.txt ./

real    0m1.644s

find /hive/data/outside/ensembl/genomes/release-99/idKeys -type f \
  | grep keySignature.txt | while read K
do
  id=`basename "${K}" | sed -e 's/.keySignature.txt//;'`
  keySig=`cat "${K}"`
  printf "%s\t%s\n" "${keySig}" "${id}"
done | sort -k1,1 > ens99.keySignatures.txt

##### Gather the UCSC database genomes keySignatures  ######################

mkdir /hive/data/inside/assemblyEquivalence/ucscDb
cd /hive/data/inside/assemblyEquivalence/ucscDb

### Avoiding an expensive 'find' operation on the hive directories, just
### check for specific named files:

ls -d /hive/data/genomes/* | while read dbDir
do
  db=`basename "${dbDir}"`
  keySig="${dbDir}/bed/idKeys/${db}.keySignature.txt"
  if [ -s "${keySig}" ]; then
     keySig=`cat "${keySig}"`
     printf "%s\t%s\n" "${keySig}" "${db}"
  fi
done | sort -k1,1 > ucscDb.keySignatures.txt

##### Gather the NCBI Refseq keySignatures  ######################

mkdir /hive/data/inside/assemblyEquivalence/refseq
cd /hive/data/inside/assemblyEquivalence/refseq

### the maxdepth and mindepth seems to help speed up the find a lot, rather
### then letting run it without any limits:
find /hive/data/genomes/asmHubs/refseqBuild/GCF \
    -maxdepth 4 -mindepth 4 -type d | while read buildDir
do
  asmId=`basename "${buildDir}"`
  keySig="${buildDir}/trackData/idKeys/${asmId}.keySignature.txt"
  if [ -s "${keySig}" ]; then
     keySig=`cat "${keySig}"`
     printf "%s\t%s\n" "${keySig}" "${asmId}"
  fi
done | sort -k1,1 > refseq.keySignatures.txt

##### Gather the NCBI genbank keySignatures  ######################

mkdir /hive/data/inside/assemblyEquivalence/genbank
cd /hive/data/inside/assemblyEquivalence/genbank

find /hive/data/genomes/asmHubs/genbankBuild/GCA \
    -maxdepth 4 -mindepth 4 -type d | while read buildDir
do
  asmId=`basename "${buildDir}"`
  keySig="${buildDir}/trackData/idKeys/${asmId}.keySignature.txt"
  if [ -s "${keySig}" ]; then
     keySig=`cat "${keySig}"`
     printf "%s\t%s\n" "${keySig}" "${asmId}"
  fi
done | sort -k1,1 > genbank.keySignatures.txt

########################################################################
