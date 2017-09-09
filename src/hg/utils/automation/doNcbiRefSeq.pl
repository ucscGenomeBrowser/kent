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

my $gff3ToRefLink = "$Bin/gff3ToRefLink.pl";
my $gbffToCds = "$Bin/gbffToCds.pl";
my $ncbiRefSeqOtherAttrs = "$Bin/ncbiRefSeqOtherAttrs.pl";
my $ncbiRefSeqOtherAs = "$Bin/../../inc/ncbiRefSeqOther.as";

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_genbank
    $opt_subgroup
    $opt_species
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
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'fileServer' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
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
my ($buildDir);
my ($secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


#########################################################################
# * step: download [workhorse]
sub doDownload {
  my $runDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download required set of files from NCBI.";
  my $workhorse = &HgAutomate::chooseWorkhorse();
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

ln -s \$outsideCopy/\${asmId}_genomic.gff.gz .
ln -s \$outsideCopy/\${asmId}_rna.fna.gz .
ln -s \$outsideCopy/\${asmId}_rna.gbff.gz .
ln -s \$outsideCopy/\${asmId}_protein.faa.gz .
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
ln -s $local2Bit .
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

  $bossScript->add(<<_EOF_
# generate idKeys for the NCBI sequence to translate names to UCSC equivalents

mkdir -p \$runDir/idKeys
cd \$runDir/idKeys
time (doIdKeys.pl -buildDir=\$runDir/idKeys -twoBit=\$runDir/\$asmId.ncbi.2bit \$db) > idKeys.log 2>&1

cd \$runDir
ln -s idKeys/\$db.idKeys.txt ./ncbi.\$asmId.idKeys.txt
ln -s /hive/data/genomes/\$db/bed/idKeys/\$db.idKeys.txt ./ucsc.\$db.idKeys.txt
twoBitInfo \$asmId.ncbi.2bit stdout | sort -k2nr > \$asmId.chrom.sizes
zcat \${asmId}_rna.fna.gz | sed -e 's/ .*//;' | gzip -c > \$asmId.rna.fa.gz
faSize -detailed $asmId.rna.fa.gz | sort -k2nr > rna.sizes

# joining the idKeys establishes a lift file to translate chrom names
join -t'\t' ucsc.\$db.idKeys.txt ncbi.\$asmId.idKeys.txt \\
   | cut -f2-3 | sort \\
     | join -t'\t' - <(sort $HgAutomate::clusterData/\$db/chrom.sizes) \\
       | awk -F'\t' '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$2, \$3, \$1, \$3}' \\
          | sort -k3nr > \${asmId}To\${db}.lift

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

  my $dbTwoBit = "$HgAutomate::clusterData/$db/$db.2bit";
  $bossScript->add(<<_EOF_
# establish all variables to use here

export asmId=$asmId
export downloadDir=$downloadDir
export db=$db
export gff3ToRefLink=$gff3ToRefLink
export gbffToCds=$gbffToCds

# this produces the genePred in NCBI coordinates
# 8/23/17: gff3ToGenePred quits over illegal attribute SO_type... make it legal (so_type):
zcat \$downloadDir/\${asmId}_genomic.gff.gz \\
  | sed -re 's/([;\\t])SO_type=/\\1so_type=/;' \\
  | gff3ToGenePred -useName -attrsOut=\$asmId.attrs.txt -allowMinimalGenes \\
      -processAllGeneChildren -unprocessedRootsOut=\$asmId.unprocessedRoots.txt stdin \$asmId.gp
genePredCheck \$asmId.gp

# extract labels from semi-structured text in gbff COMMENT/description sections:
zcat \$downloadDir/\${asmId}_rna.gbff.gz \\
  | grep ' :: ' \\
    | perl -wpe 's/\\s+::.*//; s/^\\s+//;' \\
      | sort -u \\
        > pragmaLabels.txt

# extract cross reference text for refLink
\$gff3ToRefLink \$downloadDir/\$asmId.raFile.txt \$downloadDir/\${asmId}_genomic.gff.gz pragmaLabels.txt 2> \$db.refLink.stderr.txt \\
  | sort > \$asmId.refLink.tab

# converting the NCBI coordinates to UCSC coordinates
liftUp -extGenePred -type=.gp stdout \$downloadDir/\${asmId}To\${db}.lift drop \$asmId.gp \\
  | gzip -c > \$asmId.\$db.gp.gz
genePredCheck -db=\$db \$asmId.\$db.gp.gz

# join the refLink metadata with the gene names
zcat \$asmId.\$db.gp.gz | cut -f1 | sort -u > \$asmId.\$db.name.list
join -t'\t' \$asmId.\$db.name.list \$asmId.refLink.tab > \$asmId.\$db.ncbiRefSeqLink.tab

# curated subset of all genes
(zegrep "^N(M|R)_|^YP_" \$asmId.\$db.gp.gz || true) > \$db.curated.gp
# may not be any curated genes
if [ ! -s \$db.curated.gp ]; then
  rm -f \$db.curated.gp
fi

# predicted subset of all genes
zegrep "^X(M|R)_" \$asmId.\$db.gp.gz > \$db.predicted.gp

# not curated or predicted subset of all genes, the left overs
zegrep -v "^N(M|R)_|^YP_|^X(M|R)_" \$asmId.\$db.gp.gz > \$db.other.gp

# curated and predicted without leftovers:
zegrep "^N(M|R)_|^YP_|^X(M|R)_" \$asmId.\$db.gp.gz > \$db.ncbiRefSeq.gp

if [ -s \$db.curated.gp ]; then
  genePredCheck -db=\$db \$db.curated.gp
fi
genePredCheck -db=\$db \$db.predicted.gp
genePredCheck -db=\$db \$db.other.gp
genePredToBed \$db.other.gp stdout | sort -k1,1 -k2n,2n > \$db.other.bed

$ncbiRefSeqOtherAttrs \$db.other.bed \$asmId.attrs.txt > \$db.other.extras.bed
bedToBigBed -type=bed12+13 -as=$ncbiRefSeqOtherAs -tab \\
  -extraIndex=name \\
  \$db.other.extras.bed $HgAutomate::clusterData/\$db/chrom.sizes \$db.other.bb

# Make trix index for ncbiRefSeqOther
cut -f4,14,17,21,22,31 \$db.other.extras.bed \\
  | sed -e 's/GeneID://' \\
  | perl -we 'use List::MoreUtils qw(uniq); while (<>) { chomp; \@w = uniq split; print join(" ", \@w) . "\\n"; }' \\
  | sort -u \\
    > ncbiRefSeqOther.ix.tab
ixIxx ncbiRefSeqOther.ix.tab ncbiRefSeqOther.ix{,x}

# PSL data will be loaded into a psl type track to show the alignments
(zgrep "^#" \$downloadDir/\${asmId}_genomic.gff.gz | head || true) > gffForPsl.gff
zegrep -v "NG_" \$downloadDir/\${asmId}_genomic.gff.gz \\
  | awk -F'\\t' '\$3 == "cDNA_match" || \$3 == "match"' >> gffForPsl.gff
gff3ToPsl -dropT \$downloadDir/\$asmId.chrom.sizes \$downloadDir/rna.sizes \\
  gffForPsl.gff stdout | pslPosTarget stdin \$asmId.psl
simpleChain -outPsl -maxGap=300000 \$asmId.psl stdout | pslSwap stdin stdout \\
  | liftUp -type=.psl stdout \$downloadDir/\${asmId}To\${db}.lift drop stdin \\
   | gzip -c > \$db.psl.gz
pslCheck -targetSizes=$HgAutomate::clusterData/\$db/chrom.sizes \\
   -querySizes=\$downloadDir/rna.sizes -db=\$db \$db.psl.gz
genePredToFakePsl -qSizes=\$downloadDir/rna.sizes  \$db \$asmId.\$db.gp.gz \$db.fake.psl \$db.fake.cds
pslCat -nohead \$db.psl.gz | cut -f10,14 > \$db.psl.names
pslSomeRecords -tToo -not \$db.fake.psl \$db.psl.names \$db.someRecords.psl
pslSort dirs stdout \\
 ./tmpdir \$db.psl.gz \$db.someRecords.psl \\
   | (pslCheck -quiet -db=\$db -pass=stdout -fail=\$asmId.\$db.fail.psl stdin || true) \\
    | pslRecalcMatch -ignoreQMissing stdin $dbTwoBit \$downloadDir/\$asmId.rna.fa.gz stdout \\
     | sort -k14,14 -k16,16n | gzip -c > \$asmId.\$db.psl.gz
rm -fr ./tmpdir
pslCheck -db=\$db \$asmId.\$db.psl.gz

# extract RNA CDS information from genbank record
# Note: $asmId.raFile.txt could be used instead of _rna.gbff.gz
\$gbffToCds \$downloadDir/\${asmId}_rna.gbff.gz | sort > \$asmId.rna.cds
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
  my $gffFile = "${asmId}_genomic.gff.gz";
  my $releaseDate = `ls -L --full-time $buildDir/download/$gffFile | awk '{print \$6;}'`;
  chomp $releaseDate;

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

hgLoadGenePred -genePredExt \$db ncbiRefSeqPredicted process/\$db.predicted.gp
genePredCheck -db=\$db ncbiRefSeqPredicted

mkdir -p $gbdbDir
rm -f $gbdbDir/ncbiRefSeqOther.bb
ln -s `pwd`/process/\$db.other.bb $gbdbDir/ncbiRefSeqOther.bb
hgBbiDbLink \$db ncbiRefSeqOther $gbdbDir/ncbiRefSeqOther.bb
rm -f $gbdbDir/ncbiRefSeqOther.ix{,x}
ln -s `pwd`/process/ncbiRefSeqOther.ix{,x} $gbdbDir/

# select only coding genes to have CDS records

zcat process/\$asmId.\$db.gp.gz \\
  | awk -F"\t" '\$6 != \$7 {print \$1;}' \\
  | sort -u > coding.cds.name.list

join -t'\t' coding.cds.name.list process/\$asmId.rna.cds \\
  | hgLoadSqlTab \$db ncbiRefSeqCds ~/kent/src/hg/lib/cdsSpec.sql stdin

# loading the cross reference data -- substitute empty string for "n/a" (hgc expects "")
sed -e 's!n/a!!g;' process/\$asmId.\$db.ncbiRefSeqLink.tab \\
  | hgLoadSqlTab \$db ncbiRefSeqLink ~/kent/src/hg/lib/ncbiRefSeqLink.sql stdin

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
cut -f5 process/\$asmId.\$db.ncbiRefSeqLink.tab | grep -v "n/a" | sort -u > \$db.mrnaAcc.name.list
sort -u \$db.psl.used.rna.list \$db.mrnaAcc.name.list > \$db.rna.select.list
zcat process/\$asmId.\$db.gp.gz | cut -f1 | sort -u > \$db.gp.name.list
comm -12 \$db.rna.select.list \$db.gp.name.list > \$db.toLoad.rna.list
comm -23 \$db.rna.select.list \$db.gp.name.list > \$db.not.used.rna.list
comm -13 \$db.rna.select.list \$db.gp.name.list > \$db.noRna.available.list
faSomeRecords download/\$asmId.rna.fa.gz \$db.toLoad.rna.list \$db.rna.fa

mkdir -p $gbdbDir
rm -f $gbdbDir/seqNcbiRefSeq.rna.fa
ln -s `pwd`/\$db.rna.fa $gbdbDir/seqNcbiRefSeq.rna.fa
hgLoadSeq -drop -seqTbl=seqNcbiRefSeq -extFileTbl=extNcbiRefSeq \$db $gbdbDir/seqNcbiRefSeq.rna.fa

hgLoadPsl \$db -table=ncbiRefSeqPsl process/\$asmId.\$db.psl.gz

featureBits \$db ncbiRefSeq > fb.ncbiRefSeq.\$db.txt 2>&1
cat fb.ncbiRefSeq.\$db.txt 2>&1

hgsql -e 'INSERT INTO trackVersion \\
    (db, name, who, version, updateTime, comment, source, dateReference) \\
    VALUES("$db", "ncbiRefSeq", "$ENV{'USER'}", "$releaseDate", now(), \\
	"ftp://ftp.ncbi.nlm.nih.gov/$ftpDir/$gffFile", \\
	"ftp://ftp.ncbi.nlm.nih.gov/$ftpDir/$gffFile", \\
	"$releaseDate" );' hgFixed
_EOF_
  );
  $bossScript->execute();
} # doLoad

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
gzip download/{rna.sizes,*.raFile.txt}
gzip process/*.{tab,txt,gp,psl,cds}
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

