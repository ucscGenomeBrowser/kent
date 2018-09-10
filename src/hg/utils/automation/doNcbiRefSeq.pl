#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doNcbiRefSeq.pl instead.

# HOW TO USE THIS TEMPLATE:
# 1. Global-replace doNcbiRefSeq.pl with your actual script name.
# 2. Search for template and replace each instance with something appropriate.
#    Add steps and subroutines as needed.  Other do*.pl or make*.pl may have
#    useful example code -- this is just a skeleton.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

my $doIdKeys = "$Bin/doIdKeys.pl";
my $gff3ToRefLink = "$Bin/gff3ToRefLink.pl";
my $gbffToCds = "$Bin/gbffToCds.pl";
my $ncbiRefSeqOtherIxIxx = "$Bin/ncbiRefSeqOtherIxIxx.pl";
my $ncbiRefSeqOtherAttrs = "$Bin/ncbiRefSeqOtherAttrs.pl";

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_genbank
    $opt_subgroup
    $opt_species
    $opt_toGpWarnOnly
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download', func => \&doDownload },
      { name => 'process', func => \&doProcess },
      { name => 'load', func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';
my $bigClusterHub = 'ku';
my $smallClusterHub = 'ku';
my $workhorse = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $defaultFileServer = 'hgwdev';
my $fileServer = 'hgwdev';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base [options] genbank|refseq subGroup species asmId db
required arguments:
    genbank|refseq - specify either genbank or refseq hierarchy source
    subGroup       - specify subGroup at NCBI FTP site, examples:
                   - vertebrate_mammalian vertebrate_other plant etc...
    species        - species directory at NCBI FTP site, examples:
                   - Homo_sapiens Mus_musculus etc...
    asmId          - assembly identifier at NCBI FTP site, examples:
                   - GCF_000001405.32_GRCh38.p6 GCF_000001635.24_GRCm38.p4 etc..
    db             - database to load with track tables

options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ncbiRefSeq.\$date
                          (necessary when continuing at a later date).
    -toGpWarnOnly         add -warnAndContinue to the gff3ToGenePred operation
                          to avoid gene definitions that will not convert
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
					'workhorse' => $defaultWorkhorse,
					'fileServer' => $defaultFileServer,
					'bigClusterHub' => $bigClusterHub,
					'smallClusterHub' => $smallClusterHub);
  print STDERR "
Automates UCSC's ncbiRefSeq track build.  Steps:
    download: symlink local files or rsync required files from NCBI FTP site
    process: generate the track data files from the download data
    load: load the processed data into database tables
    cleanup: Removes or compresses intermediate files.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ncbiRefSeq.\$date unless -buildDir is given.

Expects to find already done idKeys procedure and result file in:
   /hive/data/genomes/<db>/bed/idKeys/<db>.idKeys.txt
See also: doIdKeys.pl command.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: genbankRefseq subGroup species asmId db
my ($genbankRefseq, $subGroup, $species, $asmId, $db, $ftpDir);
# Other:
my ($buildDir, $toGpWarnOnly, $dbExists);
my ($secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'toGpWarnOnly',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
  $workhorse = $opt_workhorse if ($opt_workhorse);
  $bigClusterHub = $opt_bigClusterHub if ($opt_bigClusterHub);
  $smallClusterHub = $opt_smallClusterHub if ($opt_smallClusterHub);
  $fileServer = $opt_fileServer if ($opt_fileServer);
}

#########################################################################
# * step: download [workhorse]
sub doDownload {
  my $runDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download required set of files from NCBI.";
  my $bossScript = newBash HgRemoteScript("$runDir/doDownload.bash", $workhorse,
				      $runDir, $whatItDoes);
  my $outsideCopy = "/hive/data/outside/ncbi/$ftpDir";
  my $localData = "/hive/data/inside/ncbi/$ftpDir";
  $localData =~ s/all_assembly_versions/latest_assembly_versions/;
  my $local2Bit = "$localData/$asmId.ncbi.2bit";

  # establish variables
  $bossScript->add(<<_EOF_
# establish all potential variables to use here, not all may be used

export outsideCopy=$outsideCopy
export asmId=$asmId
export ftpDir=$ftpDir
export runDir=$runDir
export db=$db

_EOF_
    );

printf STDERR "# checking $outsideCopy\n";

  # see if local symLinks can be made with copies already here from NCBI:
  if ( -d "$outsideCopy" ) {
    $bossScript->add(<<_EOF_
# local file copies exist, use symlinks

ln -f -s \$outsideCopy/\${asmId}_genomic.gff.gz .
ln -f -s \$outsideCopy/\${asmId}_rna.fna.gz .
ln -f -s \$outsideCopy/\${asmId}_rna.gbff.gz .
ln -f -s \$outsideCopy/\${asmId}_protein.faa.gz .
_EOF_
    );
  } else {
    $bossScript->add(<<_EOF_
# local file copies do not exist, download from NCBI:

for F in _rna.gbff _rna.fna _protein.faa _genomic.gff
do
   rsync -a -P \\
       rsync://ftp.ncbi.nlm.nih.gov/\$ftpDir/\$asmId\${F}.gz ./
done
_EOF_
    );
  }

printf STDERR "# checking $local2Bit\n";
  if ( -s $local2Bit ) {
    $bossScript->add(<<_EOF_
ln -f -s $local2Bit .
_EOF_
    );
  } elsif ( -s "$outsideCopy/${asmId}_genomic.fna.gz") {
    $bossScript->add(<<_EOF_
# build \$asmId.ncbi.2bit from local copy of genomic fasta

faToTwoBit \$outsideCopy/\${asmId}_genomic.fna.gz \${asmId}.ncbi.2bit
_EOF_
    );
  } else {
    $bossScript->add(<<_EOF_
# download genomic fasta and build \${asmId}.ncbi.2bit
rsync -a -P \\
       rsync://ftp.ncbi.nlm.nih.gov/\$ftpDir/\${asmId}_genomic.fna.gz ./
faToTwoBit \${asmId}_genomic.fna.gz \${asmId}.ncbi.2bit
_EOF_
    );
  }

  my $haveLiftFile = 0;
  if ($dbExists) {
     # check if db.ucscToRefSeq exists?
     my $sql = "show tables like 'ucscToRefSeq';";
     my $result = `hgsql $db -BN -e "$sql"`;
     chomp $result;
     if ( $result =~ m/ucscToRefSeq/ ) {
       $haveLiftFile = 1;
       $bossScript->add(<<_EOF_
cd \$runDir
hgsql -N -e 'select 0,name,chromEnd,chrom,chromEnd from ucscToRefSeq;' \${db} \\
     > \${asmId}To\${db}.lift
_EOF_
       );
    }
  }

  if (! $haveLiftFile ) {
       $bossScript->add(<<_EOF_
# generate idKeys for the NCBI sequence to translate names to UCSC equivalents

rm -rf \$runDir/idKeys
mkdir -p \$runDir/idKeys
cd \$runDir/idKeys
time ($doIdKeys -buildDir=\$runDir/idKeys -twoBit=\$runDir/\$asmId.ncbi.2bit \$db) > idKeys.log 2>&1
cd \$runDir
ln -f -s idKeys/\$db.idKeys.txt ./ncbi.\$asmId.idKeys.txt
ln -f -s /hive/data/genomes/\$db/bed/idKeys/\$db.idKeys.txt ./ucsc.\$db.idKeys.txt
# joining the idKeys establishes a lift file to translate chrom names
join -t'\t' ucsc.\$db.idKeys.txt ncbi.\$asmId.idKeys.txt \\
   | cut -f2-3 | sort \\
     | join -t'\t' - <(sort $HgAutomate::clusterData/\$db/chrom.sizes) \\
       | awk -F'\t' '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$2, \$3, \$1, \$3}' \\
          | sort -k3nr > \${asmId}To\${db}.lift
_EOF_
       );
  }

  $bossScript->add(<<_EOF_
cd \$runDir
twoBitInfo \$asmId.ncbi.2bit stdout | sort -k2nr > \$asmId.chrom.sizes
zcat \${asmId}_rna.fna.gz | sed -e 's/ .*//;' | gzip -c > \$asmId.rna.fa.gz
faSize -detailed \${asmId}.rna.fa.gz | sort -k2nr > rna.sizes

# genbank processor extracts infomation about the RNAs
/hive/data/outside/genbank/bin/x86_64/gbProcess \\
    -inclXMs /dev/null \$asmId.raFile.txt \${asmId}_rna.gbff.gz
_EOF_
  );
  $bossScript->execute();
} # doDownload

#########################################################################
# * step: process [dbHost]
sub doProcess {
  my $runDir = "$buildDir/process";
  my $downloadDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "process NCBI download files into track files.";
  # Use dbHost since genePredCheck -db=$db needs database
  my $bossScript = newBash HgRemoteScript("$runDir/doProcess.bash", $dbHost,
				      $runDir, $whatItDoes);

  my $warnOnly = "";
  $warnOnly = "-warnAndContinue" if ($toGpWarnOnly);

  my $dbTwoBit = "$HgAutomate::clusterData/$db/$db.2bit";
  $bossScript->add(<<_EOF_
# establish all variables to use here

export asmId=$asmId
export downloadDir=$downloadDir
export ncbiGffGz=\$downloadDir/\${asmId}_genomic.gff.gz
export db=$db
export gff3ToRefLink=$gff3ToRefLink
export gbffToCds=$gbffToCds

export annotationRelease=`zcat \$ncbiGffGz | head -100 | grep ^#.annotation-source | sed -e 's/.*annotation-source //'`
if [ "\$annotationRelease" == "" ]; then
  export annotationRelease=\$asmId
fi
export versionDate=`ls -L --full-time \$ncbiGffGz | awk '{print \$6;}'`
echo "\$annotationRelease (\$versionDate)" > ncbiRefSeqVersion.txt

# this produces the genePred in NCBI coordinates
# 8/23/17: gff3ToGenePred quits over illegal attribute SO_type... make it legal (so_type):
zcat \$ncbiGffGz \\
  | sed -re 's/([;\\t])SO_type=/\\1so_type=/;' \\
  | gff3ToGenePred $warnOnly -refseqHacks -attrsOut=\$asmId.attrs.txt \\
      -unprocessedRootsOut=\$asmId.unprocessedRoots.txt stdin \$asmId.gp
genePredCheck \$asmId.gp

# extract labels from semi-structured text in gbff COMMENT/description sections:
zcat \$downloadDir/\${asmId}_rna.gbff.gz \\
  | (grep ' :: ' || true) \\
    | perl -wpe 's/\\s+::.*//; s/^\\s+//;' \\
      | sort -u \\
        > pragmaLabels.txt

# extract cross reference text for refLink
\$gff3ToRefLink \$downloadDir/\$asmId.raFile.txt \$ncbiGffGz pragmaLabels.txt 2> \$db.refLink.stderr.txt \\
  | sort > \$asmId.refLink.tab

# converting the NCBI coordinates to UCSC coordinates
liftUp -extGenePred -type=.gp stdout \$downloadDir/\${asmId}To\${db}.lift drop \$asmId.gp \\
  | gzip -c > \$asmId.\$db.gp.gz
genePredCheck -db=\$db \$asmId.\$db.gp.gz

# curated subset of all genes
(zegrep "^[NY][MRP]_" \$asmId.\$db.gp.gz || true) > \$db.curated.gp
# may not be any curated genes
if [ ! -s \$db.curated.gp ]; then
  rm -f \$db.curated.gp
fi

# predicted subset of all genes
(zegrep "^X[MR]_" \$asmId.\$db.gp.gz || true) > \$db.predicted.gp

# not curated or predicted subset of all genes, the left overs
(zegrep -v "^[NXY][MRP]_" \$asmId.\$db.gp.gz || true) > \$db.other.gp

# curated and predicted without leftovers:
(zegrep "^[NXY][MRP]_" \$asmId.\$db.gp.gz || true) > \$db.ncbiRefSeq.gp

if [ -s \$db.curated.gp ]; then
  genePredCheck -db=\$db \$db.curated.gp
fi
if [ -s \$db.predicted.gp ]; then
  genePredCheck -db=\$db \$db.predicted.gp
fi
if [ -s \$db.other.gp ]; then
  genePredCheck -db=\$db \$db.other.gp
fi

# join the refLink metadata with curated+predicted names
cut -f1 \$db.ncbiRefSeq.gp | sort -u > \$asmId.\$db.name.list
join -t'\t' \$asmId.\$db.name.list \$asmId.refLink.tab > \$asmId.\$db.ncbiRefSeqLink.tab

# Make bigBed with attributes in extra columns for ncbiRefSeqOther:
genePredToBed \$db.other.gp stdout | sort -k1,1 -k2n,2n > \$db.other.bed
$ncbiRefSeqOtherAttrs \$db.other.bed \$asmId.attrs.txt > \$db.other.extras.bed
bedToBigBed -type=bed12+13 -as=ncbiRefSeqOther.as -tab \\
  -extraIndex=name \\
  \$db.other.extras.bed $HgAutomate::clusterData/\$db/chrom.sizes \$db.other.bb

# Make trix index for ncbiRefSeqOther
$ncbiRefSeqOtherIxIxx \\
  ncbiRefSeqOther.as \$db.other.extras.bed > ncbiRefSeqOther.ix.tab

ixIxx ncbiRefSeqOther.ix.tab ncbiRefSeqOther.ix{,x}

# PSL data will be loaded into a psl type track to show the alignments
(zgrep "^#" \$ncbiGffGz | head || true) > gffForPsl.gff
(zegrep -v "NG_" \$ncbiGffGz || true) \\
  | awk -F'\\t' '\$3 == "cDNA_match" || \$3 == "match"' >> gffForPsl.gff
gff3ToPsl -dropT \$downloadDir/\$asmId.chrom.sizes \$downloadDir/rna.sizes \\
  gffForPsl.gff stdout | pslPosTarget stdin \$asmId.psl
simpleChain -outPsl -maxGap=300000 \$asmId.psl stdout | pslSwap stdin stdout \\
  | liftUp -type=.psl stdout \$downloadDir/\${asmId}To\${db}.lift drop stdin \\
   | gzip -c > \$db.psl.gz
pslCheck -targetSizes=$HgAutomate::clusterData/\$db/chrom.sizes \\
   -querySizes=\$downloadDir/rna.sizes -db=\$db \$db.psl.gz

# extract RNA CDS information from genbank record
# Note: $asmId.raFile.txt could be used instead of _rna.gbff.gz
\$gbffToCds \$downloadDir/\${asmId}_rna.gbff.gz | sort > \$asmId.rna.cds

# the NCBI _genomic.gff.gz file only contains cDNA_match records for transcripts
# that do not *exactly* match the reference genome.  For all other transcripts
# construct 'fake' PSL records representing the alignments of all cDNAs
# that would be perfect matches to the reference genome.  The pslFixCdsJoinGap
# will fixup those records with unusual alignments due to frameshifts of
# various sorts as found in the rna.cds file:
genePredToFakePsl -qSizes=\$downloadDir/rna.sizes  \$db \$db.ncbiRefSeq.gp \\
  stdout \$db.fake.cds \\
     | pslFixCdsJoinGap stdin \$asmId.rna.cds \$db.fake.psl
pslCat -nohead \$db.psl.gz | cut -f10,14 > \$db.psl.names
if [ -s \$db.psl.names ]; then
  pslSomeRecords -tToo -not \$db.fake.psl \$db.psl.names \$db.someRecords.psl
else
  cp -p \$db.fake.psl \$db.someRecords.psl
fi
pslSort dirs stdout \\
 ./tmpdir \$db.psl.gz \$db.someRecords.psl \\
   | (pslCheck -quiet -db=\$db -pass=stdout -fail=\$asmId.\$db.fail.psl stdin || true) \\
    | pslPosTarget stdin stdout \\
    | pslRecalcMatch -ignoreQMissing stdin $dbTwoBit \$downloadDir/\$asmId.rna.fa.gz stdout \\
     | sort -k14,14 -k16,16n | gzip -c > \$asmId.\$db.psl.gz
rm -fr ./tmpdir
pslCheck -db=\$db \$asmId.\$db.psl.gz
_EOF_
  );
  $bossScript->execute();
} # doProcess

#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "load NCBI processed files into track files.";
  my $bossScript = newBash HgRemoteScript("$runDir/doLoad.bash", $dbHost,
				      $runDir, $whatItDoes);

  my $gbdbDir = "$HgAutomate::gbdb/\$db/ncbiRefSeq";
  my $dbTwoBit = "$HgAutomate::clusterData/$db/$db.2bit";

  $bossScript->add(<<_EOF_
# establish all variables to use here

export db=$db
export asmId=$asmId

_EOF_
  );

  $bossScript->add(<<_EOF_
# loading the genePred tracks, all genes in one, and subsets
hgLoadGenePred -genePredExt \$db ncbiRefSeq process/\$db.ncbiRefSeq.gp
genePredCheck -db=\$db ncbiRefSeq

if [ -s process/\$db.curated.gp ]; then
  hgLoadGenePred -genePredExt \$db ncbiRefSeqCurated process/\$db.curated.gp
  genePredCheck -db=\$db ncbiRefSeqCurated
fi

if [ -s process/\$db.predicted.gp ]; then
  hgLoadGenePred -genePredExt \$db ncbiRefSeqPredicted process/\$db.predicted.gp
  genePredCheck -db=\$db ncbiRefSeqPredicted
fi

mkdir -p $gbdbDir
ln -f -s `pwd`/process/\$db.other.bb $gbdbDir/ncbiRefSeqOther.bb
hgBbiDbLink \$db ncbiRefSeqOther $gbdbDir/ncbiRefSeqOther.bb
ln -f -s `pwd`/process/ncbiRefSeqOther.ix{,x} $gbdbDir/
ln -f -s `pwd`/process/ncbiRefSeqVersion.txt $gbdbDir/

# select only coding genes to have CDS records

awk -F"\t" '\$6 != \$7 {print \$1;}' process/\$db.ncbiRefSeq.gp \\
  | sort -u > coding.cds.name.list

join -t'\t' coding.cds.name.list process/\$asmId.rna.cds \\
  | hgLoadSqlTab \$db ncbiRefSeqCds ~/kent/src/hg/lib/cdsSpec.sql stdin

# loading the cross reference data
hgLoadSqlTab \$db ncbiRefSeqLink ~/kent/src/hg/lib/ncbiRefSeqLink.sql \\
  process/\$asmId.\$db.ncbiRefSeqLink.tab

# prepare Pep table, select only those peptides that match loaded items
hgsql -N -e 'select protAcc from ncbiRefSeqLink;' \$db | grep . \\
   | sort -u > \$db.ncbiRefSeqLink.protAcc.list

zcat download/\${asmId}_protein.faa.gz \\
   | sed -e 's/ .*//;' | faToTab -type=protein -keepAccSuffix stdin stdout \\
     | sort | join -t'\t' \$db.ncbiRefSeqLink.protAcc.list - \\
        > \$db.ncbiRefSeqPepTable.tab

hgLoadSqlTab \$db ncbiRefSeqPepTable ~/kent/src/hg/lib/pepPred.sql \\
   \$db.ncbiRefSeqPepTable.tab

# and load the fasta peptides, again, only those for items that exist
pslCat -nohead process/\$asmId.\$db.psl.gz | cut -f10 \\
   | sort -u > \$db.psl.used.rna.list
cut -f5 process/\$asmId.\$db.ncbiRefSeqLink.tab | grep . | sort -u > \$db.mrnaAcc.name.list
sort -u \$db.psl.used.rna.list \$db.mrnaAcc.name.list > \$db.rna.select.list
cut -f1 process/\$db.ncbiRefSeq.gp | sort -u > \$db.gp.name.list
comm -12 \$db.rna.select.list \$db.gp.name.list > \$db.toLoad.rna.list
comm -23 \$db.rna.select.list \$db.gp.name.list > \$db.not.used.rna.list
faSomeRecords download/\$asmId.rna.fa.gz \$db.toLoad.rna.list \$db.rna.fa
grep '^>' \$db.rna.fa | sed -e 's/^>//' | sort > \$db.rna.found.list
comm -13 \$db.rna.found.list \$db.gp.name.list > \$db.noRna.available.list

# If $db.noRna.available.list is not empty but items are on chrM, make fake cDNA sequence
# for them using chrM sequence since NCBI puts proteins, not coding RNAs, in the GFF.
if [ -s \$db.noRna.available.list ]; then
  pslCat -nohead process/\$asmId.\$db.psl.gz \\
    | grep -Fwf \$db.noRna.available.list \\
      | grep chrM > missingChrMFa.psl
  if [ -s missingChrMFa.psl ]; then
    pslToBed missingChrMFa.psl stdout \\
      | twoBitToFa -bed=stdin $dbTwoBit stdout >> \$db.rna.fa
  fi
fi

mkdir -p $gbdbDir
ln -f -s `pwd`/\$db.rna.fa $gbdbDir/seqNcbiRefSeq.rna.fa
hgLoadSeq -drop -seqTbl=seqNcbiRefSeq -extFileTbl=extNcbiRefSeq \$db $gbdbDir/seqNcbiRefSeq.rna.fa

hgLoadPsl \$db -table=ncbiRefSeqPsl process/\$asmId.\$db.psl.gz

featureBits \$db ncbiRefSeq > fb.ncbiRefSeq.\$db.txt 2>&1
cat fb.ncbiRefSeq.\$db.txt 2>&1
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
gzip -f download/{rna.sizes,*.raFile.txt}
gzip -f process/*.{tab,txt,gp,gff,psl,cds,bed}
# Leave this one uncompressed, gbdb links to it:
gunzip process/ncbiRefSeqVersion.txt.gz
_EOF_
  );
  $bossScript->execute();
} # doCleanup


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 5);

$toGpWarnOnly = 0;
$toGpWarnOnly = 1 if ($opt_toGpWarnOnly);

$secondsStart = `date "+%s"`;
chomp $secondsStart;

# expected command line arguments after options are processed
($genbankRefseq, $subGroup, $species, $asmId, $db) = @ARGV;
$ftpDir = "genomes/$genbankRefseq/$subGroup/$species/all_assembly_versions/$asmId";

if ( ! -s "/hive/data/genomes/$db/bed/idKeys/$db.idKeys.txt") {
  die "ERROR: can not find /hive/data/genomes/$db/bed/idKeys/$db.idKeys.txt\n\t  need to run doIdKeys.pl for $db before this procedure.";
}

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);

# Establish what directory we will work in.
my $date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/ncbiRefSeq.$date";

# may be working on a 2bit file that does not have a database browser
$dbExists = 0;
$dbExists = 1 if (&HgAutomate::databaseExists($dbHost, $db));

# Do everything.
$stepper->execute();

$secondsEnd = `date "+%s"`;
chomp $secondsEnd;
my $elapsedSeconds = $secondsEnd - $secondsStart;
my $elapsedMinutes = int($elapsedSeconds/60);
$elapsedSeconds -= $elapsedMinutes * 60;

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");

