#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/makePushQSql.pl instead.

# $Id: makePushQSql.pl,v 1.28 2010/01/09 00:07:01 hiram Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;

# Option variable names:
use vars @HgAutomate::commonOptionVars;
use vars qw/
  $opt_noGenbank
  $opt_redmineList
  /;

# Option defaults:
my $dbHost = 'hgwdev';
my $workhorse = 'n/a';

sub usage {
  # Usage / help / self-documentation:
  my ($status) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db
options:
";
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost);
  print STDERR "    -noGenbank		  Add this if db does not have GenBank tables.\n";
  print STDERR "    -redmineList	  write list files to redmine.db.file.list\n";
  print STDERR "    			  and redmine.db.table.list\n";
  print STDERR "    			  and redmine.db.releaseLog.txt\n";
  print STDERR "
Prints (to stdout) SQL commands for creation of a new push queue for db
and the addition of an Initial Release entry in the main push queue.
These commands probably should be redirected to a file which is then reviewed
and edited before executing the SQL on the push queue host.  Ask QA for
push queue guidance when in doubt.

";
  exit $status;
} # usage

# Globals:
my ($db);
my ($sql, $prefixPattern, @wigTables, @netODbs);
my ($redmineFileList, $redmineTableList, $redmineReleaseLog);
my %noPush = ( 'bacEndPairsBad' => 1,
	       'bacEndPairsLong' => 1,
	       'genscanSubopt' => 1,
	     );

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgAutomate::commonOptionSpec,
		      'noGenbank',
		      'redmineList',
		     );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  $dbHost = $opt_dbHost if ($opt_dbHost);
} # checkOptions

# hash of hashes, chromInfoDb key is db, hash ref key is chrom name
my %chromInfoDb = ();

sub isChrom($$) {
  # Return true if $str is in chromInfo.chrom.
  my ($str, $localDb) = @_;
  my $localSql = "$HgAutomate::runSSH $dbHost hgsql -N $localDb";
  my %localHash = ();
  my $hashRef = \%localHash;
  if (exists($chromInfoDb{$localDb})) {
    $hashRef = $chromInfoDb{$localDb};
  } else {
    foreach my $chr (`echo select chrom from chromInfo | $localSql`) {
      chomp $chr;
      $hashRef->{$chr} = 1;
    }
    $chromInfoDb{$localDb} = $hashRef;
  }
  return (defined $hashRef->{$str});
} # isChrom


sub getAllTables($) {
  # Return a hash for testing the existence of all tables in given db argument.
  # Well, almost all -- ignore per-user trackDb.ra-derived tables, the
  # cron-generated tableDescriptions, and tables that we never push.
  # And collapse split tables.
  my ($localDb) = @_;
  my $localSql = "$HgAutomate::runSSH $dbHost hgsql -N $localDb";
  my %tables = ();
  foreach my $t (`echo show tables | $localSql`) {
    chomp $t;
    next if ($t =~ /^(trackDb|hgFindSpec)_\w+/);
    next if (defined $noPush{$t});
    if ($t =~ /^(\S+)_(\w+)$/) {
      my ($maybeChr, $track) = ($1, $2);
      if (&isChrom($maybeChr, $localDb)) {
	my $prefix;
	if ($maybeChr =~ /^chr/) {
	  $prefix = 'chr*_';
	  $prefixPattern = 'chr\*_';
	} elsif ($maybeChr =~ /^2micron/) {
	  $prefix = '2micron_';
	  $prefixPattern = '2micron_';
	} else {
	  die "\nSorry, I don't know what prefix (usually chr*_) to use for " .
	    $maybeChr;
	}
	$t = "$prefix$track";
      }
    }
    $tables{$t} = 1;
  }
  return \%tables;
} # getAllTables


sub dbIsChromBased {
  # Good candidate for libification, though a little hacky.
  # Would be more proper to use the db.
  my $seqCount = `wc -l < /cluster/data/$db/chrom.sizes` + 0;
  return ($seqCount <= $HgAutomate::splitThreshold);
} # dbIsChromBased


sub getInfrastructureEntry {
  # Make an entry hash structure for tables that are not associated with a
  # particular track -- genome browser infrastructure.
  my ($allTables) = @_;
  my @leftovers = ();
  my %entry = ();
  $entry{'shortLabel'} = "supporting tables and files";
  $entry{'priority'} = 0;

  # Look for the usual set of files on $dbHost:
  my $SameSpecies = ucfirst($db);  $SameSpecies =~ s/\d+$//;
  my $sameSpecies = $db;  $sameSpecies =~ s/\d+$//;
  my $Db = ucfirst($db);
  # 2017-03-02 - remove gc5Base.wib quality.wib quality.bw
  #              rarely, if ever, used any more
  my @gbdbFiles = map {"$HgAutomate::gbdb/$db/$_"}
    ("$db.2bit", 'html/description.html', "bbi/gc5BaseBw/gc5Base.bw", "ncbiRefSeq/ncbiRefSeqVersion.txt", "ncbiRefSeq/ncbiRefSeqOther.bb", "ncbiRefSeq/ncbiRefSeqOther.ix", "ncbiRefSeq/ncbiRefSeqOther.ixx", "ncbiRefSeq/seqNcbiRefSeq.rna.fa", "liftOver/${db}To$SameSpecies*");

  my @otherGbdbFiles = map {"$HgAutomate::gbdb/${sameSpecies}*/$_"}
    ("liftOver/${sameSpecies}*To$Db.over.chain.gz");

  my @goldenPathFiles = map {"$HgAutomate::goldenPath/$db/$_"}
    (qw( bigZips/* database/* chromosomes/* ),
     "liftOver/${db}To$SameSpecies*");

  my @otherGoldenPathFiles = map {"$HgAutomate::goldenPath/${sameSpecies}*/$_"}
    ("liftOver/${sameSpecies}*To$Db.over.chain.gz");

  my $sciUnderscore = &HgAutomate::getSpecies($dbHost, $db);
  $sciUnderscore =~ s/ /_/g;
  my $gatewayPhoto = "$HgAutomate::images/$sciUnderscore.jpg";
  if (! &HgAutomate::machineHasFile($dbHost, $gatewayPhoto)) {
     $gatewayPhoto = "$HgAutomate::images/$sciUnderscore.gif";
  }
  my @files = ();
  foreach my $f (@gbdbFiles, @otherGbdbFiles, @goldenPathFiles, @otherGoldenPathFiles, $gatewayPhoto) {
    if (&HgAutomate::machineHasFile($dbHost, $f)) {
      push @files, $f;
    } else {
      &HgAutomate::verbose(1, "WARNING: $dbHost does not have $f\n")
	unless ($f =~ /${db}To$SameSpecies/ ||
		($f =~ m@/chromosomes/@ && !&dbIsChromBased()));
    }
  }
  $entry{'files'} = join('\r\n', @files);
  $entry{'redmineFiles'} = join("\n", @files);

  # Look for infrastructure tables in allTables hash:
  foreach my $t (qw( chromAlias chromInfo grp seq extFile hgFindSpec trackDb history
		    tableDescriptions )) {
    if (defined $allTables->{$t}) {
      $entry{'tables'} .= "$t ";
      $entry{'redmineTables'} .= "$db.$t ";
      delete $allTables->{$t};
      &HgAutomate::verbose(3, "Deleted $t\n");
    } else {
      &HgAutomate::verbose(1, "WARNING: $db does not have $t\n");
    }
  }
  return (\%entry);
} # getInfrastructureEntry


sub getGenbankEntry {
  # Return an entry hash ref for genbank tables.
  my ($allTables) = @_;
  # Note: if any tables are added to or removed from the genbank process,
  # then these lists will have to be updated.  But hopefully warning messages
  # to the user will help diagnose.  Mark has been maintaining a list of
  # regexes in kent/src/hg/makeDb/genbank/etc/genbank.tbls .
  # /cluster/data/genbank/etc/gbMetadataTables.txt - tables in hgFixed
  # /cluster/data/genbank/etc/gbPerAssemblyTables.txt - tables in a db
  my @genbankTrackTables = qw(
    all_est all_mrna ccdsGene ccdsInfo ccdsKgMap ccdsNotes
    chr*_est chr*_intronEst chr*_mrna estOrientInfo gbMiscDiff gbWarn
    intronEst mgcFailedEst mgcFullMrna mgcFullStatus mgcGenes
    mgcIncompleteMrna mgcPickedEst mgcStatus
    mgcUnpickedEst mrnaOrientInfo orfeomeGenes orfeomeMrna
    refFlat refGene refLink refSeqAli
    xenoEst xenoMrna xenoRefFlat xenoRefGene xenoRefSeqAli
    );
  my @genbankTablesInDb = ();
  my @redmineGenbankTablesInDb = ();
  foreach my $t (@genbankTrackTables) {
    if (defined $allTables->{$t}) {
      push @genbankTablesInDb, $t;
      push @redmineGenbankTablesInDb, "$db.$t";
      delete $allTables->{$t};
      &HgAutomate::verbose(3, "Deleted $t\n");
    }
  }
  if (scalar(@genbankTablesInDb) < 0) {
    &HgAutomate::verbose(1, "WARNING: $db does not have any genbank tables\n");
  }
  my %entry = ();
  $entry{'shortLabel'} = 'Genbank-process tracks and supporting tables';
  $entry{'priority'} = 1;
  $entry{'tables'} = join(' ', @genbankTablesInDb);
  $entry{'redmineTables'} = join(" ", @redmineGenbankTablesInDb);
  $entry{'files'} = '';
  $entry{'redmineFiles'} = '';
  return \%entry;
} # getGenbankEntry


sub getTrackDb {
  # Return reference of hash to just the trackDb info that we need:
  # tableName => [ type, shortLabel, priority, otherTables ]
  # where otherTables is distilled from settings such as type wigMaf's
  # wiggle setting.
  my %trackDb = ();
  my $pipe = "echo select tableName, type, shortLabel, priority " .
    "from trackDb | $sql |";
  open(P, $pipe)
    || die "Couldn't open pipe ($pipe): $!\n";
  while (<P>) {
    chomp;
    my ($tableName, $type, $shortLabel, $priority) = split("\t");
    my $otherTables = "";
    my $settingsQuery = "select settings from trackDb " .
			    "where tableName = \\'$tableName\\'";
    my $settings = `echo $settingsQuery | $sql`;
# I don't know why this is, but the settings string comes back with
# the newlines as explicit literal backslash and n instead of a real
# newline \n character.  This sed will turn those two characters
#	'\' and 'n' into the real \n newline character.
    $settings =~ s#\\n#\n#g;
# we are not using this settingHash yet, but may need it some day
    my %settingHash;
# now we can split the settings on newlines:
    my @setNameValue = split('\n',$settings);

    &HgAutomate::verbose(2, "settings for tableName '$tableName', type='$type'\n") if (scalar(@setNameValue) > 0);
    for (my $i = 0; $i < scalar(@setNameValue); ++$i) {
	# first space separates the tag and its value
	my ($tag, $value) = split('\s', $setNameValue[$i], 2);
	$settingHash{$tag} = $value;
	&HgAutomate::verbose(2, "$i: $tag='$value'\n");
    }
    if ($type =~ /^psl/) {
      if ($settings =~ /pepTable\s+(\w+)/) {
	$otherTables .= " $1";
      }
      if ($settings =~ /baseColorUseSequence\s+(\w+)\s+(\w+)\s+(\w+)/) {
	$otherTables .= " $2";
	$otherTables .= " $3";
      }
    }
    if ($type =~ /^wigMaf/) {
      if ($settings =~ /wiggle\s+(\w+)/) {
	$otherTables .= " $1";
	push @wigTables, $1;
      }
      if ($settings =~ /frames\s+(\w+)/) {
	$otherTables .= " $1";
      }
      if ($settings =~ /summary\s+(\w+)/) {
	$otherTables .= " $1";
      }
    }
    $trackDb{$tableName} = [ $type, $shortLabel, $priority, $otherTables ];
  }
  close(P);
  return \%trackDb;
} # getTrackDb


sub substituteVars {
  my ($shortLabel, $track, $type) = @_;
  # Substitute variables in the labels if necessary.
  if ($shortLabel =~ /\$[Oo]rganism/) {
    my $Organism = &HgAutomate::getAssemblyInfo($dbHost, $db);
    my $organism = lc($Organism);
    $shortLabel =~ s/\$Organism/$Organism/g;
    $shortLabel =~ s/\$organism/$organism/g;
  }
  if ($shortLabel =~ /\$o_/) {
    my $oDb;
    if ($type =~ /^chain\s+(\w+)/ || $type =~ /^netAlign\s+(\w+)/) {
      $oDb = $1;
    } else {
      die "\nPlease fix me so I can figure out what \$o_db is " .
	"for $track (type $type)... ";
    }
    my ($oO, $oDate) = &HgAutomate::getAssemblyInfo($dbHost, $oDb);
    my $oo = lc($oO);
    $shortLabel =~ s/\$o_Organism/$oO/g;
    $shortLabel =~ s/\$o_organism/$oo/g;
    $shortLabel =~ s/\$o_date/$oDate/g;
    # This is a little bit tweaky but I think it is worth the trouble.
    # hgwdev often has "release alpha" chain/net shortLabels that use $o_db
    # but on hgwbeta, "release beta" shortLabels that use $o_Organism.
    # That looks better and I expect that $o_Organism will generate fewer
    # questions from QA about pushQ entry titles... so
    # instead of   $shortLabel =~ s/\$o_db/$oDb/g; --
    $shortLabel =~ s/\$o_db/$oO/g;
  }
  if ($shortLabel =~ /\$(\w+)/) {
    warn "Don't know how to substitute \$$1 in $track shortLabel " .
      "\"$shortLabel\"\n";
  }
  return ($shortLabel);
} # substituteVars


sub getTrackEntries {
  # Use heuristics and all.joiner info to partition the set of tables
  # into a set of track entry hash structures.  Augment with info from
  # trackDb, extFile and wiggle tables to identify labels and files
  # for tracks.  Remove accounted-for tables from $allTables and return
  # a list of entry hash refs.
  my ($allTables) = @_;
  my $trackDb = &getTrackDb();
  my %trackEntries = ();
  # For each table, if it is a track table then make an entry for it and
  # remove it from $allTables.
  foreach my $table (sort keys %{$allTables}) {
    next if (! defined $allTables->{$table}); # catch prior deletions
    my $track = $table;
    $track =~ s/^$prefixPattern// if ($prefixPattern);
    my $tdb = $trackDb->{$track};
    if (defined $tdb) {
      # This table is a track table -- add an entry.
      my ($type, $shortLabel, $priority, $otherTables) = @{$tdb};
      $shortLabel = &substituteVars($shortLabel, $track, $type);
      my %entry = ();
      $entry{'shortLabel'} = $shortLabel;
      $entry{'priority'} = $priority;
      $entry{'tables'} = $table . $otherTables;
      $entry{'redmineTables'} = "$db.$table";
      if (length($otherTables)) {
          $otherTables =~ s/^ +//;
          my @tbls = split('\s+', $otherTables);
          foreach my $table (@tbls) {
            $entry{'redmineTables'} .= " $db.$table";
          }
      }
      $entry{'files'} = "";
      $entry{'redmineFiles'} = "";
      if ($type =~ /^chain ?/) {
	$entry{'tables'} .= " ${table}Link";
	$entry{'redmineTables'} .= " $db.${table}Link";
	my $net = $table;
	$net =~ s/^.*chain/net/;
	# Lump in nets with chains, when we find them.
	if (defined $allTables->{$net}) {
	  &HgAutomate::verbose(2, "Lumping $net in with $table\n");
	  $entry{'tables'} .= " $net";
	  $entry{'redmineTables'} .= " $db.$net";
	  $entry{'shortLabel'} .= " and Net";
	  if ($net =~ /^net(\w+)/) {
	    my $ODb = $1;
	    my $over = "${db}To$ODb.over.chain.gz";
	    foreach my $downloads
	      ("$HgAutomate::goldenPath/$db/vs$ODb/*.txt",
	       "$HgAutomate::goldenPath/$db/vs$ODb/*.gz",
	       "$HgAutomate::goldenPath/$db/vs$ODb/reciprocalBest/*.gz",
	       "$HgAutomate::goldenPath/$db/vs$ODb/reciprocalBest/*.txt",
	      "$HgAutomate::goldenPath/$db/vs$ODb/reciprocalBest/axtRBestNet/*",
	       "$HgAutomate::goldenPath/$db/liftOver/$over",
	       "$HgAutomate::gbdb/$db/liftOver/$over") {
	      if (&HgAutomate::machineHasFile($dbHost, $downloads)) {
		$entry{'files'} .= $downloads . '\r\n';
		$entry{'redmineFiles'} .= $downloads . "\n";
	      } else {
		&HgAutomate::verbose(0, "WARNING: $dbHost does not have " .
				     "chain/net download $downloads !\n");
	      }
	    }
	    my $oDb = $ODb;  $oDb =~ s/^(\w)/\l$1/;
	    push @netODbs, $oDb;
	  }
	}
      } elsif ($type =~ /^wig\s/) {
	push @wigTables, $table;
      } elsif ($type =~ /^wigMaf/) {
	if ($table =~ /^multiz(\d+way)/) {
	  my $gif = "$HgAutomate::images/phylo/${db}_$1.gif";
	  my $png = "$HgAutomate::images/phylo/${db}_$1.png";
	  if (&HgAutomate::machineHasFile($dbHost, $gif)) {
	    $entry{'files'} .= $gif . '\r\n';
	    $entry{'redmineFiles'} .= $gif . "\n";
	  } elsif (&HgAutomate::machineHasFile($dbHost, $png)) {
	    $entry{'files'} .= $png . '\r\n';
	    $entry{'redmineFiles'} .= $png . "\n";
	  } else {
	    &HgAutomate::verbose(0, "WARNING: $dbHost does not have phyloPng-" .
				    "generated $gif (or png) for $table.\n");
	  }
	}
      }
      # We could get really fancy and look at wiggle and maf tables
      # (and extFile) to hunt down filenames.  But there may still be
      # other tracks (e.g. psl, linkedFeatures) with files in extFile...
      # it gets messy, so for now we'll settle for reminding the developer
      # to determine files associated with wiggle tables and to look in
      # extFile.
      elsif ($type =~ /^netAlign\s+(\w+)/) {
	my $oDb = $1;
	my $ODb = ucfirst($oDb);
	my $chainTrack = "chain$ODb";
	if ($prefixPattern) {
	  my $unEscPrefix = $prefixPattern;
	  $unEscPrefix =~ s/\\//g;
	  $chainTrack = "${unEscPrefix}chain$ODb";
	}
	if (! defined $trackEntries{$chainTrack}) {
	  my $downloads = "$HgAutomate::goldenPath/$db/vs$ODb/*";
	  if (&HgAutomate::machineHasFile($dbHost, $downloads)) {
	    $entry{'files'} = $downloads;
	    $entry{'redmineFiles'} = $downloads;
	  } else {
	    &HgAutomate::verbose(1, "WARNING: $dbHost does not have $downloads\n");
	  }
	  &HgAutomate::verbose(1, "WARNING: Found net table $table that was not " .
		       "already lumped in with chain entry $chainTrack...?\n");
	} else {
	  # This net has already been included in the corresponding Chain
	  # track entry, and removed from the hash, so skip to the next table.
	  next;
	}
      }
      # Remove accounted-for tables.
      foreach my $t (split(" ", $entry{'tables'})) {
	delete $allTables->{$t};
      }
      &HgAutomate::verbose(3, "Deleted $entry{'tables'}\n");
      $trackEntries{$track} = \%entry;
    }
  }
  # Now cycle through the leftovers still in $allTables and see if all.joiner
  # can help resolve where it belongs.  If there are multiple matches then
  # this will just take the first that is a track, which might not be always
  # correct but QA and the developer can sort it out.
  foreach my $table (keys %{$allTables}) {
    my $tbase = $table;
    $tbase =~ s/^$prefixPattern// if ($prefixPattern);
    my $allDotJoiner = "$ENV{HOME}/kent/src/hg/makeDb/schema/all.joiner";
    my $pipe = "joinableFields $allDotJoiner $db $tbase |";
    open(P, $pipe)
      || die "Couldn't open pipe ($pipe): $!\n";
    while (<P>) {
      my (undef, undef, undef, $otherDb, $otherTrack, undef) = split("\t");
      if ($otherDb && $otherDb eq $db && defined $trackEntries{$otherTrack}) {
	$trackEntries{$otherTrack}->{'tables'} .= " $table";
	$trackEntries{$otherTrack}->{'redmineTables'} .= " $otherDb.$table";
	delete $allTables->{$table};
&HgAutomate::verbose(3, "Deleted $table\n");
	last;
      }
    }
    close(P);
  }
  my @entries = sort { $a->{'priority'} <=> $b->{'priority'} }
		  values %trackEntries;
  return \@entries;
} # getTrackEntries

my @expectedTables = qw( augustusGene chromAlias chromInfo cpgIslandExt
cpgIslandExtUnmasked cytoBandIdeo gap gc5BaseBw genscan gold grp hgFindSpec
microsat nestedRepeats rmsk simpleRepeat tableDescriptions trackDb
ucscToINSDC ucscToRefSeq windowmaskerSdust );

# verify the standard set of tables exist
sub verifyExpectedTables {
  my ($allTables) = @_;
  foreach my $requiredTable (@expectedTables) {
    if (! exists($allTables->{$requiredTable})) {
      printf STDERR "WARNING: %s does not have %s\n", $db, $requiredTable;
    }
  }
}

sub getEntries {
  # Get the set of tables in $db.  Process that into a set of push
  # queue entry hash structures and remove tables that have been
  # accounted for from the set of all tables.  Return references to a
  # list of entry hash refs and to a hash of tables that could not be
  # accounted for.
  my @entries = ();
  my $allTables = &getAllTables($db);

  &verifyExpectedTables($allTables);

  push @entries, &getInfrastructureEntry($allTables);
  push @entries, &getGenbankEntry($allTables) unless $opt_noGenbank;
  push @entries, @{&getTrackEntries($allTables)};

  return (\@entries, $allTables);
} # getEntries


sub printHeader {
  # Print out the push queue table creation statement.
  print <<_EOF_
--
-- New push queue for $db
--

CREATE TABLE $db (
  qid varchar(6) NOT NULL default '',
  pqid varchar(6) NOT NULL default '',
  priority char(1) NOT NULL default '',
  rank int(10) unsigned NOT NULL default '0',
  qadate varchar(10) NOT NULL default '',
  newYN char(1) NOT NULL default '',
  track varchar(255) NOT NULL default '',
  dbs varchar(255) NOT NULL default '',
  tbls longblob NOT NULL,
  cgis varchar(255) NOT NULL default '',
  files longblob NOT NULL,
  sizeMB int(10) unsigned NOT NULL default '0',
  currLoc varchar(20) NOT NULL default '',
  makeDocYN char(1) NOT NULL default '',
  onlineHelp varchar(50) NOT NULL default '',
  ndxYN char(1) NOT NULL default '',
  joinerYN char(1) NOT NULL default '',
  stat varchar(255) NOT NULL default '',
  sponsor varchar(50) NOT NULL default '',
  reviewer varchar(50) NOT NULL default '',
  extSource varchar(128) NOT NULL default '',
  openIssues longblob NOT NULL,
  notes longblob NOT NULL,
  pushState char(1) NOT NULL default '',
  initdate varchar(10) NOT NULL default '',
  lastdate varchar(10) NOT NULL default '',
  bounces int(10) unsigned NOT NULL default '0',
  lockUser varchar(8) NOT NULL default '',
  lockDateTime varchar(16) NOT NULL default '',
  releaseLog longblob NOT NULL,
  featureBits longblob NOT NULL,
  releaseLogUrl longblob NOT NULL,
  importance char(1) NOT NULL default '',
  PRIMARY KEY  (`qid`)
) ENGINE=MyISAM;

_EOF_
  ;
} # printHeader


sub printEntry($$$$) {
  # Print out a single push queue entry (row of new table).
  my ($entry, $id, $localDb, $releaseLog) = @_;
  my $idStr = sprintf "%06d", $id;
  my $date = `date +%Y-%m-%d`;
  my $rank = $id;
  my $size = 0;  # User will have to use qaPushq to update for now.
  chomp $date;
  print <<_EOF_
INSERT INTO $db VALUES ('$idStr','','A',$rank,'$date','Y','$entry->{shortLabel}','$localDb','$entry->{tables}','','$entry->{files}',$size,'$dbHost','N','','N','N','','$ENV{USER}','','','','','N','$date','',0,'','','$releaseLog','','','');
_EOF_
  ;
  if ($opt_redmineList) {
     printf $redmineTableList "%s\n", $entry->{redmineTables} if (length($entry->{redmineTables}));
     printf $redmineFileList "%s\n", $entry->{redmineFiles} if (length($entry->{redmineFiles}));
     printf $redmineReleaseLog "%s\n", $entry->{shortLabel} if (length($entry->{shortLabel}));
  }
} # printEntry


sub printSwaps($) {
# print out entries in other DBs for chain/net swapped tracks
  my ($id) = @_;
  my $Db = ucfirst($db);
  my $checkChain = "chain$Db";
  my $checkNet = "net$Db";
  my ($oO) = &HgAutomate::getAssemblyInfo($dbHost, $db);
  foreach my $oDb (@netODbs) {
    my %entry = ();
    $entry{'shortLabel'} = "$oO Chain/Net";
    $entry{'priority'} = 1;
    my $tableList = "";
    my $redmineTables = "";
    my $dbTables = &getAllTables($oDb);
    foreach my $table (sort keys %{$dbTables}) {
	if ($table =~ m/$checkChain/ || $table =~ m/$checkNet/) {
	    $tableList .= "$table ";
	    $redmineTables .= "$oDb.$table ";
	}
    }
    $tableList =~ s/ +$//;
    $entry{'tables'} = $tableList;
    $entry{'redmineTables'} = $redmineTables;
    $entry{'files'} = "";
    $entry{'redmineFiles'} = "";
    my $over = "${oDb}To$Db.over.chain.gz";
    my $axtNet = "$HgAutomate::goldenPath/$oDb/vs$Db/axtNet/*";
    if (! &HgAutomate::machineHasFile($dbHost, $axtNet)) {
	$axtNet = "$HgAutomate::goldenPath/$oDb/vs$Db/$oDb.$db.net.axt.gz";
    }
    foreach my $downloads
      ("$HgAutomate::goldenPath/$oDb/vs$Db/*.txt",
       "$HgAutomate::goldenPath/$oDb/vs$Db/*.gz",
       $axtNet,
       "$HgAutomate::goldenPath/$oDb/liftOver/$over",
       "$HgAutomate::gbdb/$oDb/liftOver/$over") {
	  if (&HgAutomate::machineHasFile($dbHost, $downloads)) {
	    $entry{'files'} .= $downloads . '\r\n';
	    $entry{'redmineFiles'} .= $downloads . "\n";
	  } else {
	    &HgAutomate::verbose(0, "WARNING: $dbHost:$oDb does not have " .
			     "chain/net download $downloads !\n");
	  }
      }
    &printEntry(\%entry, $id, $oDb, "$oO Chain/Net");
    ++$id;
    undef($dbTables);
    undef(%entry);
  }
} # printSwaps

sub printAllEntries {
  # Print out SQL commands to add all entries.
  my ($entries) = @_;
  my $id = 1;
  foreach my $entry (@{$entries}) {
    &printEntry($entry, $id, $db, "");
    $id++;
  }
  &printSwaps($id);
} # printAllEntries


sub printMainPushQEntry {
  # Print out an Initial Release entry for the Main Push Queue that refers
  # to the new $db push queue created above.
  my $date = `date +%Y-%m-%d`;
  my $size = 0;
  chomp $date;
  my $qapushqSql = "$HgAutomate::runSSH qateam\@hgwbeta ./bin/x86_64/hgsql -h mysqlbeta -N qapushq";
  my $rankQuery = 'select rank from pushQ order by rank desc limit 1';
  my $rank = `echo $rankQuery | $qapushqSql`;
  $rank += 1;
  my (undef, undef, $assemblyLabel) =
    &HgAutomate::getAssemblyInfo($dbHost, $db);
  print <<_EOF_

-- New entry in Main Push Queue, to alert QA to existence of $db:
INSERT INTO pushQ SELECT right(concat("00000",convert(max(qid)+1,CHAR)),6),'','A',$rank,'$date','Y','$db Initial Release','$db','','','',$size,'hgwdev','N','','N','N','','$ENV{USER}','','','','','N','$date','',0,'','','Initial $db release (using $assemblyLabel): see separate push queue $db.','','','' from pushQ;
_EOF_
  ;
} # printMainPushQEntry


sub reportStragglers {
  my ($stragglers) = @_;
  my @names = sort (keys %{$stragglers});
  if (scalar(@names) > 0) {
    &HgAutomate::verbose(0, "
WARNING: Could not tell (from trackDb, all.joiner and hardcoded lists of
supporting and genbank tables) which tracks to assign these tables to:\n");
    foreach my $t (@names) {
      &HgAutomate::verbose(0, "  $t\n");
    }
    &HgAutomate::verbose(0, "\n");
  }
} # reportStragglers


sub makePushQSql {
  my ($entries, $stragglers) = &getEntries();
  &printHeader;
  &printAllEntries($entries);
# Obsolete pushQ database on hgwbeta July 2019
#   &printMainPushQEntry();
  &reportStragglers($stragglers);
} # makePushQSql


sub adviseDeveloper {
  # Suggest ways to ensure completeness and correctness of output.
  &HgAutomate::verbose(1, <<_EOF_

 *** All done!
 *** Please edit the output to ensure correctness before using.
 *** 1. Resolve any warnings output by this script.
 *** 2. Remove any entries which should not be pushed.
 *** 3. Add tables associated with the main track table (e.g. *Pep tables
        for gene prediction tracks).
 *** 4. Add files associated with tracks.  First, look at the results
        of this query:
          hgsql $db -e 'select distinct(path) from extFile'
        Then, look at file(s) named in each of the following wiggle tables:
_EOF_
);
  foreach my $t (@wigTables) {
    &HgAutomate::verbose(1, <<_EOF_
          hgsql $db -e 'select distinct(file) from $t'
_EOF_
    );
  }
  &HgAutomate::verbose(1, <<_EOF_
        Files go in the second field after tables (it's tables, cgis, files).
 *** 5. This script currently does not recognize composite tracks.  If $db
        has any composite tracks, you should manually merge the separate
        per-table entries into one entry.  The Chain/Net composites are taken
        care of, and the beginning of handling composites is here but not used
        yet.
 *** 6. Make sure that qapushq does not already have a table named $db:
          ssh qateam\@hgwbeta ./bin/x86_64/hgsql -h mysqlbeta qapushq -NBe "'desc $db;'"
        You *should* see this error:
          ERROR 1146 at line 1: Table 'qapushq.$db' doesn't exist
        If it already has that table, talk to QA and figure out whether
        it can be dropped or fixed up (by sql or the Push Queue web app).
 *** When everything is complete and correct, use hgsql -h mysqlbeta to
     execute the sql file.  Then use the Push Queue web app to check the
     contents of all entries.
 *** If you haven't already, please add $db to makeDb/schema/all.joiner !
     It should be in both \$gbd and \$chainDest.
_EOF_
  );
  if (@netODbs) {
    &HgAutomate::verbose(1, <<_EOF_
 *** When $db is on the RR (congrats!), please doBlastz -swap if you haven't
     already, and add Push Queue entries for those other databases' chains
     and nets to $db.
_EOF_
    );
  }
  &HgAutomate::verbose(1, "\n");
}

sub startReleaseLog($$) {
  my ($localDb, $outFile) = @_;
  open my $fh, '>', "$outFile" or die "can not write to $outFile";
  my $configRa = "/hive/data/genomes/$localDb/$localDb.config.ra";
  my $shortLabel = `grep 'assemblyShortLabel' $configRa | sed -e 's/^[^ \\t]\\+[ \t]//;'`;
  my $assemblyLabel = `grep 'assemblyLabel' $configRa | sed -e 's/^[^ \\t]\\+[ \t]//;'`;
  my $ncbiProject = `grep 'ncbiBioProject' $configRa | sed -e 's/^[^ \\t]\\+[ \t]//;'`;
  my $genBankAccessionID = `grep 'genBankAccessionID' $configRa | sed -e 's/^[^ \\t]\\+[ \t]//;'`;
  chomp $shortLabel; chomp $assemblyLabel; chomp $ncbiProject;
  chomp $genBankAccessionID;
  printf $fh "Initial %s release (using %s %s (NCBI project %d, %s)\n", $localDb, $assemblyLabel, $shortLabel, $ncbiProject, $genBankAccessionID;
  close $fh;
}	# sub startReleaseLog($$)

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();

&usage(1) if (scalar(@ARGV) != 1);
($db) = @ARGV;

if ($opt_redmineList) {
  open $redmineFileList, '|-', "sort -u | sed -e '/^\$/d' > redmine.$db.file.list" or die "can not write to redmine.$db.file.list";
  open $redmineTableList, '|-', "tr '[ ]' '[\n]' | sort -u | sed -e '/^\$/d' > redmine.$db.table.list" or die "can not write to redmine.$db.table.list";
  &startReleaseLog($db, "redmine.$db.releaseLog.txt");
  open $redmineReleaseLog, '|-', "egrep -v 'supporting tables|Genbank-process tracks' | sort -u >> redmine.$db.releaseLog.txt" or die "can not write to redmine.$db.releaseLog.txt";
  printf STDERR "# writing redmine listings to\n";
  printf STDERR "# redmine.$db.file.list\n# redmine.$db.table.list\n# redmine.$db.releaseLog.txt\n";
}

$sql = "$HgAutomate::runSSH $dbHost hgsql -N $db";

&makePushQSql();

&adviseDeveloper();

close $redmineFileList;
close $redmineTableList;
close $redmineReleaseLog;
