#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doEnsGeneUpdate.pl instead.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use EnsGeneAutomate;
use HgAutomate;
use HgRemoteScript;
use HgStepManager;
use File::Basename;


# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_ensVersion
    $opt_vegaGene
    $opt_buildDir
    $opt_chromSizes
    $opt_species
    /;


# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&doDownload },
      { name => 'process',   func => \&doProcess },
      { name => 'load',   func => \&doLoad },
      { name => 'cleanup', func => \&doCleanup },
      { name => 'makeDoc', func => \&doMakeDoc },
    ]
);

# Option defaults:
my $dbHost = 'hgwdev';
my $vegaSpecies = "human";
my $vegaPep = "Homo_sapiens.VEGA";

my $base = $0;
$base =~ s/^(.*\/)?//;
my (@versionList) = &EnsGeneAutomate::ensVersionList();
my $versionListString = join(", ", @versionList);
my $versionString = "";

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base -ensVersion=NN <db>.ensGene.ra
required options:
  -ensVersion=NN - must specify desired Ensembl version
                 - possible values: $versionListString
  <db>.ensGene.ra - configuration file with database and other options

other options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -vegaGene             Special processing for vegaGene instead of Ensembl
                          works only on Human, Mouse, Zebrafish
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ensGene.<ensVersion #>
                          (necessary if experimenting with builds).
    -chromSizes filePath  Use filePath for chrom.size file instead of database
                          chromInfo request
    -species "Species name"  supply a species name when working with an
                          assembly hub
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
  print STDERR "
Automates UCSC's Ensembl gene updates.  Steps:
    download: fetch GFF and protein data files from Ensembl FTP site
    process: parse the GFF file, create coding and non-coding gff files
		lift random contigs to UCSC random coordinates
    load: load the coding and non-coding tracks, and the proteins
    cleanup: Removes or compresses intermediate files.
    makeDoc: Displays the make doc text entry to stdout for the procedure
	which would be done for this build.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/ensGene.<ensVersion #> unless -buildDir is given.
<db>.ensGene.ra describes the species, assembly and downloaded files.
To see detailed information about what should appear in config.ra,
run \"$base -help\".
";
  # Detailed help (-help):
  if ($detailed) {
  print STDERR <<_EOF_
-----------------------------------------------------------------------------
Required <db>.ensGene.ra settings:

db xxxYyyN
  - The UCSC database name and assembly identifier.  xxx is the first three
    letters of the genus and Yyy is the first three letters of the species.
    N is the sequence number of this species' build at UCSC.  For some
    species that we have been processing since before this convention, the
    pattern is xyN.

Optional <db>.ensGene.ra settings:

liftRandoms yes
  - Need to lift Ensembl contig coordinates to UCSC _random coordinates.
  - Will use UCSC ctgPos information about contigs to perform the lift.
nameTranslation "<sed translation pattern>"
  - sed translation statement to change Ensembl chrom numbers and MT to
    UCSC chrN and chrM chrom names.  Example:
    nameTranslation "s/^\([0-9XY][0-9]*\)/chr\1/; s/^MT/chrM/"
geneScaffolds yes
  - need to translate Ensembl GeneScaffold coordinates to UCSC scaffolds
    Will fetch and use the Ensembl MySQL tables seq_region and assembly
    to perform the translation
skipInvalid yes
  - during the loading of the gene pred, skip all invalid genes
haplotypeLift <path/to/file.lift>
  - a lift-down file for Ensembl full chromosome haplotype coordinates
  - to UCSC simple haplotype coordinates
liftUp <path/to/ensGene.lft>
  - after everything else is done, lift the final gene pred via this
  - lift file.  Comes in handy if Ensembl chrom names are simply
  - different than UCSC chrom names.  For example in bushbaby, otoGar1
liftMtOver <path/to/ensGene.Mt.overChain>
  - before exit process step, lift the gene pred in chrM  via this lift over 
  - file if Ensembl is using chrM sequence different from the sequence 
  - used by UCSC. For example in turkey, melGal1.

Assumptions:
1. $HgAutomate::clusterData/\$db/\$db.2bit contains RepeatMasked sequence for
   database/assembly \$db.
2. $HgAutomate::clusterData/\$db/chrom.sizes contains all sequence names and sizes from
   \$db.2bit.
   (can be changed with the -chromSizes option)
3. The \$db.2bit files have already been distributed to cluster-scratch
   (/scratch/hg or /iscratch, /san etc).
4. anything else here ?
_EOF_
  }
  print "\n";
  exit $status;
}



# Globals:
my ($species, $ensGtfUrl, $ensGtfFile, $ensPepUrl, $ensPepFile,
    $ensMySqlUrl, $ensVersionDateReference, $previousEnsVersion, $buildDir,
    $dbExists, $chromSizes, $previousBuildDir, $vegaGene, $hubSpecies);
# Command line argument:
my $CONFIG;
# Required command line arguments:
my ($ensVersion);
# Required config parameters:
my ($db);
# Conditionally required config parameters:
my ($liftRandoms, $nameTranslation, $geneScaffolds, $knownToEnsembl,
    $skipInvalid, $haplotypeLift, $liftUp, $liftMtOver);
# Other globals:
my ($topDir, $chromBased);
my ($bedDir, $scriptDir, $endNotes);
my ($secondsStart, $secondsEnd);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'ensVersion=s',
		      'vegaGene',
		      'buildDir=s',
		      'chromSizes=s',
		      'species=s',
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
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";
  if (! -d "$buildDir/process" && ! $opt_debug) {
    die "ERROR: load: directory: '$buildDir/process' does not exist.\n" .
      "The process step appears to have not been done.\n" .
	"Run with -continue=process before this step.\n";
  }
  if (! -s "$buildDir/process/$db.allGenes.gp.gz" ) {
   die "ERROR: load: process result: '$buildDir/process/$db.allGenes.gp.gz'\n" .
      "does not exist.  The process step appears to have not been done.\n" .
	"Run with -continue=process before this step.\n";
  }

  if (-s "$db.ensGene.stats.txt" || -s "fb.$db.ensGene.txt" ) {
     &HgAutomate::verbose(1,
         "# step load is already completed, continuing...\n");
     return;
  }


  my $whatItDoes = "load processed ensGene data into local database.";
  my $bossScript = newBash HgRemoteScript("$runDir/doLoad.bash", $dbHost,
				      $runDir, $whatItDoes);

  my $thisGenePred = "$buildDir" . "/process/$db.allGenes.gp.gz";
  my $prevGenePred = "$previousBuildDir" . "/process/$db.allGenes.gp.gz";
  my $thisGtp = "$buildDir" . "/process/ensGtp.tab";
  my $prevGtp = "$previousBuildDir" . "/process/ensGtp.tab";
  my $thisGeneName = "$buildDir" . "/process/ensemblToGeneName.tab";
  my $prevGeneName = "$previousBuildDir" . "/process/ensemblToGeneName.tab";
  my $thisSource = "$buildDir" . "/process/ensemblSource.tab";
  my $prevSource = "$previousBuildDir" . "/process/ensemblSource.tab";
  my $identicalToPrevious = 1;
  if ( -f $prevGenePred && -f $prevGtp && -f $prevGeneName && -f $prevSource ) {
      my $thisSum = `zcat $thisGenePred | sort | sum`;
      chomp $thisSum;
      my $prevSum = `zcat $prevGenePred | sort | sum`;
      chomp $prevSum;
      printf STDERR "# genePred prev: $prevSum %s this: $thisSum\n",
         $prevSum eq $thisSum ? "==" : "!=";
      $identicalToPrevious = 0 if ($prevSum ne $thisSum);

      $thisSum = `sort $thisGtp | sum`;
      chomp $thisSum;
      $prevSum = `sort $prevGtp | sum`;
      chomp $prevSum;
      $identicalToPrevious = 0 if ($prevSum ne $thisSum);
      printf STDERR "# ensGtp prev: $prevSum %s this: $thisSum\n",
         $prevSum eq $thisSum ? "==" : "!=";

      $thisSum = `sort $thisGeneName | sum`;
      chomp $thisSum;
      $prevSum = `sort $prevGeneName | sum`;
      chomp $prevSum;
      $identicalToPrevious = 0 if ($prevSum ne $thisSum);
      printf STDERR "# ensemblToGeneName prev: $prevSum %s this: $thisSum\n",
         $prevSum eq $thisSum ? "==" : "!=";

      $thisSum = `sort $thisSource | sum`;
      chomp $thisSum;
      $prevSum = `sort $prevSource | sum`;
      chomp $prevSum;
      $identicalToPrevious = 0 if ($prevSum ne $thisSum);
      printf STDERR "# ensemblSource prev: $prevSum %s this: $thisSum\n",
         $prevSum eq $thisSum ? "==" : "!=";

      if (1 == $identicalToPrevious) {
	print STDERR "previous genes same as new genes";
      }
  } else {
    $identicalToPrevious = 0;
  }

# there are too many things to check to verify identical to previous
$identicalToPrevious = 0;

  $bossScript->add(<<_EOF_
export db="$db"

_EOF_
	  );

  if ($dbExists && $identicalToPrevious ) {
      $bossScript->add(<<_EOF_
hgsql -e 'INSERT INTO trackVersion \\
    (db, name, who, version, updateTime, comment, source, dateReference) \\
    VALUES("\$db", "ensGene", "$ENV{'USER'}", "$ensVersion", now(), \\
	"identical to previous version $previousEnsVersion", \\
	"identical to previous version $previousEnsVersion", \\
	"$ensVersionDateReference" );' hgFixed
featureBits \$db ensGene > fb.\$db.ensGene.txt 2>&1
_EOF_
	  );
  } else {
      my $skipInv = "";
      $skipInv = "-skipInvalid" if (defined $skipInvalid);

      if ($opt_vegaGene) {
      $bossScript->add(<<_EOF_
hgLoadGenePred $skipInv -genePredExt \$db \\
    vegaGene process/not.vegaPseudo.gp.gz >& load.not.pseudo.errors.txt
hgLoadGenePred $skipInv -genePredExt \$db \\
    vegaPseudoGene process/vegaPseudo.gp.gz >& load.pseudo.errors.txt
hgsql -N -e "select name from vegaPseudoGene;" \$db > pseudo.name
hgsql -N -e "select name from vegaGene;" \$db > not.pseudo.name
sort -u pseudo.name not.pseudo.name > vegaGene.name
sed -e "s/20/40/; s/19/39/" $ENV{'HOME'}/kent/src/hg/lib/ensGtp.sql \\
    > vegaGtp.sql
hgLoadSqlTab \$db vegaGtp vegaGtp.sql process/ensGtp.tab
zcat download/$ensPepFile \\
	| sed -e 's/^>.* Transcript:/>/;' | gzip > vegaPep.txt.gz
zcat vegaPep.txt.gz \\
    | ~/kent/src/utils/faToTab/faToTab.pl /dev/null /dev/stdin \\
	 | sed -e '/^\$/d; s/\*\$//' | sort > vegaPepAll.\$db.fa.tab
join vegaPepAll.\$db.fa.tab vegaGene.name | sed -e "s/ /\\t/" \\
    > vegaPep.\$db.fa.tab
hgPepPred \$db tab vegaPep vegaPep.\$db.fa.tab
# verify names in vegaGene is a superset of names in vegaPep
hgsql -N -e "select name from vegaPep;" \$db | sort > vegaPep.name
export geneCount=`cat vegaGene.name | wc -l`
export pepCount=`cat vegaPep.name | wc -l`
export commonCount=`comm -12 vegaPep.name vegaGene.name | wc -l`
export percentId=`echo \$commonCount \$pepCount | awk '{printf "%d", 100.0*\$1/\$2}'`
echo "gene count: \$geneCount, peptide count: \$pepCount, common name count: \$commonCount"
echo "percentId: \$percentId"
if [ \$percentId -lt 96 ]; then
    echo "ERROR: percent coverage of peptides to genes: \$percentId"
    echo "ERROR: should be greater than 95"
    exit 255
fi
_EOF_
	  );
      } elsif (defined $geneScaffolds) {
	  $bossScript->add(<<_EOF_
hgLoadGenePred $skipInv -genePredExt \$db ensGene process/\$db.allGenes.gp.gz \\
	>& loadGenePred.errors.txt
checkTableCoords \$db -table=ensGene
zcat process/ensemblGeneScaffolds.\$db.bed.gz | sort > to.clean.GeneScaffolds.bed
cut -f1 /hive/data/genomes/\$db/chrom.sizes | sort > legitimate.names
join -t'\t' legitimate.names to.clean.GeneScaffolds.bed \\
    | sed -e "s/GeneScaffold/GS/" | hgLoadBed \$db ensemblGeneScaffold stdin
checkTableCoords \$db -table=ensemblGeneScaffold
_EOF_
	  );
      } else {
        if ($dbExists) {
      $bossScript->add(<<_EOF_
hgLoadGenePred $skipInv -genePredExt \$db \\
    ensGene process/\$db.allGenes.gp.gz > loadGenePred.errors.txt 2>&1
_EOF_
	  );
        } else {
          if (! -s "$chromSizes") {
            die "ERROR: load: assembly hub load needs a chromSizes\n";
          }
          $bossScript->add(<<_EOF_
mkdir -p bbi
genePredFilter -verbose=2 -chromSizes=$chromSizes \\
  process/\$db.allGenes.gp.gz stdout | gzip -c > \$db.ensGene.gp.gz
genePredToBed \$db.ensGene.gp.gz stdout | sort -k1,1 -k2,2n > \$db.ensGene.gp.bed
bedToExons \$db.ensGene.gp.bed stdout | bedSingleCover.pl stdin > \$db.exons.bed
export baseCount=`awk '{sum+=\$3-\$2}END{printf "%d", sum}' \$db.exons.bed`
export asmSizeNoGaps=`grep sequences ../../\$db.faSize.txt | awk '{print \$5}'`
export perCent=`echo \$baseCount \$asmSizeNoGaps | awk '{printf "%.3f", 100.0*\$1/\$2}'`
printf "%d bases of %d (%s%%) in intersection\\n" "\$baseCount" "\$asmSizeNoGaps" "\$perCent" > fb.\$db.ensGene.txt
printf "%s\n" "${versionString}" > version.txt
bedToBigBed -extraIndex=name \$db.ensGene.gp.bed $chromSizes bbi/\$db.ensGene.bb
bigBedInfo bbi/\$db.ensGene.bb | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$db.ensGene.stats.txt
LC_NUMERIC=en_US /usr/bin/printf "# ensGene %s %'d %s %'d\\n" `cat \$db.ensGene.stats.txt` | xargs echo
_EOF_
	  );
        }
      }

      if ($dbExists && ! $opt_vegaGene) {
      $bossScript->add(<<_EOF_
zcat download/$ensPepFile \\
	| sed -e 's/^>.* transcript:/>/; s/ CCDS.*\$//; s/ .*\$//' | gzip > ensPep.txt.gz
zcat ensPep.txt.gz \\
    | ~/kent/src/utils/faToTab/faToTab.pl /dev/null /dev/stdin \\
	 | sed -e '/^\$/d; s/\*\$//' | sort > ensPep.\$db.fa.tab
hgPepPred \$db tab ensPep ensPep.\$db.fa.tab
hgLoadSqlTab \$db ensGtp ~/kent/src/hg/lib/ensGtp.sql process/ensGtp.tab
hgLoadSqlTab \$db ensemblToGeneName process/ensemblToGeneName.sql process/ensemblToGeneName.tab
hgLoadSqlTab \$db ensemblSource process/ensemblSource.sql process/ensemblSource.tab
# verify names in ensGene is a superset of names in ensPep
hgsql -N -e "select name from ensPep;" \$db | sort > ensPep.name
hgsql -N -e "select name from ensGene;" \$db | sort > ensGene.name
export geneCount=`cat ensGene.name | wc -l`
export pepCount=`cat ensPep.name | wc -l`
export commonCount=`comm -12 ensPep.name ensGene.name | wc -l`
export percentId=`echo \$commonCount \$pepCount | awk '{printf "%d", 100.0*\$1/\$2}'`
echo "gene count: \$geneCount, peptide count: \$pepCount, common name count: \$commonCount"
echo "percentId: \$percentId"
if [ \$percentId -lt 96 ]; then
    echo "ERROR: percent coverage of peptides to genes: \$percentId"
    echo "ERROR: should be greater than 95"
    exit 255
fi
_EOF_
      );
      }
      if ($dbExists && ! $opt_vegaGene && defined $knownToEnsembl) {
	  $bossScript->add(<<_EOF_
hgMapToGene \$db ensGene knownGene knownToEnsembl
_EOF_
	  );
      }
      if ($opt_vegaGene) {
      $bossScript->add(<<_EOF_
hgsql -e 'INSERT INTO trackVersion \\
    (db, name, who, version, updateTime, comment, source, dateReference) \\
    VALUES("\$db", "vegaGene", "$ENV{'USER'}", "$ensVersion", now(), \\
	"with peptides $ensPepFile", \\
	"$ensGtfUrl", \\
	"$ensVersionDateReference" );' hgFixed
_EOF_
      );
      } elsif ($dbExists) {
      $bossScript->add(<<_EOF_
hgsql -e 'INSERT INTO trackVersion \\
    (db, name, who, version, updateTime, comment, source, dateReference) \\
    VALUES("\$db", "ensGene", "$ENV{'USER'}", "$ensVersion", now(), \\
	"with peptides $ensPepFile", \\
	"$ensGtfUrl", \\
	"$ensVersionDateReference" );' hgFixed
featureBits \$db ensGene > fb.\$db.ensGene.txt 2>&1
_EOF_
      );
      }
  }
  $bossScript->execute() if (! $opt_debug);
} # doLoad

#########################################################################
# * step: process [dbHost]
sub doProcess {
  my $runDir = "$buildDir/process";
  my $geneCount = 0;
  if (-s "$runDir/$db.allGenes.gp.gz" ) {
    $geneCount = `zcat "$runDir/$db.allGenes.gp.gz" | wc -l`;
    chomp $geneCount;
  }
  if ($geneCount > 0) {
     &HgAutomate::verbose(1,
         "# step process is already completed, continuing...\n");
     return;
  }
  # First, make sure we're starting clean.
  if (-d "$runDir" && ! $opt_debug) {
    die "ERROR: process: looks like this was run successfully already\n" .
      "($runDir exists)\nEither run with -continue=load or some later\n" .
	"stage, or move aside/remove\n$runDir\nand run again.\n";
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "process downloaded data from Ensembl FTP site into locally usable data.";
  my $lifting = 0;
  my $bossScript;
  if (defined $liftRandoms) {
      $lifting = 1;
  }
  $bossScript = new HgRemoteScript("$runDir/doProcess.csh", $dbHost,
				  $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
set db = "$db"

_EOF_
      );
  #  if lifting, create the lift file
  if ($lifting) {
      $bossScript->add(<<_EOF_
rm -f randoms.\$db.lift
foreach C (`cut -f1 /hive/data/genomes/\$db/chrom.sizes | grep random`)
   set size = `grep \$C /hive/data/genomes/\$db/chrom.sizes | cut -f2`
   hgsql -N -e \\
"select chromStart,contig,size,chrom,\$size from ctgPos where chrom='\$C';" \\
	\$db  | awk '{gsub("\\\\.[0-9]+", "", \$2); print }' >> randoms.\$db.lift
end
_EOF_
      );
  }
  #  somg
  $bossScript->add(<<_EOF_
zcat ../download/$ensGtfFile \\
_EOF_
  );
  #  translate Ensemble chrom names to UCSC chrom name
  if (defined $nameTranslation) {
  $bossScript->add(<<_EOF_
	| sed -e $nameTranslation \\
_EOF_
  );
  }
  # lift down haplotypes if necessary
  if (defined $haplotypeLift) {
      $bossScript->add(<<_EOF_
	| liftUp -type=.gtf stdout $haplotypeLift carry stdin \\
_EOF_
      );
  }
  #  lift randoms if necessary
  if ($lifting) {
      $bossScript->add(<<_EOF_
	| liftUp -type=.gtf stdout randoms.\$db.lift carry stdin \\
_EOF_
      );
  }
  #  result is protein coding set
  $bossScript->add(<<_EOF_
	| gzip > allGenes.gtf.gz
_EOF_
  );
  my $name2 = "";
  $name2 = "-geneNameAsName2" if (! $dbExists);	# assembly hub use name2
  $bossScript->add(<<_EOF_
gtfToGenePred ${name2} -includeVersion -infoOut=infoOut.txt -genePredExt allGenes.gtf.gz stdout \\
    | gzip > \$db.allGenes.gp.gz
$Bin/extractGtf.pl infoOut.txt > ensGtp.tab
$Bin/ensemblInfo.pl infoOut.txt > ensemblToGeneName.tab
$Bin/extractSource.pl allGenes.gtf.gz | sort -u > ensemblSource.tab
set NL = `awk 'BEGIN{max=0}{if (length(\$1) > max) max=length(\$1)}END{print max}' ensemblToGeneName.tab`
set VL = `awk 'BEGIN{max=0}{if (length(\$2) > max) max=length(\$2)}END{print max}' ensemblToGeneName.tab`
sed -e "s/ knownTo / ensemblToGeneName /; s/known gene/ensGen/; s/INDEX(name(12)/PRIMARY KEY(name(\$NL)/; s/value(12)/value(\$VL)/" \\
            $ENV{'HOME'}/kent/src/hg/lib/knownTo.sql > ensemblToGeneName.sql
sed -e "s/name(15)/name(\$NL)/" \\
	$ENV{'HOME'}/kent/src/hg/lib/ensemblSource.sql > ensemblSource.sql
_EOF_
  );
  if ($opt_vegaGene) {
  $bossScript->add(<<_EOF_
zcat allGenes.gtf.gz | grep -i pseudo \\
  | sed -e 's/gene_id/other_gene_id/; s/gene_name/gene_id/' > vegaPseudo.gtf
zcat allGenes.gtf.gz | grep -v -i pseudo \\
  | sed -e 's/gene_id/other_gene_id/; s/gene_name/gene_id/' > not.vegaPseudo.gtf
gtfToGenePred -includeVersion -genePredExt vegaPseudo.gtf vegaPseudo.gp
gtfToGenePred -includeVersion -genePredExt not.vegaPseudo.gtf not.vegaPseudo.gp
gzip vegaPseudo.gp not.vegaPseudo.gp
_EOF_
  );
  }
  if (defined $geneScaffolds) {
      $bossScript->add(<<_EOF_
mv \$db.allGenes.gp.gz \$db.allGenes.beforeLift.gp.gz
$Bin/ensGeneScaffolds.pl ../download/seq_region.txt.gz \\
	../download/assembly.txt.gz | gzip > \$db.ensGene.lft.gz
liftAcross -warn -bedOut=ensemblGeneScaffolds.\$db.bed \$db.ensGene.lft.gz \\
	\$db.allGenes.beforeLift.gp.gz \$db.allGenes.gp >& liftAcross.err.out
gzip ensemblGeneScaffolds.\$db.bed \$db.allGenes.gp liftAcross.err.out
_EOF_
      );
  }
  if (defined $liftMtOver) {
      $bossScript->add(<<_EOF_
cp \$db.allGenes.gp.gz \$db.allGenes.beforeLiftMtOver.gp.gz
zcat \$db.allGenes.gp.gz > all.gp
grep chrM all.gp | liftOver -genePred stdin $liftMtOver chrMLifted.gp noMap.chrM
grep -v chrM all.gp | cat - chrMLifted.gp > allLifted.gp
gzip -c allLifted.gp > \$db.allGenes.gp.gz
rm *.gp
_EOF_
      );
    }
  if (defined $liftUp) {
      $bossScript->add(<<_EOF_
mv \$db.allGenes.gp.gz \$db.allGenes.beforeLiftUp.gp.gz
liftUp -extGenePred -type=.gp \$db.allGenes.gp \\
    $liftUp carry \\
    \$db.allGenes.beforeLiftUp.gp.gz
gzip \$db.allGenes.gp
_EOF_
      );
      if (defined $geneScaffolds) {
      $bossScript->add(<<_EOF_
mv ensemblGeneScaffolds.\$db.bed.gz ensemblGeneScaffolds.\$db.beforeLiftUp.bed.gz
liftUp -type=.bed ensemblGeneScaffolds.\$db.bed \\
    $liftUp carry \\
    ensemblGeneScaffolds.\$db.beforeLiftUp.bed.gz
gzip ensemblGeneScaffolds.\$db.bed
_EOF_
	  );
      }
  }
  $bossScript->add(<<_EOF_
grep -v "^#" infoOut.txt \\
  | awk -F'\\t' '{printf "%s\\t%s,%s,%s,%s\\n", \$1,\$2,\$8,\$9,\$10}' \\
    | sed -e 's/,,/,/g; s/,\\+\$//;' > \$db.ensGene.nameIndex.txt
ixIxx \$db.ensGene.nameIndex.txt \$db.ensGene.ix \$db.ensGene.ixx
_EOF_
                  );
  # if all of these are supposed to be valid, they should be able to
  #	pass genePredCheck right now
  if (! defined $skipInvalid) {
      if ($opt_vegaGene) {
      $bossScript->add(<<_EOF_
genePredCheck -db=\$db vegaPseudo.gp.gz
genePredCheck -db=\$db not.vegaPseudo.gp.gz
_EOF_
	  );
      }
      if (-s "$chromSizes") {
      $bossScript->add(<<_EOF_
genePredCheck -chromSizes=$chromSizes \$db.allGenes.gp.gz
_EOF_
         );
      } else {
      $bossScript->add(<<_EOF_
genePredCheck -db=\$db \$db.allGenes.gp.gz
_EOF_
         );
      }
  }
  $bossScript->execute() if (! $opt_debug);
} # doProcess

#########################################################################
# * step: download [dbHost]
sub doDownload {
  my $runDir = "$buildDir/download";
  # check if been already done
  if (-s "$runDir/$ensGtfFile" && -s "$runDir/$ensPepFile" ) {
     &HgAutomate::verbose(1,
         "# step download is already completed, continuing...\n");
     return;
  }
  # If not already done, then it should be clean.
  if (-d "$runDir" && ! $opt_debug) {
    die "ERROR: download: looks like this was attempted unsuccessfully" .
      " before.\n($runDir exists, download files do not)\n";
  }
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "download data from Ensembl FTP site.";
  my $bossScript = new HgRemoteScript("$runDir/doDownload.csh", $dbHost,
				      $runDir, $whatItDoes);

  $bossScript->add(<<_EOF_
wget --tries=2 --user=anonymous --password=ucscGenomeBrowser\@ucsc.edu \\
$ensGtfUrl \\
-O $ensGtfFile
wget --tries=2 --user=anonymous --password=ucscGenomeBrowser\@ucsc.edu \\
$ensPepUrl \\
-O $ensPepFile
_EOF_
  );
  if (defined $geneScaffolds) {
      $bossScript->add(<<_EOF_
wget --tries=2 --user=anonymous --password=ucscGenomeBrowser\@ucsc.edu \\
$ensMySqlUrl/seq_region.txt.gz \\
-O seq_region.txt.gz
wget --tries=2 --user=anonymous --password=ucscGenomeBrowser\@ucsc.edu \\
$ensMySqlUrl/assembly.txt.gz \\
-O assembly.txt.gz
_EOF_
      );
  }
  $bossScript->execute() if (! $opt_debug);
} # doDownload

#########################################################################
# * step: cleanup [dbHost]
sub doCleanup {
  my $runDir = "$buildDir";
  my $whatItDoes = "It cleans up or compresses intermediate files.";
  my $bossScript = new HgRemoteScript("$runDir/doCleanup.csh", $dbHost,
				      $runDir, $whatItDoes);
  if ($opt_vegaGene) {
    $bossScript->add(<<_EOF_
rm -f pseudo.name not.pseudo.name vegaGene.name vegaPepAll.$db.fa.tab vegaPep.name
gzip vegaPep.$db.fa.tab
_EOF_
    );
  } else {
    $bossScript->add(<<_EOF_
rm -f bed.tab ensPep.txt.gz ensPep.$db.fa.tab ensPep.name ensGene.name
rm -f $db.ensGene.gp.bed
_EOF_
    );
  }
  $bossScript->execute() if (! $opt_debug);
} # doCleanup

#########################################################################
# * step: makeDoc [dbHost]
sub doMakeDoc {
  my $runDir = "$buildDir";
  my $whatItDoes = "Display the make doc text to stdout.";

  if (! $dbExists) {
    &HgAutomate::verbose(1,
         "# step makeDoc is not run when not a database build\n");
    return;
  }
  my $updateTime = `hgsql -N -e 'select updateTime from trackVersion where db = "$db" order by updateTime DESC limit 1;' hgFixed`;
  chomp $updateTime;
  $updateTime =~ s/ .*//;	#	removes time
  my $organism = `hgsql -N -e 'select organism from dbDb where name = "$db";' hgcentraltest`;
  chomp $organism;
  if (length($organism) < 1) {
     if ( -s "$HgAutomate::clusterData/$db/species.name.txt" ) {
        $organism = `cat $HgAutomate::clusterData/$db/species.name.txt`;
        chomp $organism;
     } else {
        $organism = "species name not found";
     }
  }

  my $vegaOpt = "";
  my $trackName = "Ensembl";
  my $tableName = "ensGene";
  my $workDir = "ensGene";
  $tableName = "vegaGene" if ($opt_vegaGene);
  $trackName = "Vega" if ($opt_vegaGene);
  $vegaOpt = "-vegaGene" if ($opt_vegaGene);
  $workDir = "vega" if ($opt_vegaGene);
  if ($opt_vegaGene) {
    print <<_EOF_
############################################################################
#  $db - $organism - $trackName Genes version $ensVersion  (DONE - $updateTime - $ENV{'USER'})
    ssh $dbHost
    cd /hive/data/genomes/$db
_EOF_
  ;
  } else {
    print <<_EOF_
############################################################################
#  $db - $organism - $trackName Genes version $ensVersion  (DONE - $updateTime - $ENV{'USER'})
    ssh $dbHost
    cd /hive/data/genomes/$db
    cat << '_EOF_' > $db.$tableName.ra
_EOF_
  ;
    print `cat $db.ensGene.ra`;
    print "'_EOF_'\n";
    print "#  << happy emacs\n\n";
  }
  print "    doEnsGeneUpdate.pl ${vegaOpt} -ensVersion=$ensVersion $db.ensGene.ra\n";
  print "    ssh hgwdev\n";
  print "    cd /hive/data/genomes/$db/bed/$workDir.$ensVersion\n";
  print "    featureBits $db $tableName\n";
  print "    # ";
  print `featureBits $db $tableName`;
  if ($opt_vegaGene) {
    print "    featureBits $db vegaPseudoGene\n";
    print "    # ";
    print `featureBits $db vegaPseudoGene`;
  }
  print "############################################################################\n";

} # doMakeDoc
#############################################################################

sub requireVar {
  # Ensure that var is in %config and return its value.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var}
    || die "ERROR: requireVar: $CONFIG is missing required variable \"$var\".\n" .
      "For a detailed list of required variables, run \"$base -help\".\n";
  delete $config->{$var};
  return $val;
} # requireVar

sub optionalVar {
  # If var has a value in %config, return it.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var};
  delete $config->{$var} if ($val);
  return $val;
} # optionalVar

sub parseConfig {
  # Parse config.ra file, make sure it contains the required variables.
  my ($configFile) = @_;
  my %config = ();
  my $fh = &HgAutomate::mustOpen($configFile);
  while (<$fh>) {
    next if (/^\s*#/ || /^\s*$/);
    if (/^\s*(\w+)\s*(.*)$/) {
      my ($var, $val) = ($1, $2);
      if (! exists $config{$var}) {
	$config{$var} = $val;
      } else {
	die "ERROR: parseConfig: Duplicate definition for $var line $. of config file $configFile.\n";
      }
    } else {
      die "ERROR: parseConfig: Can't parse line $. of config file $configFile:\n$_\n";
    }
  }
  close($fh);

  # Required variables.
  $db = &requireVar('db', \%config);
  # may be working on an assembly hub that does not have a database browser
  $dbExists = 0;
  $dbExists = 1 if (&HgAutomate::databaseExists($dbHost, $db));

  printf STDERR "# dbExists: %d for db: %s\n", $dbExists, $db;

  if ($dbExists) {
  $species = &HgAutomate::getSpecies($dbHost, $db);
  } elsif (length($hubSpecies) < 1) {
    die "ERROR: must supply: -species='Species name' for assembly hub build\n";
  } else {
    $species = $hubSpecies;
  }
  &HgAutomate::verbose(1,
	"\n db: $db, species: '$species'\n");

  # Optional variables.
  $liftRandoms = &optionalVar('liftRandoms', \%config);
  $nameTranslation = &optionalVar('nameTranslation', \%config);
  $geneScaffolds = &optionalVar('geneScaffolds', \%config);
  $knownToEnsembl = &optionalVar('knownToEnsembl', \%config);
  $skipInvalid = &optionalVar('skipInvalid', \%config);
  $haplotypeLift = &optionalVar('haplotypeLift', \%config);
  $liftUp = &optionalVar('liftUp', \%config);
  $liftMtOver = &optionalVar('liftMtOver', \%config);
  #	verify they actually do say yes
  if (defined $liftRandoms && $liftRandoms !~ m/^yes$/i) {
	undef $liftRandoms;
  }
  if (defined $geneScaffolds && $geneScaffolds !~ m/^yes$/i) {
	undef $geneScaffolds;
  }
  if (defined $knownToEnsembl && $knownToEnsembl !~ m/^yes$/i) {
	undef $knownToEnsembl;
  }
  if (defined $skipInvalid && $skipInvalid !~ m/^yes$/i) {
	undef $skipInvalid;
  }
  if (defined $haplotypeLift) {
    if (! -s $haplotypeLift) {
        print STDERR "ERROR: config file specifies a haplotypeLift file:\n";
	die "$haplotypeLift can not be found.\n";
    }
  }
  if (defined $liftUp) {
    if (! -s $liftUp) {
        print STDERR "ERROR: config file specifies a liftUp file:\n";
	die "$liftUp can not be found.\n";
    }
  }
  if (defined $liftMtOver) {
    if (! -s $liftMtOver) {
        print STDERR "ERROR: config file specifies a liftMtOver file:\n";
        die "$liftMtOver can not be found.\n";
    }
  } 

  # Make sure no unrecognized variables were given.
  my @stragglers = sort keys %config;
  if (scalar(@stragglers) > 0) {
    die "ERROR: parseConfig: config file $CONFIG has unrecognized variables:\n"
      . "    " . join(", ", @stragglers) . "\n" .
      "For a detailed list of supported variables, run \"$base -help\".\n";
  }
  $topDir = "/hive/data/genomes/$db";
} # parseConfig


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);

$secondsStart = `date "+%s"`;
chomp $secondsStart;

if (!defined $ENV{'USER'}) {
    print STDERR "ERROR: your shell environment does not define a USER";
    print STDERR "The USER identity is required for history";
    exit 255;
}

if (!defined $opt_ensVersion) {
    print STDERR "ERROR: must specify -ensVersion=NN on command line\n";
    &usage(1);
}

$ensVersion = $opt_ensVersion;
$versionString = "version $ensVersion/";
$versionString .= ucfirst(substr($EnsGeneAutomate::verToDate[$ensVersion],0,3));
$versionString .= ". " . substr($EnsGeneAutomate::verToDate[$ensVersion],3,4);

if (! $opt_vegaGene) {
  if ($versionListString !~ m/$ensVersion/) {
    print STDERR "ERROR: do not recognize given -ensVersion=$ensVersion\n";
    die "must be one of: $versionListString\n";
  }
}

$chromSizes = $opt_chromSizes ? $opt_chromSizes : "";

$hubSpecies = $opt_species ? $opt_species : "";

($CONFIG) = @ARGV;
&parseConfig($CONFIG);
$bedDir = "$topDir/$HgAutomate::trackBuild";

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);
printf STDERR "# running debug mode, will not execute scripts\n" if ($opt_debug);

# Establish previous version
$previousEnsVersion = `hgsql -Ne 'select max(version) from trackVersion where name="ensGene" AND db="$db" AND version<$ensVersion;' hgFixed`;
chomp $previousEnsVersion;
if ( $previousEnsVersion eq 'NULL') { $previousEnsVersion=0;}

# Establish what directory we will work in, tack on the ensembl version ID.
if ($opt_vegaGene) {
  if ($db !~ m/^mm|^hg|^danRer/) {
    printf STDERR "ERROR: this is database: $db\n";
    die "vegaGene build only good on human, mouse or zebrafish\n";
  }
  $buildDir = $opt_buildDir ? $opt_buildDir :
    "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/vega.$ensVersion";
  $previousBuildDir =
    "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/vega.$previousEnsVersion";
} else {
  $buildDir = $opt_buildDir ? $opt_buildDir :
    "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/ensGene.$ensVersion";
  $previousBuildDir =
    "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/ensGene.$previousEnsVersion";
}

if ($opt_vegaGene) {
    $vegaSpecies = "mouse" if ($db =~ m/^mm/);
    $vegaSpecies = "human" if ($db =~ m/^hg/);
    $vegaSpecies = "danio" if ($db =~ m/^danRer/);
    $vegaPep = "Mus_musculus.VEGA$ensVersion" if ($db =~ m/^mm/);
    $vegaPep = "Homo_sapiens.VEGA$ensVersion" if ($db =~ m/^hg/);
    $vegaPep = "Danio_rerio.VEGA$ensVersion" if ($db =~ m/^danRer/);
    $ensGtfUrl = "ftp://ftp.sanger.ac.uk/pub/vega/$vegaSpecies/gtf_file.gz";
    $ensGtfUrl = "ftp://ftp.sanger.ac.uk/pub/vega/$vegaSpecies/gtf_human_rel$ensVersion.gz" if ($db =~ m/^hg/);;
    $ensPepUrl = "ftp://ftp.sanger.ac.uk/pub/vega/$vegaSpecies/pep/$vegaPep.$ensVersion.pep.all.fa.gz";
    $ensMySqlUrl = "";
    $ensVersionDateReference = `date "+%Y-%m-%d"`;
    chomp $ensVersionDateReference;
} else {
  ($ensGtfUrl, $ensPepUrl, $ensMySqlUrl, $ensVersionDateReference) =
    &EnsGeneAutomate::ensGeneVersioning($db, $ensVersion );
}

die "ERROR: download: can not find Ensembl version $ensVersion FTP URL for UCSC database $db"
    if (!defined $ensGtfUrl);

$ensGtfFile = basename($ensGtfUrl);
$ensPepFile = basename($ensPepUrl);
&HgAutomate::verbose(2,"Ensembl GTF URL: $ensGtfUrl\n");
&HgAutomate::verbose(2,"Ensembl GTF File: $ensGtfFile\n");
&HgAutomate::verbose(2,"Ensembl PEP URL: $ensPepUrl\n");
&HgAutomate::verbose(2,"Ensembl PEP File: $ensPepFile\n");
&HgAutomate::verbose(2,"Ensembl MySql URL: $ensMySqlUrl\n");
&HgAutomate::verbose(2,"Ensembl Date Reference: $ensVersionDateReference\n");

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

