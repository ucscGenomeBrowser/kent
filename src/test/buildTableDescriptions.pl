#!/usr/bin/env perl
#
# hgTest.pl: parameterizable test for hg CGI tools.
# See usage for a description of parameters.
#

use warnings;
# Figure out path of executable so we can add perllib to the path.
use FindBin qw($Bin);
use lib "$Bin/perllib";
use TrackDb;
use HgConf;

use Getopt::Long;
use DBI;
use Carp;
use strict;

#
# Default behaviors, changeable by command line args:
#
my $kentSrc   = "/cluster/home/angie/kentclean/src";
my $gbdDPath  = "/cluster/home/angie/browser/goldenPath/gbdDescriptions.html";
my $noLoad    = 0;
my $verbose   = 0;

# Hard-coded behaviors:
my $debug         = 0;
# These are ignored to avoid errors about duplicate table/object definitions.
my %autoSqlIgnore = ( "hg/autoSql/tests/input/polyTest.as" => "",
		      "hg/autoSql/tests/input/newTest.as" => "",
		      "hg/autoSql/doc.as" => "",
		      "hg/autoSql/doc2.as" => "",
		      "hg/instinct/instinctMember/members.as" => "",
		      "hg/lib/bed.as" => "",
		      "hg/lib/ggDbRep.as" => "",
		      "hg/lib/genotype.as" => "",
		      "hg/lib/ispyTables.as" => "",
		      "hg/makeDb/schema/joinerGraph/all.as" => "",
		      "hg/makeDb/schema/joinerGraph/swissProt.as" => "",
		      "hg/protein/spToDb/spDbTables.as" => "",
		      "hg/ultras/ultraDb/as/udHistory.as" => "",
		      "hg/bioImage/bioImage.as" => "",
		      "hg/bioImage/loadMahoney/mahoney.as" => "",
		      "hg/genePix/genePix.as" => "",
		      "hg/genePix/loadMahoney/mahoney.as" => "",
		      "hg/visiGene/visiGene.as" => "",
		      "hg/visiGene/vgLoadMahoney/mahoney.as" => "",
		      "lib/pslWScore.as" => "",
		    );

my $basename      = $0;  $basename =~ s@.*/@@;
#
# usage: Print help message and exit, happy or unhappy.
#
sub usage {
    print STDERR "Usage:
$basename  [-kentSrc dir]  [-gbdDPath f]  [-noLoad]  [-help]
    -kentSrc dir:	Use dir as the kent/src checkout.  
			Default: $kentSrc.
    -gbdDPath f:	Use f as the gbdDescriptions.html.  
			Default: $gbdDPath.
    -db db:             Work only on db, not on all active dbs.
    -hgConf file:       Use file instead of ~/.hg.conf.
    -noLoad:		Don't load the database, just create .sql files.
    -help:		Print this message.
";
    exit(@_);
} # end usage


#
# getActiveDbs: connect to central db, get list of active dbs
#
sub getActiveDbs {
  my $hgConf = shift;
  confess "Too many arguments" if (defined shift);
  my $centdb = $hgConf->lookup('central.db');
  my $host = $hgConf->lookup('central.host');
  my $username = $hgConf->lookup('central.user');
  my $password = $hgConf->lookup('central.password');
  my $dbh = DBI->connect("DBI:mysql:database=$centdb;host=$host",
			 $username, $password);
  my $results =
      $dbh->selectcol_arrayref("select name from dbDb where active = 1;");
  $dbh->disconnect();
  return @{$results};
}

#
# simplifyFields: trim fieldSpec of occasionally-used prefix/suffix fields
#
sub simplifyFields {
  my $fieldSpec = shift;
  confess "Too few arguments"  if (! defined $fieldSpec);
  confess "Too many arguments" if (defined shift);
  $fieldSpec =~ s/^bin,//;
  $fieldSpec =~ s/,crc,$/,/;
  return $fieldSpec;
}

#
# getTableFields: connect to db, get tables and fields 
#
sub getTableFields {
  my $hgConf = shift;
  my $db = shift;
  confess "Too few arguments"  if (! defined $db);
  confess "Too many arguments" if (defined shift);
  my $username = $hgConf->lookup('db.user');
  my $password = $hgConf->lookup('db.password');
  my $dbh = DBI->connect("DBI:mysql:$db", $username, $password);
  my %tableFields = ();
  my %tableNamesInsens = ();
  my $tables = $dbh->selectcol_arrayref("show tables;");
  foreach my $t (@{$tables}) {
    my $desc = $dbh->selectcol_arrayref("desc $t;");
    my $fields = "";
    foreach my $f (@{$desc}) {
      $fields .= "$f,";
    }
    $t =~ s/^chr\w+_(\w+_)?/chrN_/;
    $fields = &simplifyFields($fields);
    if (defined $tableFields{$t} && 
	$tableFields{$t} ne $fields) {
      warn "Duplicate fieldSpec for table $db.$t:\n$tableFields{$t}\n$fields";
    }
    my $tableInsens = $t;
    $tableInsens =~ tr/A-Z/a-z/;
    if (! defined $tableFields{$t} &&
	defined $tableNamesInsens{$tableInsens}) {
      warn "Case-insensitive duplicate for $db.$t... dropping.";
    } else {
      $tableFields{$t} = $fields;
      $tableNamesInsens{$tableInsens} = 1;
    }
  }
  $dbh->disconnect();
  return %tableFields;
}

#
# slurpAutoSql: find all .as files under rootDir, grab contents.
#
sub slurpAutoSql {
  my $rootDir = shift;
  confess "Too few arguments"  if (! defined $rootDir);
  confess "Too many arguments" if (defined shift);
  open(P, "find $rootDir -name '*.as' -print |") || die "Can't open pipe";
  my %tableAS = ();
  my %objectAS = ();
  my $gotLeftParen = 0;
  while (<P>) {
    chop;
    my $filename = $_;
    my $filetail = $filename;  $filetail =~ s/^$kentSrc\///;
    next if (defined $autoSqlIgnore{$filetail});
    open(F, "$filename") || die "Can't open $filename";
    my $as = "";
    my $table = "";
    my $object = "";
    my $fields = "";
    while (<F>) {
      $as .= $_;
      if (/^\s*\(/) {
	$gotLeftParen = 1;
      }
      if (/^\s*table\s+(\S+)[^\;]*$/i) {
	$table = $1;
	$object = "";
      } elsif (/^\s*(object|simple)\s+(\S+)/ && !$gotLeftParen) {
	$object = $2;
	$table = "";
      } elsif (/^\s*enum\s*\([^\)]+\s*(\S+)\s*;/) {
	$fields .= "$1,";
      } elsif (/^[^\"]+\s+(\S+)\s*;/) {
	$fields .= "$1,";
      } elsif (/^\s*\)/) {
	if (($table eq "" && $object eq "") || $fields eq "") {
	  die "Trouble parsing autoSql file $filename:\n$as";
	}
	if ($table ne "") {
	  if (defined $tableAS{$table}) {
	    die "Duplicate autoSql def for table $table (" .
	      $tableAS{$table}->{filename} . " vs. $filename)";
	  }
	  $tableAS{$table} = { fields => &simplifyFields($fields),
			       autoSql => $as,
			       tableName => $table,
			       filename => $filename, };
	} elsif ($object ne "") {
	  if (defined $objectAS{$table}) {
	    die "Duplicate autoSql def for object $object (" .
	      $objectAS{$object}->{filename} . " vs. $filename)";
	  }
	  $objectAS{$object} = { autoSql => $as,
				 objectName => $object,
				 filename => $filename, };
	}
	$as = $table = $object = $fields = "";
	$gotLeftParen = 0;
      }
    } # each line of autoSql file
    close(F);
  } # each autoSql file found in $rootDir
  close(P);
  # Add bedN (for when we get the fields from the trackDb.type).
  my @bedFields = split(",", $tableAS{"bed"}->{fields});
  my @autoSql   = split("\n", $tableAS{"bed"}->{autoSql});
  my $filename  = $tableAS{"bed"}->{filename};
  for (my $n=scalar(@bedFields);  $n >= 3;  $n--) {
    $tableAS{"bed$n"} = { fields => join(",", @bedFields) . ",",
			  autoSql => join("\n", @autoSql) . "\n",
			  tableName => "bed$n",
			  filename => $filename, };
    my $lastField = pop(@bedFields);
    my @newAS = ();
    my $nm1 = $n - 1;
    foreach my $as (@autoSql) {
      $as =~ s/table\s+\S+/table bed$nm1/;
      if ($as !~ /\s+$lastField\s*;/) {
	push @newAS, $as;
      }
    }
    @autoSql = @newAS;
  }
  # When table definitions rely on objects, add the object definitions to
  # the stored table autoSql (so the autoSql parser doesn't die in hgTables).
  foreach my $table (keys %tableAS) {
    if ($tableAS{$table}->{autoSql} =~ /\n\s+table (\w+)/) {
      my $object = $1;
      if (defined $objectAS{$object}) {
	$tableAS{$table}->{autoSql} .= ("\n" . $objectAS{$object}->{autoSql});
      } elsif (defined $tableAS{$object}) {
	$tableAS{$table}->{autoSql} .= ("\n" . $tableAS{$object}->{autoSql});
      } else {
	warn "Incomplete AutoSql? table $table refers to object $object " .
	  "but I can't find the definition of $object.";
      }
    }
  }
  return %tableAS;
}

#
# indexAutoSqlByFields: make a new AutoSql hash, indexed by fields not table
#
sub indexAutoSqlByFields {
  my $tASRef = shift;
  confess "Too few arguments"  if (! defined $tASRef);
  confess "Too many arguments" if (defined shift);
  my %fieldsAS = ();
  foreach my $t (keys %{$tASRef}) {
    my $asRef = $tASRef->{$t};
    my $fields = $asRef->{fields};
    $fieldsAS{$fields} = $asRef;
  }
  return %fieldsAS;
}

#
# matchAutoSqlByFields: see if there's an autoSql def for given fields.
#
sub matchAutoSqlByFields {
  my $fields = shift;
  my $tASRef = shift;
  my $fASRef = shift;
  confess "Too few arguments"  if (! defined $fASRef);
  confess "Too many arguments" if (defined shift);
  # try standard types first, to save time (and avoid dupl's for std types).
  if ($fields eq $tASRef->{"psl"}->{fields}) {
    return $tASRef->{"psl"};
  } elsif ($fields eq $tASRef->{"genePredExt"}->{fields}) {
    return $tASRef->{"genePredExt"};
  } elsif ($fields eq $tASRef->{"genePred"}->{fields}) {
    return $tASRef->{"genePred"};
  } elsif ($fields eq $tASRef->{"lfs"}->{fields}) {
    return $tASRef->{"lfs"};
  } elsif ($fields eq $tASRef->{"genericNameValue"}->{fields}) {
    return $tASRef->{"genericNameValue"};
  } else {
    for (my $n=12;  $n >= 3;  $n--) {
      if ($fields eq $tASRef->{"bed$n"}->{fields}) {
	return $tASRef->{"bed$n"};
      }
    }
    return $fASRef->{$fields};
  }
}


#
# parseGbdDescriptions: parse anchors and .as out of gbdDescriptions.html
#
sub parseGbdDescriptions {
  my $filename = shift;
  confess "Too few arguments"  if (! defined $filename);
  confess "Too many arguments" if (defined shift);
  open(F, "$filename") || die "Can't open $filename";
  my %tableAnchors = ();
  my $anchor = "";
  while (<F>) {
    if (m/<a name=\"?(\w+)\"?/i) {
      $anchor = $1;
    } elsif (/<PRE>\s*table\s+([\w_]+)/) {
      $tableAnchors{$1} = $anchor;
    }
  }
  close(F);
  return %tableAnchors;
}


###########################################################################
#
# Parse & process command line args
#
# GetOptions will put command line args here:
use vars qw/
    $opt_kentSrc
    $opt_gbdDPath
    $opt_noLoad
    $opt_db
    $opt_hgConf
    $opt_help
    $opt_verbose
    /;

my $ok = GetOptions("kentSrc=s",
		    "gbdDPath=s",
		    "noLoad",
		    "db=s",
		    "hgConf=s",
		    "help",
		    "verbose");
&usage(1) if (! $ok);
&usage(0) if ($opt_help);
$kentSrc  = $opt_kentSrc if ($opt_kentSrc);
$gbdDPath = $opt_gbdDPath if ($opt_gbdDPath);
$noLoad   = 1 if (defined $opt_noLoad);
$verbose  = $opt_verbose if (defined $opt_verbose);
$verbose  = 1 if ($debug);


############################################################################
# MAIN

my %tableAutoSql = slurpAutoSql($kentSrc);
my %fieldsAutoSql = indexAutoSqlByFields(\%tableAutoSql);
my %tableAnchors = parseGbdDescriptions($gbdDPath);
my $hgConf = HgConf->new($opt_hgConf);
my @auxDbs = ('hgFixed', 'proteome');
my @dbs = (defined $opt_db) ? split(',', $opt_db) :
                              (&getActiveDbs($hgConf), @auxDbs);
foreach my $db (@dbs) {
  if (($db !~ /^\w\w\d+$/ && $db !~ /^\w\w\w\w\w\w\d+$/ &&
       $db !~ /^caePb\d+$/ && $db ne 'hiv1' &&
       (scalar(grep {$_ eq $db} @auxDbs) == 0)) ||
      $db =~ /^zoo/) {
    print "Skipping database $db.\n";
    next;
  }
  my $sqlFile = "$db.tableDescriptions.sql";
  open(SQL, ">$sqlFile") || die "Can't open $sqlFile for writing";
  print SQL "use $db;\n";
  print SQL "drop table if exists tableDescriptions;";
  open(F, "$kentSrc/hg/lib/tableDescriptions.sql")
    || die "Can't open $kentSrc/hg/lib/tableDescriptions.sql";
  while (<F>) {
    print SQL;
  }
  close (F);
  my $trackDb = TrackDb->new($db);
  my %tableTypes = $trackDb->getTrackNamesTypes();
  my %tableFields = &getTableFields($hgConf, $db);
  foreach my $table (sort keys %tableFields) {
    next if ($table =~ /^(trackDb|hgFindSpec)_/);
    if ((! defined $tableAutoSql{$table}) ||
	($tableFields{$table} ne $tableAutoSql{$table}->{fields})) {
      my $as =
	&matchAutoSqlByFields($tableFields{$table}, \%tableAutoSql,
			      \%fieldsAutoSql);
      if (defined $as) {
	$tableAutoSql{$table} = $as;
      }
    }
    if (! defined $tableTypes{$table} &&
       defined $tableAutoSql{$table}) {
      $tableTypes{$table} = $tableAutoSql{$table}->{tableName};
      $tableTypes{$table} =~ s/bed\d+/bed/;
    }
    my $type   = $tableTypes{$table};
    if (defined $type && $table ne "mrna") {
      my $typeN = $type;
      $typeN =~ s/^bed (\d+).*/bed$1/;
      $typeN =~ s/^(\w+).*/$1/;
      $type =~ s/^(\w+).*/$1/;
      if (! defined $tableAutoSql{$table} &&
	  defined $tableAutoSql{$typeN}) {
	$tableAutoSql{$table} = $tableAutoSql{$typeN};
      }
      if (! defined $tableAnchors{$table} &&
	  defined $tableAnchors{$type}) {
	$tableAnchors{$table} = $tableAnchors{$type};
      }
    }
    my $as     = $tableAutoSql{$table};
    if (defined $as) {
      if ($tableFields{$table} ne $as->{fields}) {
	print "$db.$table FIELD MISMATCH:\n";
	print "$db.$table table fields:   $tableFields{$table}\n";
	print "$db.$table autoSql fields: $as->{fields} [$as->{tableName}]\n";
      }
    } else {
      print "$db.$table: No AutoSql.\n";
    }
    my $anchor = $tableAnchors{$table} || "";
    #*** should suggest addition to gbdD of table&.as if not already in there;
    #*** should complain about gbdD tables not in any active db.  
    my $asd = (defined $as) ? $as->{autoSql} : "";
    $asd =~ s/'/\\'/g;
    print SQL "INSERT INTO tableDescriptions (tableName, autoSqlDef, gbdAnchor)"
      . " values ('$table', '$asd', '$anchor');\n";
  }
  close(SQL);
  if (! $noLoad) {
    (! system("hgsql $db < $sqlFile")) || warn "hgsql error for $sqlFile";
    print "Loaded $db.tableDescriptions.\n";
    unlink($sqlFile);
  }
}

