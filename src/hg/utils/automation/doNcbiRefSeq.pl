#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doNcbiRefSeq.pl instead.

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
    $opt_liftFile
    $opt_assemblyHub
    $opt_target2bit
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
usage: $base [options] asmId db
required arguments:
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
    -assemblyHub          the processing is taking place in an assembly hub
                          build
    -toGpWarnOnly         add -warnAndContinue to the gff3ToGenePred operation
                          to avoid gene definitions that will not convert
    -liftFile pathName    a lift file to translate NCBI names to local genome
                          names
    -target2bit pathName  when not a local UCSC genome, use this 2bit file for
                          the target genome sequence
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
NOTE: Override these assumptions with the -target2Bit option
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: asmId db
my ($asmId, $db);
# Other:
my ($ftpDir, $buildDir, $assemblyHub, $toGpWarnOnly, $dbExists, $liftFile, $target2bit);
my ($secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'liftFile=s',
		      'target2bit=s',
		      'assemblyHub',
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
  my $filesFound = 0;
 my @requiredFiles = qw( genomic.gff.gz rna.fna.gz rna.gbff.gz protein.faa.gz );
  my $filesExpected = scalar(@requiredFiles);
  foreach my $expectFile (@requiredFiles) {
    if ( -s "/hive/data/outside/ncbi/genomes/$ftpDir/${asmId}_${expectFile}" ) {
      ++$filesFound;
    } else {
      printf STDERR "# doNcbiRefSeq.pl: missing required file /hive/data/outside/ncbi/${asmId}_${expectFile}\n";
    }
  }

  if ($filesFound < $filesExpected) {
    printf STDERR "# doNcbiRefSeq.pl download: can not find all files required\n";
    exit 255;
  }
  my $runDir = "$buildDir/download";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download required set of files from NCBI.";
  my $bossScript = newBash HgRemoteScript("$runDir/doDownload.bash", $workhorse,
				      $runDir, $whatItDoes);
  my $outsideCopy = "/hive/data/outside/ncbi/genomes/$ftpDir";
  # might already have the NCBI 2bit file here:
  my $localData = $buildDir;
  $localData =~ s#trackData/ncbiRefSeq#download#;
  my $local2Bit = "$localData/$asmId.2bit";

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
printf STDERR "# checking $local2Bit\n";

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

  if ( -s $local2Bit ) {
    $bossScript->add(<<_EOF_
ln -f -s $local2Bit \${asmId}.ncbi.2bit
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
  if ( -s "$liftFile" ) {
       $haveLiftFile = 1;
  } elsif ($dbExists) {
     # check if db.ucscToRefSeq exists?
     my $sql = "show tables like 'ucscToRefSeq';";
     my $result = `hgsql $db -BN -e "$sql"`;
     chomp $result;
     if ( $result =~ m/ucscToRefSeq/ ) {
       $haveLiftFile = 1;
### the sort -k2,2 -u eliminates the problem of duplicate sequences existing
### in the target $db assembly
       $bossScript->add(<<_EOF_
cd \$runDir
hgsql -N -e 'select 0,name,chromEnd,chrom,chromEnd from ucscToRefSeq;' \${db} \\
     | sort -k2,2 -u > \${asmId}To\${db}.lift
_EOF_
       );
    }
  }

  my $dbTwoBit = "$HgAutomate::clusterData/$db/$db.2bit";
  $dbTwoBit = $target2bit if (-s "$target2bit");

  if (! $haveLiftFile ) {
     # if this is an assembly hub and it did NOT supply a lift file,
     # then we don't need one, do not attempt to build
     if (! $opt_assemblyHub) {
       $bossScript->add(<<_EOF_
# generate idKeys for the NCBI sequence to translate names to UCSC equivalents

rm -rf \$runDir/idKeys
mkdir -p \$runDir/idKeys
cd \$runDir/idKeys
time ($doIdKeys -buildDir=\$runDir/idKeys -twoBit=\$runDir/\$asmId.ncbi.2bit \$db) > idKeys.log 2>&1
cd \$runDir
twoBitInfo $dbTwoBit stdout | sort -k2,2nr > \$db.chrom.sizes
ln -f -s idKeys/\$db.idKeys.txt ./ncbi.\$asmId.idKeys.txt
ln -f -s /hive/data/genomes/\$db/bed/idKeys/\$db.idKeys.txt ./ucsc.\$db.idKeys.txt
# joining the idKeys establishes a lift file to translate chrom names
join -t\$'\\t' ucsc.\$db.idKeys.txt ncbi.\$asmId.idKeys.txt \\
   | cut -f2-3 | sort \\
     | join -t\$'\\t' - <(sort \$db.chrom.sizes) \\
       | awk -F\$'\\t' '{printf "0\\t%s\\t%d\\t%s\\t%d\\n", \$2, \$3, \$1, \$3}' \\
          | sort -k3nr > \${asmId}To\${db}.lift
_EOF_
       );
     }
  }

  $bossScript->add(<<_EOF_
cd \$runDir
twoBitInfo \$asmId.ncbi.2bit stdout | sort -k2nr > \$asmId.ncbi.chrom.sizes
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

  my $genePredCheckDb = "genePredCheck -db=\$db";
  if (! $dbExists) {
    $genePredCheckDb = "genePredCheck";
  }
  my $warnOnly = "";
  $warnOnly = "-warnAndContinue" if ($toGpWarnOnly);

  my $localLiftFile = "$downloadDir/${asmId}To${db}.lift";
  if (! -s "$localLiftFile") {
     $localLiftFile = "../download/${asmId}To${db}.lift" if (-s "../download/${asmId}To${db}.lift");
  }
  $localLiftFile = "" if (! -s "$localLiftFile");
  $localLiftFile = $liftFile if (-s $liftFile);
  my $pslTargetSizes = "-db=\$db";
  my $fakePslSizes = "";
  if (-s "$target2bit") {
    $pslTargetSizes = "-targetSizes=\$db.chrom.sizes";
    $fakePslSizes = "-chromSize=\$db.chrom.sizes";
  }

  my $dbTwoBit = "$HgAutomate::clusterData/$db/$db.2bit";
  $dbTwoBit = $target2bit if (-s "$target2bit");
  my $localSizes = "$HgAutomate::clusterData/$db/chrom.sizes";

  $bossScript->add(<<_EOF_
# establish all variables to use here

export asmId=$asmId
export downloadDir=$downloadDir
export ncbiGffGz=\$downloadDir/\${asmId}_genomic.gff.gz
export db=$db
export chromSizes="$localSizes"
export gff3ToRefLink=$gff3ToRefLink
export gbffToCds=$gbffToCds
export dateStamp=`date "+%F"`
if [ -s "../../../\$asmId.chrom.sizes" ]; then
  chromSizes="../../../\$asmId.chrom.sizes"
fi
if [ -s "../download/\$asmId.ncbi.chrom.sizes" ]; then
  chromSizes="../download/\$asmId.ncbi.chrom.sizes"
fi

export annotationRelease=`zcat \$ncbiGffGz | head -100 | grep ^#.annotation-source | sed -e 's/.*annotation-source //; s/ Updated Annotation Release//;'`
if [ "\$annotationRelease" == "" ]; then
  export annotationRelease=\$asmId
fi
export versionDate=`ls -L --full-time \$ncbiGffGz | awk '{print \$6;}'`
echo "\$annotationRelease (\$versionDate)" > ncbiRefSeqVersion.txt

# this produces the genePred in NCBI coordinates
# 8/23/17: gff3ToGenePred quits over illegal attribute SO_type... make it legal (so_type):
if [ -s ../../../download/\${asmId}.remove.dups.list ]; then
  zcat \$ncbiGffGz | grep -v -f ../../../download/\${asmId}.remove.dups.list \\
    | sed -re 's/([;\\t])SO_type=/\\1so_type=/;' \\
      | gff3ToGenePred $warnOnly -refseqHacks -attrsOut=\$asmId.attrs.txt \\
        -unprocessedRootsOut=\$asmId.unprocessedRoots.txt stdin stdout \\
      | genePredFilter -chromSizes=\$chromSizes stdin \$asmId.gp
else
  zcat \$ncbiGffGz \\
    | sed -re 's/([;\\t])SO_type=/\\1so_type=/;' \\
      | gff3ToGenePred $warnOnly -refseqHacks -attrsOut=\$asmId.attrs.txt \\
        -unprocessedRootsOut=\$asmId.unprocessedRoots.txt stdin stdout \\
      | genePredFilter -chromSizes=\$chromSizes stdin \$asmId.gp
fi
genePredCheck \$asmId.gp

rm -f \$asmId.refseqSelectTranscripts.txt
zegrep 'tag=(RefSeq|MANE) Select' \$ncbiGffGz > before.cut9.txt || true

if [ -s before.cut9.txt ]; then
  cut -f9- before.cut9.txt | tr ';' '\\n' \\
    | grep 'Name=' | grep -v NP_ | cut -d= -f2 | sort -u \\
       > \$asmId.refseqSelectTranscripts.txt
fi
rm -f before.cut9.txt

# extract labels from semi-structured text in gbff COMMENT/description sections:
zcat \$downloadDir/\${asmId}_rna.gbff.gz \\
  | (grep ' :: ' || true) \\
    | perl -wpe 's/\\s+::.*//; s/^\\s+//;' \\
      | sort -u \\
        > pragmaLabels.txt

# extract cross reference text for refLink
\$gff3ToRefLink \$downloadDir/\$asmId.raFile.txt \$ncbiGffGz pragmaLabels.txt 2> \$db.refLink.stderr.txt \\
  | sort > \$asmId.refLink.tab

_EOF_
  );
  if ( -s "$localLiftFile") {
  $bossScript->add(<<_EOF_
# converting the NCBI coordinates to UCSC coordinates
liftUp -extGenePred -type=.gp stdout $localLiftFile warn \$asmId.gp \\
  | gzip -c > \$asmId.\$db.gp.gz
_EOF_
  );
  } else {
  $bossScript->add(<<_EOF_
# no lifting necessary
cat \$asmId.gp | gzip -c > \$asmId.\$db.gp.gz
_EOF_
  );
  }
  $bossScript->add(<<_EOF_
$genePredCheckDb \$asmId.\$db.gp.gz
zcat \$asmId.\$db.gp.gz > ncbiRefSeq.\$dateStamp
genePredToGtf -utr file ncbiRefSeq.\$dateStamp stdout | gzip -c \\
  > ../\$db.\$dateStamp.ncbiRefSeq.gtf.gz
rm -f ncbiRefSeq.\$dateStamp

# curated subset of all genes
(zegrep "^[NY][MRP]_" \$asmId.\$db.gp.gz || true) > \$db.curated.gp
# may not be any curated genes
if [ ! -s \$db.curated.gp ]; then
  rm -f \$db.curated.gp
  rm -f \$db.refseqSelect.curated.gp
  rm -f hgmd.curated.gp
else
  if [ -s \$asmId.refseqSelectTranscripts.txt ]; then
    cat \$db.curated.gp | fgrep -f \$asmId.refseqSelectTranscripts.txt - \\
      > \$db.refseqSelect.curated.gp
    # may not be any refseqSelect.curated genes
    if [ ! -s \$db.refseqSelect.curated.gp ]; then
      rm -f \$db.refseqSelect.curated.gp
    fi
  fi
  if [ \$db = "hg19" -o \$db = "hg38" ]; then
     hgmdFile=`ls /hive/data/outside/hgmd/20*.4-hgmd-public_hg38.tsv | tail -1`
     if [ -s "\$hgmdFile" ]; then
       cut -f7 "\$hgmdFile" | cut -d. -f1 | sort -u | awk '{printf "%s.\\n", \$1}' > hgmdTranscripts.txt
       fgrep -f hgmdTranscripts.txt \$db.curated.gp > hgmd.curated.gp
     fi
  fi
fi

# predicted subset of all genes
(zegrep "^X[MR]_" \$asmId.\$db.gp.gz || true) > \$db.predicted.gp

# not curated or predicted subset of all genes, the left overs
(zegrep -v "^[NXY][MRP]_" \$asmId.\$db.gp.gz || true) > \$db.other.gp

# curated and predicted without leftovers:
(zegrep "^[NXY][MRP]_" \$asmId.\$db.gp.gz || true) > \$db.ncbiRefSeq.gp

if [ -s \$db.curated.gp ]; then
  $genePredCheckDb \$db.curated.gp
  if [ -s \$db.refseqSelect.curated.gp ]; then
     $genePredCheckDb \$db.refseqSelect.curated.gp
  fi
  if [ -s hgmd.curated.gp ]; then
     $genePredCheckDb hgmd.curated.gp
  fi
fi
if [ -s \$db.predicted.gp ]; then
  $genePredCheckDb \$db.predicted.gp
fi
if [ -s \$db.other.gp ]; then
  $genePredCheckDb \$db.other.gp
fi

# join the refLink metadata with curated+predicted names
cut -f1 \$db.ncbiRefSeq.gp | sort -u > \$asmId.\$db.name.list
join -t\$'\\t' \$asmId.\$db.name.list \$asmId.refLink.tab > \$asmId.\$db.ncbiRefSeqLink.tab

# Make bigBed with attributes in extra columns for ncbiRefSeqOther:
twoBitInfo $dbTwoBit stdout | sort -k2,2n > \$db.chrom.sizes

if [ -s \$db.other.gp ]; then
  if [ -s ../../../download/\${asmId}.remove.dups.list ]; then
    genePredToBed -tab -fillSpace \$db.other.gp stdout | sort -k1,1 -k2n,2n \\
      | grep -v -f ../../../download/\${asmId}.remove.dups.list > \$db.other.bed
  else
    genePredToBed -tab -fillSpace \$db.other.gp stdout | sort -k1,1 -k2n,2n > \$db.other.bed
  fi
  $ncbiRefSeqOtherAttrs \$db.other.bed \$asmId.attrs.txt > \$db.other.extras.bed
  bedToBigBed -type=bed12+13 -as=ncbiRefSeqOther.as -tab \\
    -extraIndex=name \\
    \$db.other.extras.bed \$db.chrom.sizes \$db.other.bb

  # Make trix index for ncbiRefSeqOther
  $ncbiRefSeqOtherIxIxx \\
    ncbiRefSeqOther.as \$db.other.extras.bed > ncbiRefSeqOther.ix.tab

  ixIxx ncbiRefSeqOther.ix.tab ncbiRefSeqOther.ix{,x}
fi

# PSL data will be loaded into a psl type track to show the alignments
(zgrep "^#" \$ncbiGffGz | head || true) > gffForPsl.gff
if [ -s ../../../download/\${asmId}.remove.dups.list ]; then
  (zegrep -v "NG_" \$ncbiGffGz || true) \\
    | grep -v -f ../../../download/\${asmId}.remove.dups.list \\
    | awk -F\$'\\t' '\$3 == "cDNA_match" || \$3 == "match"' >> gffForPsl.gff
  gff3ToPsl -dropT \$downloadDir/\$asmId.ncbi.chrom.sizes \$downloadDir/rna.sizes \\
    gffForPsl.gff stdout | pslPosTarget stdin \$asmId.psl
else
  (zegrep -v "NG_" \$ncbiGffGz || true) \\
    | awk -F\$'\\t' '\$3 == "cDNA_match" || \$3 == "match"' >> gffForPsl.gff
  gff3ToPsl -dropT \$downloadDir/\$asmId.ncbi.chrom.sizes \$downloadDir/rna.sizes \\
    gffForPsl.gff stdout | pslPosTarget stdin \$asmId.psl
fi
# might be empty result
if [ ! -s \$asmId.psl ]; then
  rm -f \$asmId.psl
else
_EOF_
  );
  # note else clause of above if statement is concluded below in these two
  # cases
  if ( -s "$localLiftFile") {
  $bossScript->add(<<_EOF_
 simpleChain -outPsl -maxGap=300000 \$asmId.psl stdout | pslSwap stdin stdout \\
   | liftUp -type=.psl stdout $localLiftFile warn stdin \\
     | gzip -c > \$db.psl.gz
fi
_EOF_
  );
  } else {
  $bossScript->add(<<_EOF_
 simpleChain -outPsl -maxGap=300000 \$asmId.psl stdout | pslSwap stdin stdout \\
    | gzip -c > \$db.psl.gz
fi
_EOF_
  );
  }

  $bossScript->add(<<_EOF_
if [ -s \$db.psl.gz ]; then
  pslCheck $pslTargetSizes \\
    -querySizes=\$downloadDir/rna.sizes \$db.psl.gz
fi

# extract RNA CDS information from genbank record
# Note: $asmId.raFile.txt could be used instead of _rna.gbff.gz
\$gbffToCds \$downloadDir/\${asmId}_rna.gbff.gz | sort > \$asmId.rna.cds

# the NCBI _genomic.gff.gz file only contains cDNA_match records for transcripts
# that do not *exactly* match the reference genome.  For all other transcripts
# construct 'fake' PSL records representing the alignments of all cDNAs
# that would be perfect matches to the reference genome.  The pslFixCdsJoinGap
# will fixup those records with unusual alignments due to frameshifts of
# various sorts as found in the rna.cds file:
genePredToFakePsl -qSizes=\$downloadDir/rna.sizes $fakePslSizes \$db \$db.ncbiRefSeq.gp \\
  stdout \$db.fake.cds \\
     | pslFixCdsJoinGap stdin \$asmId.rna.cds \$db.fake.psl
if [ -s \$db.psl.gz ]; then
  pslCat -nohead \$db.psl.gz | cut -f10,14 > \$db.psl.names
fi
if [ -s \$db.psl.names ]; then
  pslSomeRecords -tToo -not \$db.fake.psl \$db.psl.names \$db.someRecords.psl
else
  cp -p \$db.fake.psl \$db.someRecords.psl
fi
if [ -s \$db.psl.gz ]; then
  pslSort dirs stdout \\
   ./tmpdir \$db.psl.gz \$db.someRecords.psl | gzip -c > \$db.sorted.psl.gz
else
  pslSort dirs stdout \\
   ./tmpdir \$db.someRecords.psl | gzip -c > \$db.sorted.psl.gz
fi
(pslCheck -quiet $pslTargetSizes -pass=stdout -fail=\$asmId.\$db.fail.psl \$db.sorted.psl.gz || true) \\
    | pslPosTarget stdin stdout \\
    | pslRecalcMatch -ignoreQMissing stdin $dbTwoBit \$downloadDir/\$asmId.rna.fa.gz stdout \\
     | sort -k14,14 -k16,16n | gzip -c > \$asmId.\$db.psl.gz
rm -fr ./tmpdir
pslCheck $pslTargetSizes \$asmId.\$db.psl.gz
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
  $dbTwoBit = $target2bit if (-s "$target2bit");

  my $genePredCheckDb = "genePredCheck -db=\$db";
  if (! $dbExists) {
    $genePredCheckDb = "genePredCheck";
  }

  my $verString = `cat $buildDir/process/ncbiRefSeqVersion.txt`;
  chomp $verString;
  $verString =~ s/.*elease //;
  $verString =~ s/^[^0-9]*//;
  $verString =~ s/ .*//;

  $bossScript->add(<<_EOF_
# establish all variables to use here

export db="$db"
export asmId="$asmId"
export verString="$verString"

_EOF_
  );
  if (! $dbExists) {
    $bossScript->add(<<_EOF_
export target2bit=$dbTwoBit

twoBitInfo \$target2bit stdout | sort -k2,2nr > \$db.chrom.sizes
wget -O bigGenePred.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/bigGenePred.as'
wget -O bigPsl.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/bigPsl.as'

### overall gene track with both predicted and curated
genePredToBigGenePred process/\$db.ncbiRefSeq.gp stdout | sort -k1,1 -k2,2n > \$db.ncbiRefSeq.bigGp
genePredToBed -tab -fillSpace process/\$db.ncbiRefSeq.gp stdout \\
    | bedToExons stdin stdout | bedSingleCover.pl stdin > \$asmId.exons.bed
export baseCount=`awk '{sum+=\$3-\$2}END{printf "%d", sum}' \$asmId.exons.bed`
export asmSizeNoGaps=`grep sequences ../../\$asmId.faSize.txt | awk '{print \$5}'`
export perCent=`echo \$baseCount \$asmSizeNoGaps | awk '{printf "%.3f", 100.0*\$1/\$2}'`
bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as -extraIndex=name \\
  \$db.ncbiRefSeq.bigGp \$db.chrom.sizes \\
    \$db.ncbiRefSeq.bb
bigBedInfo \$db.ncbiRefSeq.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$db.ncbiRefSeq.stats.txt
LC_NUMERIC=en_US /usr/bin/printf "# ncbiRefSeq %s %'d %s %'d\\n" `cat \$db.ncbiRefSeq.stats.txt` | xargs echo
~/kent/src/hg/utils/automation/gpToIx.pl process/\$db.ncbiRefSeq.gp \\
    | sort -u > \$asmId.ncbiRefSeq.ix.txt
ixIxx \$asmId.ncbiRefSeq.ix.txt \$asmId.ncbiRefSeq.ix{,x}
rm -f \$asmId.ncbiRefSeq.ix.txt

### curated only if present
if [ -s process/\$db.curated.gp ]; then
  genePredToBigGenePred process/\$db.curated.gp stdout | sort -k1,1 -k2,2n > \$db.ncbiRefSeqCurated.bigGp
  bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as -extraIndex=name \\
  \$db.ncbiRefSeqCurated.bigGp \$db.chrom.sizes \\
    \$db.ncbiRefSeqCurated.bb
  rm -f \$db.ncbiRefSeqCurated.bigGp
  bigBedInfo \$db.ncbiRefSeqCurated.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$db.ncbiRefSeqCurated.stats.txt
  LC_NUMERIC=en_US /usr/bin/printf "# ncbiRefSeqCurated %s %'d %s %'d\\n" `cat \$db.ncbiRefSeqCurated.stats.txt` | xargs echo
  ~/kent/src/hg/utils/automation/gpToIx.pl process/\$db.curated.gp \\
    | sort -u > \$asmId.ncbiRefSeqCurated.ix.txt
  ixIxx \$asmId.ncbiRefSeqCurated.ix.txt \$asmId.ncbiRefSeqCurated.ix{,x}
  rm -f \$asmId.ncbiRefSeqCurated.ix.txt
### and refseqSelect if exists (a subset of curated)
  if [ -s process/\$db.refseqSelect.curated.gp ]; then
    genePredToBigGenePred process/\$db.refseqSelect.curated.gp stdout | sort -k1,1 -k2,2n > \$db.ncbiRefSeqSelectCurated.bigGp
    bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as -extraIndex=name \\
    \$db.ncbiRefSeqSelectCurated.bigGp \$db.chrom.sizes \\
      \$db.ncbiRefSeqSelectCurated.bb
    rm -f \$db.ncbiRefSeqSelectCurated.bigGp
    bigBedInfo \$db.ncbiRefSeqSelectCurated.bb | egrep "^itemCount:|^basesCovered:" \\
      | sed -e 's/,//g' > \$db.ncbiRefSeqSelectCurated.stats.txt
    LC_NUMERIC=en_US /usr/bin/printf "# ncbiRefSeqSelectCurated %s %'d %s %'d\\n" `cat \$db.ncbiRefSeqSelectCurated.stats.txt` | xargs echo
    ~/kent/src/hg/utils/automation/gpToIx.pl process/\$db.refseqSelect.curated.gp \\
      | sort -u > \$asmId.ncbiRefSeqSelectCurated.ix.txt
    ixIxx \$asmId.ncbiRefSeqSelectCurated.ix.txt \$asmId.ncbiRefSeqSelectCurated.ix{,x}
    rm -f \$asmId.ncbiRefSeqSelectCurated.ix.txt
  fi
### and hgmd if exists (a subset of curated)
  if [ -s process/hgmd.curated.gp ]; then
    genePredToBigGenePred process/hgmd.curated.gp stdout | sort -k1,1 -k2,2n > \$db.ncbiRefSeqHgmd.bigGp
    bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as -extraIndex=name \\
    \$db.ncbiRefSeqHgmd.bigGp \$db.chrom.sizes \\
      \$db.ncbiRefSeqHgmd.bb
    rm -f \$db.ncbiRefSeqHgmd.bigGp
    bigBedInfo \$db.ncbiRefSeqHgmd.bb | egrep "^itemCount:|^basesCovered:" \\
      | sed -e 's/,//g' > \$db.ncbiRefSeqHgmd.stats.txt
    LC_NUMERIC=en_US /usr/bin/printf "# ncbiRefSeqHgmd %s %'d %s %'d\\n" `cat \$db.ncbiRefSeqHgmd.stats.txt` | xargs echo
    ~/kent/src/hg/utils/automation/gpToIx.pl process/hgmd.curated.gp \\
      | sort -u > \$asmId.ncbiRefSeqHgmd.ix.txt
    ixIxx \$asmId.ncbiRefSeqHgmd.ix.txt \$asmId.ncbiRefSeqHgmd.ix{,x}
    rm -f \$asmId.ncbiRefSeqHgmd.ix.txt
  fi
fi

### predicted only if present
if [ -s process/\$db.predicted.gp ]; then
  genePredToBigGenePred process/\$db.predicted.gp stdout | sort -k1,1 -k2,2n > \$db.ncbiRefSeqPredicted.bigGp
  bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as -extraIndex=name \\
  \$db.ncbiRefSeqPredicted.bigGp \$db.chrom.sizes \\
    \$db.ncbiRefSeqPredicted.bb
  rm -f \$db.ncbiRefSeqPredicted.bigGp
  bigBedInfo \$db.ncbiRefSeqPredicted.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$db.ncbiRefSeqPredicted.stats.txt
  LC_NUMERIC=en_US /usr/bin/printf "# ncbiRefSeqPredicted %s %'d %s %'d\\n" `cat \$db.ncbiRefSeqPredicted.stats.txt` | xargs echo
  ~/kent/src/hg/utils/automation/gpToIx.pl process/\$db.predicted.gp \\
    | sort -u > \$asmId.ncbiRefSeqPredicted.ix.txt
  ixIxx \$asmId.ncbiRefSeqPredicted.ix.txt \$asmId.ncbiRefSeqPredicted.ix{,x}
  rm -f \$asmId.ncbiRefSeqPredicted.ix.txt
fi

### all other annotations, not necessarily genes
if [ -s "process/\$db.other.bb" ]; then
  ln -f -s process/\$db.other.bb \$db.ncbiRefSeqOther.bb
  bigBedInfo \$db.ncbiRefSeqOther.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$db.ncbiRefSeqOther.stats.txt
  LC_NUMERIC=en_US /usr/bin/printf "# ncbiRefSeqOther %s %'d %s %'d\\n" `cat \$asmId.ncbiRefSeqOther.stats.txt` | xargs echo
fi
if [ -s "process/ncbiRefSeqOther.ix" ]; then
  ln -f -s process/ncbiRefSeqOther.ix ./\$db.ncbiRefSeqOther.ix
  ln -f -s process/ncbiRefSeqOther.ixx ./\$db.ncbiRefSeqOther.ixx
fi
ln -f -s process/ncbiRefSeqVersion.txt ./\$db.ncbiRefSeqVersion.txt
# select only coding genes to have CDS records

awk -F" " '\$6 != \$7 {print \$1;}' process/\$db.ncbiRefSeq.gp \\
  | sort -u > coding.cds.name.list

join -t\$'\t' coding.cds.name.list process/\$asmId.rna.cds \\
  > \$db.ncbiRefSeqCds.tab

rm -f coding.cds.name.list

ln -f -s process/\$asmId.\$db.ncbiRefSeqLink.tab .
cut -f6 \$asmId.\$db.ncbiRefSeqLink.tab | grep . \\
  | sort -u > \$db.ncbiRefSeqLink.protAcc.list

zcat download/\${asmId}_protein.faa.gz \\
   | sed -e 's/ .*//;' | faToTab -type=protein -keepAccSuffix stdin stdout \\
     | sort | join -t\$'\\t' \$db.ncbiRefSeqLink.protAcc.list - \\
        > \$db.ncbiRefSeqPepTable.tab

# and load the fasta peptides, again, only those for items that exist
pslCat -nohead process/\$asmId.\$db.psl.gz | cut -f10 \\
   | sort -u > \$db.psl.used.rna.list
cut -f5 process/\$asmId.\$db.ncbiRefSeqLink.tab \\
   | grep . | sort -u > \$db.mrnaAcc.name.list
sort -u \$db.psl.used.rna.list \$db.mrnaAcc.name.list > \$db.rna.select.list
cut -f1 process/\$db.ncbiRefSeq.gp | sort -u > \$db.gp.name.list
comm -12 \$db.rna.select.list \$db.gp.name.list > \$db.toLoad.rna.list
comm -23 \$db.rna.select.list \$db.gp.name.list > \$db.not.used.rna.list
faSomeRecords download/\$asmId.rna.fa.gz \$db.toLoad.rna.list \$db.rna.fa
grep '^>' \$db.rna.fa | sed -e 's/^>//' | sort > \$db.rna.found.list
comm -13 \$db.rna.found.list \$db.gp.name.list > \$db.noRna.available.list

_EOF_
    );

  my $nonNucNames="chrM";

  my $haveLiftFile = 0;
  if ( -s "$liftFile" ) {
       $haveLiftFile = 1;
  }
  # if this is an assembly hub, find out what the non-nuclear names are
  if ( $opt_assemblyHub) {
     if ( -s "../../sequence/$asmId.nonNucChr.names") {
     # with lift file means to use UCSC name in the nonNucChr.names
     if ($haveLiftFile ) {
        $nonNucNames=`cut -f1 ../../sequence/$asmId.nonNucChr.names | xargs echo | tr ' ' '|'`;
     } else {
        $nonNucNames=`cut -f2 ../../sequence/$asmId.nonNucChr.names | xargs echo | tr ' ' '|'`;
     }
     chomp $nonNucNames;
     }
  }
     # if this is an assembly hub and it did NOT supply a lift file,
     # then we don't need one, do not attempt to build
    $bossScript->add(<<_EOF_

# If \$db.noRna.available.list is not empty but items are on chrM,
# make fake cDNA sequence for them using chrM sequence
# since NCBI puts proteins, not coding RNAs, in the GFF.
if [ -s \$db.noRna.available.list ]; then
  pslCat -nohead process/\$asmId.\$db.psl.gz \\
    | grep -Fwf \$db.noRna.available.list \\
      | egrep "$nonNucNames" > missingChrMFa.psl
  if [ -s missingChrMFa.psl ]; then
    pslToBed missingChrMFa.psl stdout \\
      | twoBitToFa -bed=stdin \$target2bit stdout >> \$db.rna.fa
  fi
fi

if [ -s process/\$asmId.rna.cds ]; then
  cat process/\$asmId.rna.cds | grep '[0-9]\\+\\.\\.[0-9]\\+' \\
    | pslMismatchGapToBed -cdsFile=stdin -db=\$db -ignoreQNamePrefix=X \\
      process/\$asmId.\$db.psl.gz \$target2bit \\
        \$db.rna.fa ncbiRefSeqGenomicDiff || true

  if [ -s ncbiRefSeqGenomicDiff.bed ]; then
    wget -O txAliDiff.as 'http://genome-source.soe.ucsc.edu/gitlist/kent.git/raw/master/src/hg/lib/txAliDiff.as'
    bedToBigBed -type=bed9+ -tab -as=txAliDiff.as \\
      ncbiRefSeqGenomicDiff.bed \$db.chrom.sizes ncbiRefSeqGenomicDiff.bb
  else
    rm -f ncbiRefSeqGenomicDiff.bed
  fi
fi

export totalBases=`ave -col=2 \$db.chrom.sizes | grep "^total" | awk '{printf "%d", \$2}'`
export basesCovered=`bedSingleCover.pl \$db.ncbiRefSeq.bigGp | ave -col=4 stdin | grep "^total" | awk '{printf "%d", \$2}'`
export percentCovered=`echo \$basesCovered \$totalBases | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$basesCovered" \\
   "\$totalBases" "\$percentCovered" > fb.ncbiRefSeq.\$db.txt
printf "%d bases of %d (%s%%) in intersection\\n" "\$baseCount" "\$asmSizeNoGaps" "\$perCent" > fb.\$asmId.ncbiRefSeq.txt

rm -f \$db.ncbiRefSeq.bigGp \$asmId.exons.bed

pslToBigPsl -fa=download/\$asmId.rna.fa.gz -cds=process/\$asmId.rna.cds \\
  process/\$asmId.\$db.psl.gz stdout | sort -k1,1 -k2,2n > \$asmId.bigPsl
bedToBigBed -type=bed12+13 -tab -as=bigPsl.as -extraIndex=name \\
  \$asmId.bigPsl \$db.chrom.sizes \$asmId.bigPsl.bb
rm -f \$asmId.bigPsl
_EOF_
    );
  } else {	# processing for a database genome

    $bossScript->add(<<_EOF_
# loading the genePred tracks, all genes in one, and subsets
hgLoadGenePred -genePredExt \$db ncbiRefSeq process/\$db.ncbiRefSeq.gp
$genePredCheckDb ncbiRefSeq

if [ -s process/\$db.curated.gp ]; then
  hgLoadGenePred -genePredExt \$db ncbiRefSeqCurated process/\$db.curated.gp
  $genePredCheckDb ncbiRefSeqCurated
  if [ -s process/\$db.refseqSelect.curated.gp ]; then
    hgLoadGenePred -genePredExt \$db ncbiRefSeqSelect process/\$db.refseqSelect.curated.gp
    $genePredCheckDb ncbiRefSeqSelect
  fi
  if [ -s process/hgmd.curated.gp ]; then
    hgLoadGenePred -genePredExt \$db ncbiRefSeqHgmd process/hgmd.curated.gp
    $genePredCheckDb ncbiRefSeqHgmd
  fi
fi

if [ -s process/\$db.predicted.gp ]; then
  hgLoadGenePred -genePredExt \$db ncbiRefSeqPredicted process/\$db.predicted.gp
  $genePredCheckDb ncbiRefSeqPredicted
fi

mkdir -p $gbdbDir
ln -f -s `pwd`/process/\$db.other.bb $gbdbDir/ncbiRefSeqOther.bb
hgBbiDbLink \$db ncbiRefSeqOther $gbdbDir/ncbiRefSeqOther.bb
ln -f -s `pwd`/process/ncbiRefSeqOther.ix{,x} $gbdbDir/
ln -f -s `pwd`/process/ncbiRefSeqVersion.txt $gbdbDir/

# select only coding genes to have CDS records

awk -F\$'\\t' '\$6 != \$7 {print \$1;}' process/\$db.ncbiRefSeq.gp \\
  | sort -u > coding.cds.name.list

join -t\$'\\t' coding.cds.name.list process/\$asmId.rna.cds \\
  | hgLoadSqlTab \$db ncbiRefSeqCds ~/kent/src/hg/lib/cdsSpec.sql stdin

# loading the cross reference data
hgLoadSqlTab \$db ncbiRefSeqLink ~/kent/src/hg/lib/ncbiRefSeqLink.sql \\
  process/\$asmId.\$db.ncbiRefSeqLink.tab

# prepare Pep table, select only those peptides that match loaded items
hgsql -N -e 'select protAcc from ncbiRefSeqLink;' \$db | grep . \\
   | sort -u > \$db.ncbiRefSeqLink.protAcc.list

zcat download/\${asmId}_protein.faa.gz \\
   | sed -e 's/ .*//;' | faToTab -type=protein -keepAccSuffix stdin stdout \\
     | sort | join -t\$'\\t' \$db.ncbiRefSeqLink.protAcc.list - \\
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

if [ -s process/\$asmId.rna.cds ]; then
  cat process/\$asmId.rna.cds | grep '[0-9]\\+\\.\\.[0-9]\\+' \\
    | pslMismatchGapToBed -cdsFile=stdin -db=\$db -ignoreQNamePrefix=X \\
      process/\$asmId.\$db.psl.gz $dbTwoBit \\
        \$db.rna.fa ncbiRefSeqGenomicDiff || true

  rm -f $gbdbDir/ncbiRefSeqGenomicDiff.bb
  if [ -s ncbiRefSeqGenomicDiff.bed ]; then
    bedToBigBed -type=bed9+ -tab -as=\${HOME}/kent/src/hg/lib/txAliDiff.as \\
      ncbiRefSeqGenomicDiff.bed process/\$db.chrom.sizes ncbiRefSeqGenomicDiff.bb
    ln -s `pwd`/ncbiRefSeqGenomicDiff.bb $gbdbDir/ncbiRefSeqGenomicDiff.bb
  else
    rm -f ncbiRefSeqGenomicDiff.bed
  fi
fi

if [ -d "/usr/local/apache/htdocs-hgdownload/goldenPath/archive" ]; then
 mkdir -p /usr/local/apache/htdocs-hgdownload/goldenPath/archive/\$db/ncbiRefSeq/\$verString
 rm -f /usr/local/apache/htdocs-hgdownload/goldenPath/archive/\$db/ncbiRefSeq/\$verString/\$db.\$verString.ncbiRefSeq.gtf.gz
 ln -s `pwd`/\$db.*.ncbiRefSeq.gtf.gz \\
   /usr/local/apache/htdocs-hgdownload/goldenPath/archive/\$db/ncbiRefSeq/\$verString/\$db.\$verString.ncbiRefSeq.gtf.gz
fi

rm -f /usr/local/apache/htdocs-hgdownload/goldenPath/\$db/bigZips/genes/\$db.ncbiRefSeq.gtf.gz
mkdir -p /usr/local/apache/htdocs-hgdownload/goldenPath/\$db/bigZips/genes
ln -s `pwd`/\$db.*.ncbiRefSeq.gtf.gz \\
    /usr/local/apache/htdocs-hgdownload/goldenPath/\$db/bigZips/genes/\$db.ncbiRefSeq.gtf.gz

featureBits \$db ncbiRefSeq > fb.ncbiRefSeq.\$db.txt 2>&1
cat fb.ncbiRefSeq.\$db.txt 2>&1
_EOF_
    );
  }	#	if ($dbExists)
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
&usage(1) if (scalar(@ARGV) != 2);

$toGpWarnOnly = 0;
$toGpWarnOnly = 1 if ($opt_toGpWarnOnly);
$liftFile = $opt_liftFile ? $opt_liftFile : "";
$target2bit = $opt_target2bit ? $opt_target2bit : "";

$secondsStart = `date "+%s"`;
chomp $secondsStart;

# expected command line arguments after options are processed
($asmId, $db) = @ARGV;
# yes, there can be more than two fields separated by _
# but in this case, we only care about the first two:
# GC[AF]_123456789.3_assembly_Name
#   0         1         2      3 ....
my @partNames = split('_', $asmId);
$ftpDir = sprintf("%s/%s/%s/%s/%s", $partNames[0],
   substr($partNames[1],0,3), substr($partNames[1],3,3),
   substr($partNames[1],6,3), $asmId);

if ( -z "$liftFile" && ! -s "/hive/data/genomes/$db/bed/idKeys/$db.idKeys.txt") {
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

