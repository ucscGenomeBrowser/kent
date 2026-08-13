#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doNcbiGene.pl instead.

use Getopt::Long;
use warnings;
use strict;
use File::stat;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;
use AsmHub;

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    $opt_assemblySource
    $opt_chromSizes
    $opt_namesFile
    $opt_liftFile
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'ncbiGene', func => \&doNcbiGene },
      { name => 'cleanup',  func => \&doCleanup },
    ]
				);

# Option defaults:
my $workhorse = 'hgwdev';
my $defaultWorkhorse = 'hgwdev';
my $defaultFileServer = 'hgwdev';
my $fileServer = 'hgwdev';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  my ($status, $detailed) = @_;
  print STDERR "
usage: $base [options] asmId db
required arguments:
    asmId              - assembly identifier at NCBI, e.g.
                          GCF_000001405.32_GRCh38.p6 -- used to locate the
                          source gff3/lift/remove.dups.list files.
    db                 - name used for the track's own output files, e.g.
                          bigDataUrl bbi/db.ncbiGene.bb -- may equal asmId
                          for a plain GenArk build, or be a custom -dbName.

options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default (current directory).
                          This *is* the runDir -- typically the hub's
                          trackData/ncbiGene -- no further nesting is added.
    -assemblySource dir   Directory holding \${asmId}_genomic.gff.gz.
    -chromSizes path      Path to the assembly's chrom.sizes file.
    -namesFile path       Path to the hub's \$db.names.tab (built by the
                          gatewayPage step), e.g.
                          \$buildDir/../../html/\$db.names.tab -- used
                          for the archived-version description page.
    -liftFile path        Optional lift file translating NCBI names to
                          UCSC names, e.g.
                          \$buildDir/../../sequence/\$asmId.ncbiToUcsc.lift
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('workhorse' => $defaultWorkhorse,
					'fileServer' => $defaultFileServer);
  print STDERR "
Automates construction of the 'ncbiGene' track from an assembly's own
NCBI GFF3 gene predictions, for assembly hub (GenArk) builds. Steps:
    ncbiGene: if a previous \$db.ncbiGene.bb exists and the source gff
              is newer, translate the current gff3 into a bigGenePred track,
              building it entirely under *.new file names so the existing
              live track is never touched while the build might still fail.
              Only once that build has fully succeeded is the previous
              version archived under archive/<date>/ (keyed by the previous
              build's own gff-derived mtime) and the new *.new files
              promoted into place.
    cleanup:  compress intermediate files
";
  print "\n";
  exit $status;
}

# Globals:
my ($asmId, $db);
my ($buildDir, $assemblySource, $chromSizes, $namesFile, $liftFile);
my ($secondsStart, $secondsEnd);

sub checkOptions {
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      'assemblySource=s',
		      'chromSizes=s',
		      'namesFile=s',
		      'liftFile=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $workhorse = $opt_workhorse if ($opt_workhorse);
  $fileServer = $opt_fileServer if ($opt_fileServer);
}

# same mtime-compare doAssemblyHub.pl has used all along
sub needsUpdate($$) {
  my ($source, $result) = @_;
  if (-s $result) {
    return (stat($source)->mtime > stat($result)->mtime) ? 1 : 0;
  }
  return 1;
}

#########################################################################
# archive whatever the previous build left behind, keyed by the date
# that was stamped onto it (the existing 'touch -r $gffFile' convention
# means the old .bb's mtime already *is* the previous source gff's date,
# so it doubles as a version string for free -- no separate release
# metadata needed, unlike ncbiRefSeq's NCBI-supplied $verString).
sub archivePriorVersion {
  my $priorBb = "$buildDir/$db.ncbiGene.bb";
  return if (! -s "$priorBb");

  my $priorMtime = stat($priorBb)->mtime;
  my ($mday,$mon,$year) = (localtime($priorMtime))[3,4,5];
  my $priorVersion = sprintf("%04d-%02d-%02d", $year+1900, $mon+1, $mday);
  my $archiveDir = "$buildDir/archive/$priorVersion";

  if ( -d "$archiveDir" ) {
    &HgAutomate::verbose(1,
      "# ncbiGene: archive/$priorVersion already exists, not re-archiving\n");
    return;
  }
  &HgAutomate::mustMkdir($archiveDir);
  foreach my $ext (qw( bb gtf.gz ix ixx stats.txt )) {
    my $f = "$buildDir/$db.ncbiGene.$ext";
    rename($f, "$archiveDir/$db.ncbiGene.$ext") if (-e "$f");
  }
  my $geneAttrs = "$buildDir/$db.geneAttrs.ncbi.txt.gz";
  rename($geneAttrs, "$archiveDir/$db.geneAttrs.ncbi.txt.gz") if (-e "$geneAttrs");
  my $fb = "$buildDir/fb.$db.ncbiGene.txt";
  rename($fb, "$archiveDir/fb.$db.ncbiGene.txt") if (-e "$fb");

  writeArchiveHub($archiveDir, $priorVersion);

  &HgAutomate::verbose(1,
    "# ncbiGene: archived previous version to archive/$priorVersion/\n");
} # archivePriorVersion

#########################################################################
# promote a successful *.new build into the live $db.ncbiGene.* names.
# Only ever called after archivePriorVersion() and only after the boss
# script below has exited 0 -- i.e. after the new track has already been
# fully built and validated under *.new names, so every rename() here is
# just swapping a already-good file into place, never risking the live
# track on a build that might still fail.
sub promoteNewBuild {
  foreach my $ext (qw( bb gtf.gz ix ixx stats.txt )) {
    my $new = "$buildDir/$db.ncbiGene.$ext.new";
    rename($new, "$buildDir/$db.ncbiGene.$ext") if (-e "$new");
  }
  my $geneAttrsNew = "$buildDir/$db.geneAttrs.ncbi.txt.new";
  rename($geneAttrsNew, "$buildDir/$db.geneAttrs.ncbi.txt") if (-e "$geneAttrsNew");
  my $fbNew = "$buildDir/fb.$db.ncbiGene.txt.new";
  rename($fbNew, "$buildDir/fb.$db.ncbiGene.txt") if (-e "$fbNew");
} # promoteNewBuild

#########################################################################
# a single hub.txt (useOneFile on) that puts just the archived ncbiGene
# track up for viewing.  No twoBitPath/organism/defaultPos are needed in
# the genome stanza: the true NCBI accession is already a published
# GenArk genome, so 'genome $accession' with no twoBitPath just attaches
# this one extra track to that already-known assembly (see
# trackHubGenomeReadRa() in hg/lib/trackHub.c -- twoBitPath is only
# required when *introducing* a new genome).
sub writeArchiveHub {
  my ($archiveDir, $priorVersion) = @_;
  my $haveIx = ( -s "$archiveDir/$db.ncbiGene.ix" );

  writeArchiveHtml($archiveDir, $priorVersion);

  # 'genome' wants the bare accession (GCF_937001465.1), not the full
  # asmId with its _assemblyName suffix (GCF_937001465.1_mOrcOrc1.1) --
  # that suffix is what NCBI adds for the gff/download file names, but
  # it is not the name the genome is registered under.  This must come
  # from the true NCBI $asmId, not the output-naming $db, since $db may
  # be an arbitrary -dbName.
  my @parts = split('_', $asmId);
  my $accession = "$parts[0]_$parts[1]";

  my $hubTxt = "$archiveDir/hub.txt";
  open(my $fh, ">", $hubTxt) or die "can not write $hubTxt: $!";
  print $fh <<_HUB_;
hub ${asmId}_ncbiGene_${priorVersion}
shortLabel $asmId ncbiGene archive $priorVersion
longLabel Archived NCBI GFF3 gene track for $asmId, superseded $priorVersion when NCBI updated the source annotation
useOneFile on
email genome-www\@soe.ucsc.edu

genome $accession

track ncbiGene
shortLabel NCBI GenBank ($priorVersion)
longLabel Gene models submitted to GenBank, ENA, DDBJ -- archived version $priorVersion
visibility pack
color 0,80,150
altColor 150,80,0
colorByStrand 0,80,150 150,80,0
type bigGenePred
bigDataUrl $db.ncbiGene.bb
html $db.ncbiGene
_HUB_
  if ($haveIx) {
    print $fh "searchIndex name\nsearchTrix $db.ncbiGene.ix\n";
  }
  close($fh);
} # writeArchiveHub

#########################################################################
# the description page for the archived hub above, built by handing the
# same paths off to AsmHub::ncbiGeneDescription() that asmHubNcbiGene.pl
# uses for the live track -- one source of truth for what this page says,
# just with an extra banner noting it is a superseded version.
sub writeArchiveHtml {
  my ($archiveDir, $priorVersion) = @_;
  my $bbPath = "$archiveDir/$db.ncbiGene.bb";
  my $statsPath = "$archiveDir/$db.ncbiGene.stats.txt";
  my $archiveNote = "This is an archived copy of the ncbiGene track as it stood on $priorVersion, before NCBI's source GFF3 annotation was updated to a newer version.";

  my $html = AsmHub::ncbiGeneDescription($bbPath, $statsPath, $chromSizes,
                                          $namesFile, $asmId, $archiveNote);

  my $htmlPath = "$archiveDir/$db.ncbiGene.html";
  open(my $fh, ">", $htmlPath) or die "can not write $htmlPath: $!";
  print $fh $html;
  close($fh);
} # writeArchiveHtml

#########################################################################
# * step: ncbiGene [workhorse]
sub doNcbiGene {
  my $gffFile = "$assemblySource/${asmId}_genomic.gff.gz";
  if ( ! -s "${gffFile}" ) {
    &HgAutomate::verbose(1, "# step ncbiGene: no gff file found at:\n#  $gffFile\n");
    return;
  }
  if ( ! needsUpdate($gffFile, "$buildDir/$db.ncbiGene.bb") ) {
    &HgAutomate::verbose(1, "# ncbiGene step previously completed\n");
    return;
  }
  &HgAutomate::mustMkdir($buildDir);

  # NOTE: the previous live version is deliberately *not* archived here.
  # Everything below builds under *.new file names, leaving the existing
  # live track completely untouched; archivePriorVersion() only runs
  # after $bossScript->execute() returns below, i.e. only once the new
  # build has actually succeeded (execute() dies on any remote failure,
  # via 'set -e' in the boss script, so a failed build never reaches
  # that line and the live track is left exactly as it was).

  my $whatItDoes = "translate NCBI GFF3 gene definitions into a track";
  # NOTE: must not be named doNcbiGene.bash -- doAssemblyHub.pl's own
  # wrapper script that invokes this program is *already* named
  # $buildDir/doNcbiGene.bash and is still running (mid-ssh-exec) at the
  # moment this line runs.  Reusing that filename here truncates and
  # rewrites the file the outer shell is still reading, corrupting its
  # read position and producing a bogus "syntax error" once the outer
  # shell resumes -- after the real work below has already succeeded.
  my $bossScript = newBash HgRemoteScript("$buildDir/buildNcbiGene.bash",
                    $workhorse, $buildDir, $whatItDoes);

  my $dupList = "";
  # this list is curated by hand into the build's own download/ area,
  # not something NCBI supplies in $assemblySource
  if ( -s "${buildDir}/../../download/${asmId}.remove.dups.list" ) {
    $dupList = " | (grep -v -f \"${buildDir}/../../download/${asmId}.remove.dups.list\"  || true)";
  }

  $bossScript->add(<<_EOF_
export asmId=$db
export gffFile=$gffFile
export chromSizes=$chromSizes

function cleanUp() {
  rm -f \$asmId.ncbiGene.genePred.gz \$asmId.ncbiGene.genePred
  rm -f \$asmId.geneAttrs.ncbi.txt.new
}

if [ \$gffFile -nt \$asmId.ncbiGene.bb ]; then
  ln -sf \$gffFile ./
  (gff3ToGenePred -warnAndContinue -useName \\
    -attrsOut=\$asmId.geneAttrs.ncbi.txt.new \$gffFile stdout \\
      2>> \$asmId.ncbiGene.log.txt || true) | genePredFilter \\
         -chromSizes=\$chromSizes stdin stdout \\
        $dupList | gzip -c > \$asmId.ncbiGene.genePred.gz
  genePredCheck \$asmId.ncbiGene.genePred.gz
  zcat \$asmId.ncbiGene.genePred.gz > ncbiGene.\$asmId.gp
  genePredToGtf -utr file ncbiGene.\$asmId.gp stdout | gzip -c > \$asmId.ncbiGene.gtf.gz.new
  rm -f ncbiGene.\$asmId.gp
  export howMany=`genePredCheck \$asmId.ncbiGene.genePred.gz 2>&1 | grep "^checked" | awk '{print \$2}'`
  if [ "\${howMany}" -eq 0 ]; then
     printf "# ncbiGene: no gene definitions found in \$gffFile\\n"
     cleanUp
     exit 0
  fi
  export ncbiGenePred="\$asmId.ncbiGene.genePred.gz"
_EOF_
  );
  if ( -s "$liftFile" ) {
    $bossScript->add(<<_EOF_
  liftUp -extGenePred -type=.gp stdout \\
    $liftFile warn \\
      \$asmId.ncbiGene.genePred.gz | gzip -c \\
        > \$asmId.ncbiGene.ucsc.genePred.gz
  ncbiGenePred="\$asmId.ncbiGene.ucsc.genePred.gz"
_EOF_
    );
  }
  $bossScript->add(<<_EOF_
  ~/kent/src/hg/utils/automation/gpToIx.pl \$ncbiGenePred \\
    > \$asmId.gpToIx.txt
  ~/kent/src/hg/utils/automation/gffAttrsToIx.py \$asmId.geneAttrs.ncbi.txt.new \\
     \$ncbiGenePred > \$asmId.attrsToIx.txt
  sort -u \$asmId.gpToIx.txt \$asmId.attrsToIx.txt > \$asmId.ncbiGene.ix.txt
  if [ -s \$asmId.ncbiGene.ix.txt ]; then
    ixIxx \$asmId.ncbiGene.ix.txt \$asmId.ncbiGene.ix.new \$asmId.ncbiGene.ixx.new
  fi
  rm -f \$asmId.ncbiGene.ix.txt \$asmId.gpToIx.txt \$asmId.attrsToIx.txt
  genePredToBigGenePred \$ncbiGenePred stdout \\
      | sort -k1,1 -k2,2n > \$asmId.ncbiGene.bed
  (bedToBigBed -type=bed12+8 -tab -as=\$HOME/kent/src/hg/lib/bigGenePred.as \\
      -extraIndex=name \$asmId.ncbiGene.bed \\
        \$chromSizes \$asmId.ncbiGene.bb.new || true)
  if [ ! -s "\$asmId.ncbiGene.bb.new" ]; then
    printf "# ncbiGene: failing bedToBigBed\\n" 1>&2
    exit 255
  fi
  touch -r \$gffFile \$asmId.ncbiGene.bb.new
  bigBedInfo \$asmId.ncbiGene.bb.new | egrep "^itemCount:|^basesCovered:" \\
    | sed -e 's/,//g' > \$asmId.ncbiGene.stats.txt.new
  LC_NUMERIC=en_US /usr/bin/printf "# ncbiGene %s %'d %s %'d\\n" `cat \$asmId.ncbiGene.stats.txt.new` | xargs echo

  # basesCovered comes straight out of the bigBedInfo call above --
  # no need to separately rebuild exons via bedToExons/bedSingleCover.pl
  export totalBases=`ave -col=2 \$chromSizes | grep total | awk '{printf "%d", \$NF}'`
  export basesCovered=`grep basesCovered \$asmId.ncbiGene.stats.txt.new | awk '{printf "%s", \$NF}'`
  export percentCovered=`echo \$basesCovered \$totalBases | awk '{printf "%.3f", 100.0*\$1/\$2}'`
  printf "%d bases of %d (%s%%) in intersection\\n" "\$basesCovered" "\$totalBases" "\$percentCovered" > fb.\$asmId.ncbiGene.txt.new

  # Everything above is built entirely under *.new (or otherwise
  # non-live) file names -- nothing under the live \$asmId.ncbiGene.*
  # names has been touched.  The calling doNcbiGene.pl only archives the
  # previous live version and promotes these *.new files into place
  # once this script has exited 0, so a failure at any point above
  # (this script runs under 'set -e') leaves the existing live track
  # completely untouched.
else
  printf "# ncbiGene step previously completed\\n" 1>&2
fi
_EOF_
  );
  $bossScript->execute();

  # reached only if the boss script above exited 0 -- the new build is
  # complete and validated under *.new file names, so it is now safe to
  # archive the previous live version and promote the new one into place
  archivePriorVersion();
  promoteNewBuild();
} # doNcbiGene

#########################################################################
# * step: cleanup [fileServer]
sub doCleanup {
  my $whatItDoes = "compress intermediate files";
  my $bossScript = new HgRemoteScript("$buildDir/doCleanup.csh", $fileServer,
				      $buildDir, $whatItDoes);
  # guard each file individually -- this step has no needsUpdate() check
  # of its own, so it can be re-run (e.g. by the outer driver script) on
  # a later invocation where ncbiGene did nothing new; by then these are
  # already $file.gz from the previous successful cleanup, and a plain
  # 'gzip -f' on a now-missing plain-named file would die under this
  # script's 'csh -e'
  $bossScript->add(<<_EOF_
foreach f ( $db.geneAttrs.ncbi.txt $db.ncbiGene.log.txt $db.ncbiGene.bed )
  if ( -e \$f ) then
    gzip -f \$f
  endif
end
_EOF_
  );
  $bossScript->execute();
} # doCleanup

#########################################################################
# main

&HgAutomate::closeStdin();

&checkOptions();
&usage(1) if (scalar(@ARGV) != 2);

$secondsStart = `date "+%s"`;
chomp $secondsStart;

($asmId, $db) = @ARGV;

$assemblySource = $opt_assemblySource or die "ERROR: -assemblySource is required\n";
$chromSizes = $opt_chromSizes or die "ERROR: -chromSizes is required\n";
$namesFile = $opt_namesFile or die "ERROR: -namesFile is required\n";
$liftFile = $opt_liftFile ? $opt_liftFile : "";
$buildDir = $opt_buildDir ? $opt_buildDir : `pwd`;
chomp $buildDir;

$stepper->execute();

$secondsEnd = `date "+%s"`;
chomp $secondsEnd;
my $elapsedSeconds = $secondsEnd - $secondsStart;
my $elapsedMinutes = int($elapsedSeconds/60);
$elapsedSeconds -= $elapsedMinutes * 60;

my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" : "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done !$upThrough  Elapsed time: ${elapsedMinutes}m${elapsedSeconds}s\n");
&HgAutomate::verbose(1, " *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");
