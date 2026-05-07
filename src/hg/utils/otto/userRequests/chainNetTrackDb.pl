#!/usr/bin/env perl

# chainNetTrackDb.pl - generate a chainNet trackDb stanza for a UCSC native
#   target database, by probing axtChain/*.bb files in the current build
#   directory.  Writes the result to the kent trackDb tree at
#   ~/kent/src/hg/makeDb/trackDb/<clade>/<targetDb>/<targetDb>.<queryAcc>.chainNet.ra
#
# Run from a build directory, e.g.:
#   /hive/data/genomes/ce11/bed/blastz.GCA_000180635.4.swap
#   /hive/data/genomes/hg38/bed/lastzMm39.YYYY-MM-DD
#
# usage: chainNetTrackDb.pl <targetDb> <queryDb>
#
# Probes (ignoring any *quick* files):
#   ./axtChain/chain<Query>.bb           + chain<Query>Link.bb
#   ./axtChain/chainLiftOver<Query>.bb   + chainLiftOver<Query>Link.bb
#   ./axtChain/chainSyn<Query>.bb        + chainSyn<Query>Link.bb
#   ./axtChain/chainRBest<Query>.bb      + chainRBest<Query>Link.bb
#   ./axtChain/<qAcc>.net.bb             + <qAcc>.net.summary.bb
#   ./axtChain/<qAcc>.synNet.bb          + <qAcc>.synNet.summary.bb
#   ./axtChain/<qAcc>.rbestNet.bb        + <qAcc>.rbestNet.summary.bb
# Where:
#   qAcc  = queryDb stripped to GC[AF]_NNNNNNNNN.N for GenArk, else queryDb
#   Query = ucfirst(qAcc)
#
# Metadata sources:
#   target (UCSC native): hgcentraltest.dbDb
#   query  GenArk:        hgcentraltest.genark + assembly_report.txt
#   query  UCSC native:   hgcentraltest.dbDb

use strict;
use warnings;
use Cwd qw(getcwd abs_path);
use File::Path qw(make_path);
use Fcntl qw(:flock);

my @months = qw( zero Jan. Feb. Mar. Apr. May Jun. Jul. Aug. Sep. Oct. Nov. Dec. );

sub usage {
  printf STDERR "usage: chainNetTrackDb.pl <targetDb> <queryDb>\n";
  printf STDERR "  Run from a build directory containing an axtChain/ subdir.\n";
  printf STDERR "  Probes axtChain/*.bb (ignoring *quick*) to determine which\n";
  printf STDERR "  chainNet stanzas to emit, and writes them to:\n";
  printf STDERR "  ~/kent/src/hg/makeDb/trackDb/<clade>/<targetDb>/<targetDb>.<qAcc>.chainNet.ra\n";
  printf STDERR "  Only handles UCSC native targetDb; GenArk targets use doTrackDb.bash.\n";
  exit 255;
}

usage() if (scalar(@ARGV) != 2);

my $targetDb = shift;
my $queryDb  = shift;

if ($targetDb =~ /^GC[AF]_/) {
  printf STDERR "ERROR: GenArk target '%s' is not handled here; use doTrackDb.bash\n",
    $targetDb;
  exit 255;
}

my $axtChain = "axtChain";
if (! -d $axtChain) {
  printf STDERR "ERROR: must be run from a build directory; no %s/ in cwd %s\n",
    $axtChain, getcwd();
  exit 255;
}

# normalize query - strip _asmName suffix for GenArk
my $qAcc = $queryDb;
if ($queryDb =~ /^(GC[AF]_\d+\.\d+)/) {
  $qAcc = $1;
}
my $Query = ucfirst($qAcc);

##############################################################################
# look up target metadata from dbDb
##############################################################################
my $tRow = `hgsql -N -e 'SELECT organism,scientificName,taxId,description FROM dbDb WHERE name="$targetDb"' hgcentraltest 2>/dev/null`;
chomp $tRow;
if (length($tRow) < 1) {
  printf STDERR "ERROR: target db '%s' not found in hgcentraltest.dbDb\n", $targetDb;
  exit 255;
}
my ($tOrganism, $tSciName, $tTaxId, $tDesc) = split(/\t/, $tRow);
$tOrganism = "" if (!defined $tOrganism);

##############################################################################
# look up query metadata
##############################################################################
my $qOrganism = "";
my $qSciName  = "";
my $qTaxId    = "";
my $qDate     = "";
my $qAsmName  = "";

if ($qAcc =~ /^GC[AF]_/) {
  # GenArk: get commonName/scientificName/taxId/asmName from the genark
  # table; only fall back to assembly_report.txt for the assembly date,
  # which the genark table does not carry.
  my $gRow = `hgsql -N -e 'SELECT commonName,scientificName,taxId,asmName FROM genark WHERE gcAccession="$qAcc"' hgcentraltest 2>/dev/null`;
  chomp $gRow;
  if (length($gRow) < 1) {
    printf STDERR "ERROR: GenArk accession '%s' not found in hgcentraltest.genark\n", $qAcc;
    exit 255;
  }
  ($qOrganism, $qSciName, $qTaxId, $qAsmName) = split(/\t/, $gRow);
  $qOrganism = "" if (!defined $qOrganism);
  $qSciName  = "" if (!defined $qSciName);
  $qTaxId    = "" if (!defined $qTaxId);
  $qAsmName  = "" if (!defined $qAsmName);
  # commonName may be empty; fall back to scientificName
  if (length($qOrganism) < 1) {
    $qOrganism = $qSciName;
  }

  # assembly_report.txt: only needed for the assembly date
  my $gcX = substr($qAcc, 0, 3);
  my $d0  = substr($qAcc, 4, 3);
  my $d1  = substr($qAcc, 7, 3);
  my $d2  = substr($qAcc, 10, 3);
  my $hubBuild = "/hive/data/genomes/asmHubs/allBuild/$gcX/$d0/$d1/$d2";
  my $ncbiPath = "/hive/data/outside/ncbi/genomes/$gcX/$d0/$d1/$d2";

  my $asmReport = `ls -d $hubBuild/${qAcc}*/download/*assembly_report.txt 2>/dev/null | head -1`;
  chomp $asmReport;
  if (! -s $asmReport) {
    $asmReport = `ls -d $ncbiPath/${qAcc}*/${qAcc}*assembly_report.txt 2>/dev/null | head -1`;
    chomp $asmReport;
  }
  if (-s $asmReport) {
    my $ymd = `grep -i 'date:' $asmReport | head -1 | tr -d '\r' | sed -e 's/.*date: *//i;'`;
    chomp $ymd;
    if ($ymd =~ /^(\d{4})-(\d{1,2})/) {
      my ($y, $m) = ($1, $2 + 0);
      $qDate = sprintf("%s %d", $months[$m], $y) if ($m >= 1 && $m <= 12);
    }
  }
} else {
  # UCSC native: dbDb has organism, sciName, taxId, description
  my $qRow = `hgsql -N -e 'SELECT organism,scientificName,taxId,description FROM dbDb WHERE name="$qAcc"' hgcentraltest 2>/dev/null`;
  chomp $qRow;
  if (length($qRow) < 1) {
    printf STDERR "ERROR: query db '%s' not found in hgcentraltest.dbDb\n", $qAcc;
    exit 255;
  }
  my $desc;
  ($qOrganism, $qSciName, $qTaxId, $desc) = split(/\t/, $qRow);
  $qOrganism = "" if (!defined $qOrganism);
  # dbDb.description is typically the assembly date string, e.g. "Apr. 2017"
  $qDate = defined($desc) ? $desc : "";
}

##############################################################################
# probe axtChain/ for available files
##############################################################################
my $hasChain = (-s "$axtChain/chain${Query}.bb"
             && -s "$axtChain/chain${Query}Link.bb");
my $hasLO    = (-s "$axtChain/chainLiftOver${Query}.bb"
             && -s "$axtChain/chainLiftOver${Query}Link.bb");
my $hasSyn   = (-s "$axtChain/chainSyn${Query}.bb"
             && -s "$axtChain/chainSyn${Query}Link.bb");
my $hasRBest = (-s "$axtChain/chainRBest${Query}.bb"
             && -s "$axtChain/chainRBest${Query}Link.bb");

my $hasNet      = (-s "$axtChain/${qAcc}.net.bb"
                && -s "$axtChain/${qAcc}.net.summary.bb");
my $hasSynNet   = (-s "$axtChain/${qAcc}.synNet.bb"
                && -s "$axtChain/${qAcc}.synNet.summary.bb");
my $hasRBestNet = (-s "$axtChain/${qAcc}.rbestNet.bb"
                && -s "$axtChain/${qAcc}.rbestNet.summary.bb");

if (!$hasChain && !$hasLO && !$hasSyn && !$hasRBest
    && !$hasNet && !$hasSynNet && !$hasRBestNet) {
  printf STDERR "ERROR: no chain/net .bb files found in %s/ for %s\n",
    $axtChain, $Query;
  exit 255;
}

##############################################################################
# resolve output dir in the dedicated otto kent clone, take an exclusive
# lock on it to serialize concurrent invocations, and verify the working
# tree is pristine before we modify anything.
##############################################################################
my $kentTree = $ENV{OTTO_KENT_TREE} // "/hive/data/outside/genark/ottoKent/kent";
if (! -d "$kentTree/.git") {
  printf STDERR "ERROR: not a git working tree: %s\n", $kentTree;
  exit 255;
}

# blocking exclusive lock on a dotfile inside .git so it stays out of the
# tracked tree.  Kernel releases the lock on process exit, so no cleanup
# is needed.  $lockFh stays in scope until script end.
my $lockPath = "$kentTree/.git/chainNetTrackDb.lock";
open(my $lockFh, '>>', $lockPath)
  or die "ERROR: cannot open lock '$lockPath': $!\n";
if (!flock($lockFh, LOCK_EX)) {
  die "ERROR: cannot acquire lock '$lockPath': $!\n";
}

# refuse to run on a dirty tree -- this clone exists only for this script,
# so any uncommitted state means something went wrong last time.
my $dirty = `git -C "$kentTree" status --porcelain 2>&1`;
if (length($dirty) > 0) {
  printf STDERR "ERROR: '%s' has uncommitted changes:\n%s",
    $kentTree, $dirty;
  exit 255;
}

my @hits = grep { -d $_ } glob("$kentTree/src/hg/makeDb/trackDb/*/$targetDb");
if (scalar(@hits) != 1) {
  printf STDERR "ERROR: expected exactly 1 trackDb dir for '%s', found %d:\n",
    $targetDb, scalar(@hits);
  printf STDERR "  %s\n", $_ for @hits;
  exit 255;
}
my $outDir  = $hits[0];
my $outFile = "$outDir/$targetDb.$qAcc.chainNet.ra";

##############################################################################
# create /gbdb/<targetDb>/chainNet/ and symlink the axtChain/*.bb files there
# so the bigDataUrl/linkDataUrl/summary paths emitted below resolve.
# Each link in /gbdb/<tDb>/chainNet/ has the prefix '<tDb>.' to match the
# '$D.' prefix used in the trackDb stanzas.
##############################################################################
my $axtAbs = abs_path($axtChain);
if (!defined($axtAbs) || ! -d $axtAbs) {
  printf STDERR "ERROR: cannot resolve absolute path for %s/\n", $axtChain;
  exit 255;
}

my $gbdbDir = "/gbdb/$targetDb/chainNet";
if (! -d $gbdbDir) {
  make_path($gbdbDir);
  if (! -d $gbdbDir) {
    printf STDERR "ERROR: cannot create %s: %s\n", $gbdbDir, $!;
    exit 255;
  }
}

# pairs of (file in axtChain/, link basename in /gbdb/<tDb>/chainNet/)
# the link basename gets the '<targetDb>.' prefix so it matches '$D.<...>'
my @linkPairs = ();
if ($hasChain) {
  push @linkPairs, ["chain${Query}.bb",     "$targetDb.chain${Query}.bb"];
  push @linkPairs, ["chain${Query}Link.bb", "$targetDb.chain${Query}Link.bb"];
}
if ($hasLO) {
  push @linkPairs, ["chainLiftOver${Query}.bb",     "$targetDb.chainLiftOver${Query}.bb"];
  push @linkPairs, ["chainLiftOver${Query}Link.bb", "$targetDb.chainLiftOver${Query}Link.bb"];
}
if ($hasSyn) {
  push @linkPairs, ["chainSyn${Query}.bb",     "$targetDb.chainSyn${Query}.bb"];
  push @linkPairs, ["chainSyn${Query}Link.bb", "$targetDb.chainSyn${Query}Link.bb"];
}
if ($hasRBest) {
  push @linkPairs, ["chainRBest${Query}.bb",     "$targetDb.chainRBest${Query}.bb"];
  push @linkPairs, ["chainRBest${Query}Link.bb", "$targetDb.chainRBest${Query}Link.bb"];
}
if ($hasNet) {
  push @linkPairs, ["${qAcc}.net.bb",         "$targetDb.${qAcc}.net.bb"];
  push @linkPairs, ["${qAcc}.net.summary.bb", "$targetDb.${qAcc}.net.summary.bb"];
}
if ($hasSynNet) {
  push @linkPairs, ["${qAcc}.synNet.bb",         "$targetDb.${qAcc}.synNet.bb"];
  push @linkPairs, ["${qAcc}.synNet.summary.bb", "$targetDb.${qAcc}.synNet.summary.bb"];
}
if ($hasRBestNet) {
  push @linkPairs, ["${qAcc}.rbestNet.bb",         "$targetDb.${qAcc}.rbestNet.bb"];
  push @linkPairs, ["${qAcc}.rbestNet.summary.bb", "$targetDb.${qAcc}.rbestNet.summary.bb"];
}

for my $pair (@linkPairs) {
  my ($srcName, $dstName) = @$pair;
  my $src = "$axtAbs/$srcName";
  my $dst = "$gbdbDir/$dstName";
  if (-l $dst || -e $dst) {
    if (!unlink($dst)) {
      printf STDERR "ERROR: cannot remove existing '%s': %s\n", $dst, $!;
      exit 255;
    }
  }
  if (!symlink($src, $dst)) {
    printf STDERR "ERROR: symlink '%s' -> '%s' failed: %s\n", $dst, $src, $!;
    exit 255;
  }
}

##############################################################################
# compose stanza, write to file
##############################################################################
my $shortOrg = (length($qOrganism) > 0) ? $qOrganism : $qAcc;

my $longTail;
if ($qAsmName ne "") {
  $longTail = "${qAcc}_${qAsmName}";
} else {
  $longTail = $qAcc;
}
my $longInner = (length($qDate) > 0) ? "$qDate $longTail" : $longTail;

printf STDERR "### writing to $outFile\n";

open(my $fh, '>', $outFile)
  or die "ERROR: cannot write '$outFile': $!\n";

printf $fh "##############################################################################\n";
printf $fh "# %s - %s - %s - taxId: %s\n", $qAcc, $shortOrg, $qSciName, $qTaxId;
printf $fh "##############################################################################\n";
printf $fh "track chainNet%s\n", $Query;
printf $fh "compositeTrack on\n";
printf $fh "shortLabel %s Chain/Net\n", $shortOrg;
printf $fh "longLabel %s (%s), Chain and Net Alignments\n", $shortOrg, $longInner;
printf $fh "subGroup1 view Views chain=Chain net=Net\n";
printf $fh "dragAndDrop subTracks\n";
printf $fh "visibility hide\n";
printf $fh "group compGeno\n";
printf $fh "noInherit on\n";
printf $fh "priority 100.1\n";
printf $fh "color 0,0,0\n";
printf $fh "altColor 100,50,0\n";
printf $fh "type bed 3\n";
printf $fh "sortOrder view=+\n";
printf $fh "matrix 16 91,-114,-31,-123,-114,100,-125,-31,-31,-125,100,-114,-123,-31,-114,91\n";
printf $fh "chainMinScore 3000\n";
printf $fh "chainLinearGap medium\n";
printf $fh "matrixHeader A, C, G, T\n";
printf $fh "otherDb %s\n", $qAcc;
printf $fh "html chainNet\n";
printf $fh "\n";

##############################################################################
# Chain view
##############################################################################
if ($hasChain || $hasLO || $hasSyn || $hasRBest) {
  printf $fh "    track chainNet%sViewchain\n", $Query;
  printf $fh "    shortLabel Chain\n";
  printf $fh "    view chain\n";
  printf $fh "    visibility pack\n";
  printf $fh "    parent chainNet%s\n", $Query;
  printf $fh "    spectrum on\n";
  printf $fh "\n";

  if ($hasChain) {
    printf $fh "        track chain%s\n", $Query;
    printf $fh "        parent chainNet%sViewchain\n", $Query;
    printf $fh "        subGroups view=chain\n";
    printf $fh "        shortLabel %s Chain\n", $shortOrg;
    printf $fh "        longLabel %s (%s) Chained Alignments\n", $shortOrg, $longInner;
    printf $fh "        type bigChain %s\n", $qAcc;
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.chain%s.bb\n", $Query;
    printf $fh "        linkDataUrl /gbdb/\$D/chainNet/\$D.chain%sLink.bb\n", $Query;
    printf $fh "\n";
  }

  if ($hasLO) {
    printf $fh "        track chainLiftOver%s\n", $Query;
    printf $fh "        parent chainNet%sViewchain\n", $Query;
    printf $fh "        subGroups view=chain\n";
    printf $fh "        shortLabel %s liftOverChain\n", $shortOrg;
    printf $fh "        longLabel %s (%s) Lift Over Chained Alignments\n", $shortOrg, $longInner;
    printf $fh "        type bigChain %s\n", $qAcc;
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.chainLiftOver%s.bb\n", $Query;
    printf $fh "        linkDataUrl /gbdb/\$D/chainNet/\$D.chainLiftOver%sLink.bb\n", $Query;
    printf $fh "\n";
  }

  if ($hasSyn) {
    printf $fh "        track chainSyn%s\n", $Query;
    printf $fh "        parent chainNet%sViewchain\n", $Query;
    printf $fh "        subGroups view=chain\n";
    printf $fh "        shortLabel %s synChain\n", $shortOrg;
    printf $fh "        longLabel %s (%s) Syntenic Chained Alignments\n", $shortOrg, $longInner;
    printf $fh "        type bigChain %s\n", $qAcc;
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.chainSyn%s.bb\n", $Query;
    printf $fh "        linkDataUrl /gbdb/\$D/chainNet/\$D.chainSyn%sLink.bb\n", $Query;
    printf $fh "\n";
  }

  if ($hasRBest) {
    printf $fh "        track chainRBest%s\n", $Query;
    printf $fh "        parent chainNet%sViewchain\n", $Query;
    printf $fh "        subGroups view=chain\n";
    printf $fh "        shortLabel %s rBestChain\n", $shortOrg;
    printf $fh "        longLabel %s (%s) Reciprocal Best Chained Alignments\n", $shortOrg, $longInner;
    printf $fh "        type bigChain %s\n", $qAcc;
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.chainRBest%s.bb\n", $Query;
    printf $fh "        linkDataUrl /gbdb/\$D/chainNet/\$D.chainRBest%sLink.bb\n", $Query;
    printf $fh "\n";
  }
}

##############################################################################
# Net view
##############################################################################
if ($hasNet || $hasSynNet || $hasRBestNet) {
  printf $fh "    track mafNet%sViewnet\n", $Query;
  printf $fh "    shortLabel Net\n";
  printf $fh "    view net\n";
  printf $fh "    visibility full\n";
  printf $fh "    parent chainNet%s\n", $Query;
  printf $fh "\n";

  if ($hasNet) {
    printf $fh "        track net%s\n", $Query;
    printf $fh "        parent mafNet%sViewnet\n", $Query;
    printf $fh "        subGroups view=net\n";
    printf $fh "        shortLabel %s mafNet\n", $shortOrg;
    printf $fh "        longLabel %s (%s) mafNet Alignment\n", $shortOrg, $longInner;
    printf $fh "        type bigMaf\n";
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.%s.net.bb\n", $qAcc;
    printf $fh "        summary /gbdb/\$D/chainNet/\$D.%s.net.summary.bb\n", $qAcc;
    printf $fh "        speciesOrder %s\n", $qAcc;
    printf $fh "        speciesLabels %s=\"%s\"\n", $qAcc, $shortOrg;
    printf $fh "\n";
  }

  if ($hasSynNet) {
    printf $fh "        track synNet%s\n", $Query;
    printf $fh "        parent mafNet%sViewnet\n", $Query;
    printf $fh "        subGroups view=net\n";
    printf $fh "        shortLabel %s synNet\n", $shortOrg;
    printf $fh "        longLabel %s (%s) Syntenic Net Alignment\n", $shortOrg, $longInner;
    printf $fh "        type bigMaf\n";
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.%s.synNet.bb\n", $qAcc;
    printf $fh "        summary /gbdb/\$D/chainNet/\$D.%s.synNet.summary.bb\n", $qAcc;
    printf $fh "        speciesOrder %s\n", $qAcc;
    printf $fh "        speciesLabels %s=\"%s\"\n", $qAcc, $shortOrg;
    printf $fh "\n";
  }

  if ($hasRBestNet) {
    printf $fh "        track rbestNet%s\n", $Query;
    printf $fh "        parent mafNet%sViewnet\n", $Query;
    printf $fh "        subGroups view=net\n";
    printf $fh "        shortLabel %s rBestNet\n", $shortOrg;
    printf $fh "        longLabel %s (%s) Reciprocal Best Net Alignment\n", $shortOrg, $longInner;
    printf $fh "        type bigMaf\n";
    printf $fh "        bigDataUrl /gbdb/\$D/chainNet/\$D.%s.rbestNet.bb\n", $qAcc;
    printf $fh "        summary /gbdb/\$D/chainNet/\$D.%s.rbestNet.summary.bb\n", $qAcc;
    printf $fh "        speciesOrder %s\n", $qAcc;
    printf $fh "        speciesLabels %s=\"%s\"\n", $qAcc, $shortOrg;
    printf $fh "\n";
  }
}

close($fh);

printf STDERR "wrote %s\n", $outFile;

##############################################################################
# ensure trackDb.ra in the same dir has an 'include <basename> alpha' line
# for the newly written .ra file; append it if not already present
##############################################################################
my $trackDbRa  = "$outDir/trackDb.ra";
my $baseName   = "$targetDb.$qAcc.chainNet.ra";
my $includeLn  = "include $baseName alpha";

my $alreadyIncluded = 0;
if (-e $trackDbRa) {
  open(my $rh, '<', $trackDbRa)
    or die "ERROR: cannot read '$trackDbRa': $!\n";
  while (my $line = <$rh>) {
    if ($line =~ /^\s*include\s+\Q$baseName\E(\s|$)/) {
      $alreadyIncluded = 1;
      last;
    }
  }
  close($rh);
}

if (!$alreadyIncluded) {
  open(my $ah, '>>', $trackDbRa)
    or die "ERROR: cannot append to '$trackDbRa': $!\n";
  printf $ah "%s\n", $includeLn;
  close($ah);
  printf STDERR "appended '%s' to %s\n", $includeLn, $trackDbRa;
}

##############################################################################
# stage the new .ra (and trackDb.ra if it changed) and commit.
# Requires user.name / user.email configured in the clone's git config.
##############################################################################
my $newRel = $outFile;     $newRel =~ s,^\Q$kentTree/\E,,;
my $tdbRel = $trackDbRa;   $tdbRel =~ s,^\Q$kentTree/\E,,;

if (system('git', '-C', $kentTree, 'add', '--', $newRel, $tdbRel) != 0) {
  printf STDERR "ERROR: 'git add' failed in %s\n", $kentTree;
  exit 255;
}

my $gitChanged = 1;
# nothing to commit if neither file actually changed (idempotent re-runs)
if (system('git', '-C', $kentTree, 'diff', '--cached', '--quiet') == 0) {
  printf STDERR "no changes to source tree\n";
  $gitChanged = 0;
}

if ($gitChanged) {
  my $msg = sprintf("chainNet trackDb for %s %s, otto liftOver",
                  $targetDb, $qAcc);
  if (system('git', '-C', $kentTree, 'commit', '-m', $msg) != 0) {
    printf STDERR "ERROR: 'git commit' failed in %s\n", $kentTree;
    exit 255;
  }
  printf STDERR "committed: %s\n", $msg;
}

##############################################################################
# build/install trackDb for the target database (default release + alpha)
# so the new tracks show up in the genome browser.
##############################################################################
my $tdbDir = "$kentTree/src/hg/makeDb/trackDb";
for my $args ( ["DBS=$targetDb"], ["DBS=$targetDb", "alpha"] ) {
  printf STDERR "make -c %s %s\n", $tdbDir, $args->[0];
  if (system('make', '-C', $tdbDir, @$args) != 0) {
    printf STDERR "ERROR: 'make -C %s %s' failed\n",
      $tdbDir, join(' ', @$args);
    exit 255;
  }
}

exit 0;
