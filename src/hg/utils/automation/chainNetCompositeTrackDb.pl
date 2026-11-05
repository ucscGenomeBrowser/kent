#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/chainNetCompositeTrackDb.pl instead.

# chainNetCompositeTrackDb.pl - construct one composite trackDb .ra for the
#   chain/net alignments of a UCSC database target to one or more query
#   assemblies, any mix of lastz and minimap2 builds, primary or -swap,
#   the UCSC database equivalent of asmHubChainNetTrackDb.pl and
#   asmHubMiniMap2ChainNetTrackDb.pl.  The build directories are found
#   with the same ChainNetInfo::findAlignments() as chainNetDescription.pl,
#   which makes the description page for this composite.
#   With -cladeFile from chainNetCladeOrder.pl, the queries are in their
#   taxonomy clade groups, ordered by distance to the target, with the
#   default on/off from that file, the same order on the description page.
#
# This script never writes anything into /gbdb/.  A subtrack is output
# only when its files already exist in /gbdb/<db>/chainNet/, files that
# a build produced but are not in /gbdb are listed on stderr.

use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use ChainNetInfo qw(accessionOrDb findAlignments readCladeFile);

my $methodOpt = "all";
my $cladeFile = "";
my $composite = "";
my $htmlName = "";
my $shortLabel = "";
my $longLabel = "";
my $group = "compGeno";
my $priority = "100.1";
my $optOk = GetOptions('method=s' => \$methodOpt,
                       'cladeFile=s' => \$cladeFile,
                       'composite=s' => \$composite,
                       'html=s' => \$htmlName,
                       'shortLabel=s' => \$shortLabel,
                       'longLabel=s' => \$longLabel,
                       'group=s' => \$group,
                       'priority=s' => \$priority);
my $argc = scalar(@ARGV);

if (! $optOk || $argc < 1 || length($composite) < 1
      || (length($cladeFile) < 1 && $argc < 2)
      || (length($cladeFile) && $argc > 1)
      || $methodOpt !~ m/^(all|lastz|miniMap2)$/) {
  printf STDERR "usage: chainNetCompositeTrackDb.pl -composite=<trackName> [options] <db> <query> [<query> ...] > composite.ra\n";
  printf STDERR "   or: chainNetCompositeTrackDb.pl -composite=<trackName> -cladeFile=<file> [options] <db> > composite.ra\n";
  printf STDERR "where db is a UCSC database (e.g. hg38),\n";
  printf STDERR "and   query is one or more UCSC database or GenArk assemblies\n";
  printf STDERR "      (e.g.: hs1 GCF_002880755.1)\n";
  printf STDERR "options:\n";
  printf STDERR "  -composite=<trackName> - required, name of the composite track\n";
  printf STDERR "  -cladeFile=<file> - the queries, their clades, order and default\n";
  printf STDERR "      on/off, from chainNetCladeOrder.pl, in place of the query list,\n";
  printf STDERR "      give the same file to chainNetDescription.pl\n";
  printf STDERR "  -html=<name> - description page for the composite, default: the\n";
  printf STDERR "      composite name, make it with: chainNetDescription.pl\n";
  printf STDERR "      using the same -method, db and query arguments\n";
  printf STDERR "  -method=lastz|miniMap2 - only the alignments made with this method,\n";
  printf STDERR "      default: all alignments found\n";
  printf STDERR "  -shortLabel=\"...\" -longLabel=\"...\" - composite labels\n";
  printf STDERR "  -group=<group> - default: $group\n";
  printf STDERR "  -priority=<N> - composite priority, default: $priority\n";
  printf STDERR "looks for the build directories in /hive/data/genomes/<db>/bed/:\n";
  printf STDERR "  lastz.<query>  blastz.<query>.swap  miniMap2.<query>  miniMap2.<query>.swap\n";
  printf STDERR "and for their files in /gbdb/<db>/chainNet/ - nothing is written there,\n";
  printf STDERR "files that are missing from /gbdb are listed on stderr.\n";
  printf STDERR "The trackDb .ra is written to stdout.\n";
  exit 255;
}

my $db = shift;
my @queryList = @ARGV;
$htmlName = $composite if (length($htmlName) < 1);

my %queryClade;	# key is qId, value is clade name from -cladeFile
my %queryChainOn;	# key is qId, value is 1 when its chain is on by default
my %queryNetOn;	# key is qId, value is 1 when its net is on by default
if (length($cladeFile)) {
  foreach my $entry (&readCladeFile($cladeFile)) {
    my $qId = $entry->{'query'};
    push @queryList, $qId;
    $queryClade{$qId} = $entry->{'clade'};
    $queryChainOn{$qId} = $entry->{'chainOn'};
    $queryNetOn{$qId} = $entry->{'netOn'};
  }
}

if ($db =~ m/^GC[AF]_/) {
  die "ERROR: '$db' is a GenArk assembly, use asmHubChainNetTrackDb.pl or asmHubMiniMap2ChainNetTrackDb.pl\n";
}
my $tRoot = "/hive/data/genomes/$db/bed";
die "ERROR: can not find build directory '$tRoot'\n" if (! -d "$tRoot");
my $gbdbDir = "/gbdb/$db/chainNet";

# single row hgsql query, list form open so nothing goes through a shell
sub hgsqlRow($$) {
  my ($sql, $database) = @_;
  open (my $fh, "-|", "hgsql", "-N", "-e", $sql, $database) or return ();
  my $line = <$fh>;
  close ($fh);
  return () if (! defined($line));
  chomp $line;
  return split('\t', $line);
}

# query labels: (assembly name, common name, long label prefix,
#                short label prefix, short label prefix as displayed)
#   a UCSC database query short and long labels use the $o_Organism
#   ($o_date) trackDb substitutions from its otherDb dbDb entry, a GenArk
#   query has them spelled out
sub queryLabels($) {
  my ($qId) = @_;
  if ($qId =~ m/^GC/) {
    my ($date, $asmName) = &HgAutomate::hubDateName($qId);
    $asmName =~ s/^_//;
    my ($comName) = &hgsqlRow("select commonName from genark where gcAccession=\"$qId\"", "hgcentraltest");
    $comName = $qId if (! defined($comName) || length($comName) < 1);
    $comName =~ s/\s\(.*//;
    $date = "" if (! defined($date) || $date eq "some date");
    my $short = length($asmName) ? $asmName : $qId;
    my $fullName = length($asmName) ? "${qId}_$asmName" : $qId;
    return ($short, $comName, "$comName ($date $fullName)", $comName, $comName);
  }
  my ($genome, $organism) = &hgsqlRow("select genome,organism from dbDb where name=\"$qId\"", "hgcentraltest");
  $genome = $qId if (! defined($genome) || length($genome) < 1);
  $organism = $genome if (! defined($organism) || length($organism) < 1);
  return ($qId, $genome, "\$o_Organism (\$o_date)", "\$o_Organism", $organism);
}

# subGroup title: no spaces or punctuation, the matrix shows _ as a space
sub subGroupTitle($) {
  my ($title) = @_;
  $title =~ s/[^A-Za-z0-9'_-]+/_/g;
  $title =~ s/_+/_/g;
  $title =~ s/^_//;
  $title =~ s/_$//;
  return $title;
}

##############################################################################
# the subtrack types.  minimap2 builds only have chain and lift over chains.
#   key, track/file name prefix, view, chainType tag, short label, long label
my @chainTypes = (
  [ "chain",    "chain",         "chain",    "c00", "Chain",   "Chained Alignments" ],
  [ "syntenic", "chainSyn",      "synten",   "c01", "synChain", "Syntenic Chained Alignments" ],
  [ "rBest",    "chainRBest",    "rbest",    "c02", "rbChain", "Reciprocal Best Chained Alignments" ],
  [ "liftOver", "chainLiftOver", "liftover", "c03", "loChain", "Lift Over Chained Alignments" ],
);
#   net name, chainType tag, short label, long label, view
my @netTypes = (
  [ "net",         "c04", "mafNet",      "Net Alignment", "net" ],
  [ "synNet",      "c05", "synNet",      "Syntenic Net Alignment", "synNet" ],
  [ "rbestNet",    "c06", "rbestNet",    "Reciprocal Best Net Alignment", "rbestNet" ],
  [ "liftOverNet", "c07", "liftOverNet", "Lift Over Net Alignment", "liftOverNet" ],
);
# subGroup1: one view for each type of chain and net
my %viewTitle = ( 'chain' => "Chains", 'synten' => "Syntenic_chains",
   'rbest' => "Reciprocal_best_chains", 'liftover' => "Lift_over_chains",
   'net' => "Nets", 'synNet' => "Syntenic_nets",
   'rbestNet' => "Reciprocal_best_nets", 'liftOverNet' => "Lift_over_nets" );
my @viewOrder = ( "chain", "synten", "rbest", "liftover",
                  "net", "synNet", "rbestNet", "liftOverNet" );
my %netView = map { $_->[4] => 1 } @netTypes;	# the net views
my %chainTypeTitle = ( 'c00' => "chain", 'c01' => "syntenic",
   'c02' => "reciprocal_best", 'c03' => "lift_over", 'c04' => "net",
   'c05' => "syntenic_net", 'c06' => "reciprocal_best_net",
   'c07' => "lift_over_net" );
my %methodTag = ( 'lastz' => "m0", 'lastzSwap' => "m1",
   'miniMap2' => "m2", 'miniMap2Swap' => "m3" );
my %methodTitle = ( 'm0' => "lastz", 'm1' => "lastz_swap",
   'm2' => "minimap2", 'm3' => "minimap2_swap" );

# an older lastz build may only have loaded the database tables, as
# chainNetStanza.pl falls back to: type chain <q> / type netAlign <q> chain<Q>
sub tableExists($) {
  my ($table) = @_;
  my ($found) = &hgsqlRow("show tables like \"$table\"", $db);
  return (defined($found) && $found eq $table) ? 1 : 0;
}

# first of the given /gbdb basenames (without .bb) that exists, or ""
sub gbdbFound(@) {
  my @names = @_;
  foreach my $name (@names) {
    return $name if (-s "$gbdbDir/$db.$name.bb");
  }
  return "";
}

my @subtracks;	# hash references, in output order
my @missing;	# [ /gbdb path, build directory source ]
my %speciesTag;	# key is qId, value is s000 tag
my %speciesTitle;	# key is s000 tag, value is title
my @speciesOrder;
my %queryPrio;	# key is qId, value is subtrack priority
my %queryShortLabel;	# key is qId, value is its short label prefix
my %queryShownName;	# key is qId, value is that prefix as displayed, lower case
my %queryAsmName;	# key is qId, value is db or GenArk assembly name
my %cladeTag;	# key is clade name, value is g00 tag
my %cladeTitle;	# key is g00 tag, value is title
my @cladeOrder;	# g00 tags in the order of the -cladeFile

foreach my $query (@queryList) {
  my $qId = &accessionOrDb($query);
  my $Q = ucfirst($qId);
  my @rows = &findAlignments($tRoot, $db, $query, $methodOpt);
  if (! scalar(@rows)) {
    printf STDERR "# warning: no %s alignment found for '%s' in %s\n",
       ($methodOpt eq "all") ? "lastz or miniMap2" : $methodOpt, $query, $tRoot;
    next;
  }
  my ($qShort, $comName, $qLong, $qShortLabel, $qShownName) = &queryLabels($qId);
  $queryShortLabel{$qId} = $qShortLabel;
  $queryShownName{$qId} = lc($qShownName);
  $queryAsmName{$qId} = $qShort;
  my $tag = sprintf("s%03d", scalar(@speciesOrder));
  # the common name and the assembly, several assemblies of one species
  # are told apart in the matrix
  my $title = (lc($comName) eq lc($qShort)) ? $qShort : "${comName}_$qShort";
  $title = &subGroupTitle($title);
  my $bestChain = 0;
  foreach my $row (@rows) {
    $bestChain = $row->{'chain'} if (length($row->{'chain'}) && $row->{'chain'} > $bestChain);
  }
  $queryPrio{$qId} = sprintf("%.3f", 100 - $bestChain);
  my $subtrackCount = 0;

  foreach my $row (@rows) {
    my $method = $row->{'method'};
    my $methodName = ($method eq "miniMap2") ? "MiniMap2" : "";
    # a swap gets the Swap suffix when the same method also has a primary
    # build for this query, otherwise it may have been installed either way
    my $havePrimary = scalar(grep { $_->{'method'} eq $method && ! $_->{'isSwap'} } @rows);
    my @swapSuffixes = ("");
    if ($row->{'isSwap'}) {
      @swapSuffixes = $havePrimary ? ("Swap") : ("", "Swap");
    }
    my $mTag = $methodTag{$method . ($row->{'isSwap'} ? "Swap" : "")};
    my $methodLabel = ($method eq "miniMap2") ? "mm2 " : "";
    $methodLabel .= "Swap " if ($row->{'isSwap'});
    my $longMethod = ($method eq "miniMap2") ? "minimap2 " : "lastz ";
    $longMethod = "Swapped from $qId $longMethod" if ($row->{'isSwap'});
    my $params = $row->{'params'};

    foreach my $type (@chainTypes) {
      my ($key, $prefix, $view, $cTag, $shortTag, $longTag) = @$type;
      next if ($method eq "miniMap2" && $key ne "chain" && $key ne "liftOver");
      my $found = "";
      my $trackName = "";
      foreach my $swapSuffix (@swapSuffixes) {
        my $base = "${prefix}${methodName}";
        $found = &gbdbFound("${base}${Q}${swapSuffix}", "${base}${qId}${swapSuffix}");
        if (length($found)) {
          $trackName = "${base}${Q}${swapSuffix}";
          last;
        }
      }
      if (! length($found) && $method eq "lastz" && $key eq "chain"
            && $swapSuffixes[0] eq "" && &tableExists("chain$Q")) {
        push @subtracks, {
          'track' => "chain$Q", 'view' => $view, 'species' => $tag,
          'chainType' => $cTag, 'method' => $mTag, 'qId' => $qId,
          'shortLabel' => "$qShortLabel $methodLabel$shortTag",
          'longLabel' => "$qLong $longMethod$longTag",
          'type' => "chain $qId",
          'otherDb' => $qId,
          'chainMinScore' => (defined($params) && length($params->{'minScore'})) ? $params->{'minScore'} : undef,
          'chainLinearGap' => (defined($params) && length($params->{'linearGap'})) ? $params->{'linearGap'} : undef,
        };
        ++$subtrackCount;
        next;
      }
      if (! length($found)) {
        # doBlastzChainNet.pl/doMiniMap2.pl put them in axtChain/<prefix><Q>.bb
        my $src = "$row->{'dir'}/axtChain/${prefix}${Q}.bb";
        if (-s "$src") {
          my $want = "${prefix}${methodName}${Q}$swapSuffixes[-1]";
          push @missing, [ "$gbdbDir/$db.$want.bb", $src ];
          push @missing, [ "$gbdbDir/$db.${want}Link.bb", "$row->{'dir'}/axtChain/${prefix}${Q}Link.bb" ];
        }
        next;
      }
      my %sub = (
        'track' => $trackName, 'view' => $view, 'species' => $tag,
        'chainType' => $cTag, 'method' => $mTag, 'qId' => $qId,
        'shortLabel' => "$qShortLabel $methodLabel$shortTag",
        'longLabel' => "$qLong $longMethod$longTag",
        'type' => "bigChain $qId",
        'bigDataUrl' => "/gbdb/\$D/chainNet/\$D.$found.bb",
        'linkDataUrl' => "/gbdb/\$D/chainNet/\$D.${found}Link.bb",
        'otherDb' => $qId,
      );
      if ($method eq "lastz" && defined($params)) {
        $sub{'chainMinScore'} = $params->{'minScore'} if (length($params->{'minScore'}));
        $sub{'chainLinearGap'} = $params->{'linearGap'} if (length($params->{'linearGap'}));
      }
      push @subtracks, \%sub;
      ++$subtrackCount;
    }

    next if ($method ne "lastz");	# nets only from lastz builds
    foreach my $net (@netTypes) {
      my ($netName, $cTag, $shortTag, $longTag, $netViewName) = @$net;
      my $found = "";
      my $swapSuffix = "";
      foreach my $suffix (@swapSuffixes) {
        $found = &gbdbFound("$qId.$netName$suffix");
        if (length($found)) {
          $swapSuffix = $suffix;
          last;
        }
      }
      if (! length($found) && $netName eq "net" && $swapSuffixes[0] eq ""
            && &tableExists("net$Q")) {
        push @subtracks, {
          'track' => "net$Q", 'view' => "net",
          'species' => $tag, 'chainType' => $cTag, 'method' => $mTag,
          'qId' => $qId,
          'shortLabel' => "$qShortLabel ${methodLabel}Net",
          'longLabel' => "$qLong $longMethod$longTag",
          'type' => "netAlign $qId chain$Q",
          'otherDb' => $qId,
        };
        ++$subtrackCount;
        next;
      }
      if (! length($found)) {
        foreach my $src ("$row->{'dir'}/axtChain/$qId.$netName.bb",
                         "$row->{'dir'}/bigMaf/$db.$qId.$netName.bb") {
          next if (! -s "$src");
          (my $srcSummary = $src) =~ s/\.bb$/.summary.bb/;
          my $want = "$qId.$netName$swapSuffixes[-1]";
          push @missing, [ "$gbdbDir/$db.$want.bb", $src ];
          push @missing, [ "$gbdbDir/$db.$want.summary.bb", $srcSummary ];
          last;
        }
        next;
      }
      push @subtracks, {
        'track' => "$netName$Q$swapSuffix", 'view' => $netViewName,
        'species' => $tag, 'chainType' => $cTag, 'method' => $mTag,
        'qId' => $qId,
        'shortLabel' => "$qShortLabel $methodLabel$shortTag",
        'longLabel' => "$qLong $longMethod$longTag",
        'type' => "bigMaf",
        'bigDataUrl' => "/gbdb/\$D/chainNet/\$D.$found.bb",
        'summary' => "/gbdb/\$D/chainNet/\$D.$found.summary.bb",
        'otherDb' => $qId,
        'speciesOrder' => $qId,
        'speciesLabels' => "$qId=\"$comName\"",
      };
      ++$subtrackCount;
    }
  }
  if ($subtrackCount) {
    $speciesTag{$qId} = $tag;
    $speciesTitle{$tag} = $title;
    push @speciesOrder, $tag;
    if (length($cladeFile)) {
      # the -cladeFile order is the subtrack order
      $queryPrio{$qId} = scalar(@speciesOrder);
      my $clade = $queryClade{$qId};
      if (! defined($cladeTag{$clade})) {
        my $cTag = sprintf("g%02d", scalar(@cladeOrder));
        $cladeTag{$clade} = $cTag;
        $cladeTitle{$cTag} = &subGroupTitle($clade);
        push @cladeOrder, $cTag;
      }
    }
  } else {
    printf STDERR "# warning: alignments for '%s' found, but none of their files are in %s\n", $query, $gbdbDir;
  }
}

if (scalar(@missing)) {
  printf STDERR "# files a build produced that are not in %s,\n", $gbdbDir;
  printf STDERR "# their subtracks are not in the output:\n";
  foreach my $miss (@missing) {
    my ($gbdbPath, $src) = @$miss;
    printf STDERR "#   ln -s %s \\\n#      %s\n", $src, $gbdbPath;
  }
}

die "ERROR: no subtracks, none of the alignment files are in $gbdbDir\n"
  if (scalar(@subtracks) < 1);

# queries whose short labels would read the same, e.g. two Chimp
# assemblies, get their db or assembly name after the organism name:
#   $o_Organism panTro6 Chain
{
  my %nameQueries;	# key is displayed name, value is count of queries
  foreach my $qId (keys %speciesTag) {
    $nameQueries{$queryShownName{$qId}} += 1;
  }
  foreach my $sub (@subtracks) {
    my $qId = $sub->{'qId'};
    next if ($nameQueries{$queryShownName{$qId}} < 2);
    my $prefix = $queryShortLabel{$qId};
    $sub->{'shortLabel'} =~ s/^\Q$prefix\E /$prefix $queryAsmName{$qId} /;
  }
}

##############################################################################
# track names already defined in this database's trackDb, outside this
# composite, would collide with the names output here
{
  my @names = map { $_->{'track'} } @subtracks;
  my $nameList = join(",", map { "\"$_\"" } @names);
  my $sql = "select tableName from trackDb where tableName in ($nameList) and settings not like \"%$composite%\"";
  if (open (my $fh, "-|", "hgsql", "-N", "-e", $sql, $db)) {
    while (my $name = <$fh>) {
      chomp $name;
      printf STDERR "# WARNING: track name '%s' is already defined in %s trackDb outside of %s\n", $name, $db, $composite;
    }
    close ($fh);
  } else {
    printf STDERR "# warning: can not check %s trackDb for track name collisions\n", $db;
  }
}

##############################################################################
# output
my %viewUsed;
my %chainTypeUsed;
my %methodUsed;
foreach my $sub (@subtracks) {
  $viewUsed{$sub->{'view'}} = 1;
  $chainTypeUsed{$sub->{'chainType'}} = 1;
  $methodUsed{$sub->{'method'}} = 1;
}

if (length($shortLabel) < 1) {
  $shortLabel = "Chain/Net";
  $shortLabel = "minimap2 Chain/Net" if ($methodOpt eq "miniMap2");
}
if (length($longLabel) < 1) {
  $longLabel = "Chain and Net Alignments to other assemblies";
  $longLabel = "minimap2 Chain and Lift Over Alignments to other assemblies"
     if ($methodOpt eq "miniMap2");
}

# default on/off: with a -cladeFile, its chainOn turns on the query's plain
# chain, netOn turns on its net, or its plain chain when it has no net
# (a minimap2 build).  Without one, only the first subtrack is on.
if (length($cladeFile)) {
  foreach my $qId (keys %speciesTag) {
    my @mine = grep { $_->{'qId'} eq $qId } @subtracks;
    my ($chain) = grep { $_->{'chainType'} eq "c00" } @mine;
    my ($net) = grep { $_->{'chainType'} eq "c04" } @mine;
    $chain->{'on'} = 1 if ($queryChainOn{$qId} && defined($chain));
    if ($queryNetOn{$qId}) {
      if (defined($net)) {
        $net->{'on'} = 1;
      } elsif (defined($chain)) {
        $chain->{'on'} = 1;
      }
    }
  }
} else {
  my ($firstView) = grep { $viewUsed{$_} } @viewOrder;
  my ($firstSub) = grep { $_->{'view'} eq $firstView } @subtracks;
  $firstSub->{'on'} = 1;
}

# without a -cladeFile, every query is in one clade, the matrix has one column
if (! scalar(@cladeOrder)) {
  $cladeTag{"all"} = "g00";
  $cladeTitle{"g00"} = "All";
  push @cladeOrder, "g00";
  $queryClade{$_} = "all" foreach (keys %speciesTag);
}

# subGroup1 the views, one for each type of chain and net, subGroup2 the
# species, subGroup3 the clades, the matrix: clade x species, the same
# arrangement as the older chainNetComposite.pl composites
my @subGroups = (
  "view Views " . join(" ", map { "$_=$viewTitle{$_}" } grep { $viewUsed{$_} } @viewOrder),
  "species Species " . join(" ", map { "$_=$speciesTitle{$_}" } @speciesOrder),
  "clade Clade " . join(" ", map { "$_=$cladeTitle{$_}" } @cladeOrder),
);
my @dimensions = ("dimensionX=clade", "dimensionY=species");
my @sortOrder = ("species=+", "view=+", "clade=+");

printf "##############################################################################\n";
printf "# %s - made by: chainNetCompositeTrackDb.pl -composite=%s -method=%s%s %s %s\n",
   $composite, $composite, $methodOpt,
   length($cladeFile) ? " -cladeFile=$cladeFile" : "", $db,
   length($cladeFile) ? "" : join(" ", @queryList);
printf "##############################################################################\n";
printf "track %s\n", $composite;
printf "compositeTrack on\n";
printf "shortLabel %s\n", $shortLabel;
printf "longLabel %s\n", $longLabel;
for (my $i = 0; $i < scalar(@subGroups); ++$i) {
  printf "subGroup%d %s\n", $i + 1, $subGroups[$i];
}
printf "dragAndDrop subTracks\n";
printf "visibility hide\n";
printf "group %s\n", $group;
printf "noInherit on\n";
printf "priority %s\n", $priority;
printf "color 0,0,0\n";
printf "altColor 100,50,0\n";
printf "type bed 3\n";
printf "dimensions %s\n", join(" ", @dimensions);
printf "sortOrder %s\n", join(" ", @sortOrder);
# the most common lastz axtChain settings on the composite too, as the
# older composites had them, a description page may refer to them as
# $chainMinScore and $chainLinearGap.  With noInherit on, each subtrack
# keeps its own.
foreach my $setting ('chainMinScore', 'chainLinearGap') {
  my %valueCount;
  foreach my $sub (@subtracks) {
    $valueCount{$sub->{$setting}} += 1 if (defined($sub->{$setting}));
  }
  next if (! scalar(keys %valueCount));
  my ($common) = sort { $valueCount{$b} <=> $valueCount{$a} || $a cmp $b } keys %valueCount;
  printf "%s %s\n", $setting, $common;
}
printf "configurable on\n";
printf "html %s\n", $htmlName;

foreach my $view (@viewOrder) {
  next if (! $viewUsed{$view});
  my $viewTrack = "${composite}View$view";
  my $viewVis = "hide";
  if ($view eq "chain") {
    $viewVis = "pack";
  } elsif ($netView{$view} && scalar(grep { $_->{'view'} eq $view && $_->{'on'} } @subtracks)) {
    $viewVis = "dense";
  }
  printf "\n    track %s\n", $viewTrack;
  printf "    shortLabel %s\n", $viewTitle{$view} =~ s/_/ /gr;
  printf "    view %s\n", $view;
  printf "    visibility %s\n", $viewVis;
  printf "    parent %s\n", $composite;
  printf "    spectrum on\n" if (! $netView{$view});
  foreach my $sub (grep { $_->{'view'} eq $view } @subtracks) {
    printf "\n        track %s\n", $sub->{'track'};
    printf "        parent %s %s\n", $viewTrack, $sub->{'on'} ? "on" : "off";
    printf "        subGroups view=%s species=%s clade=%s\n",
       $view, $sub->{'species'}, $cladeTag{$queryClade{$sub->{'qId'}}};
    printf "        shortLabel %s\n", $sub->{'shortLabel'};
    printf "        longLabel %s\n", $sub->{'longLabel'};
    printf "        type %s\n", $sub->{'type'};
    foreach my $setting ('bigDataUrl', 'linkDataUrl', 'summary', 'otherDb', 'speciesOrder',
                         'speciesLabels', 'chainMinScore', 'chainLinearGap') {
      printf "        %s %s\n", $setting, $sub->{$setting} if (defined($sub->{$setting}));
    }
    printf "        priority %s\n", $queryPrio{$sub->{'qId'}};
  }
}
printf "\n";
