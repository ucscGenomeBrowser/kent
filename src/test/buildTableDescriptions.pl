#!/usr/bin/perl -w
#
# hgTest.pl: parameterizable test for hg CGI tools.
# See usage for a description of parameters.
#

# Figure out path of executable so we can add perllib to the path.
use FindBin qw($Bin);
use lib "$Bin/perllib";
use TrackDb;
use HgConf;

use Getopt::Long;
use DBI;
use Carp;
use strict;

my $kentSrc   = "/cluster/home/angie/kentclean/src";
my $gbdDPath  = "/cluster/home/angie/browser/goldenPath/gbdDescriptions.html";
my %autoSqlIgnore = ( "$kentSrc/hg/lib/bed.as" => "",
		      "$kentSrc/hg/lib/ggDbRep.as" => "",
		      "$kentSrc/hg/lib/rmskOut.as" => "",
		      "$kentSrc/lib/pslWScore.as" => "",
		    );

#*** usage

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
  my $tables = $dbh->selectcol_arrayref("show tables;");
  foreach my $t (@{$tables}) {
    my $desc = $dbh->selectcol_arrayref("desc $t;");
    my $fields = "";
    foreach my $f (@{$desc}) {
      $fields .= "$f,";
    }
    $t =~ s/^chr\w+_(random_)?/chrN_/;
    $fields = &simplifyFields($fields);
    if (defined $tableFields{$t} && 
	$tableFields{$t} ne $fields) {
      warn "Duplicate fieldSpec for table $t:\n$tableFields{$t}\n$fields";
    }
    $tableFields{$t} = $fields;
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
  while (<P>) {
    chop;
    my $filename = $_;
    next if (defined $autoSqlIgnore{$filename});
    open(F, "$filename") || die "Can't open $filename";
    my $as = "";
    my $table = "";
    my $object = "";
    my $fields = "";
    while (<F>) {
      $as .= $_;
      if (/^\s*table\s+(\S+)/) {
	$table = $1;
	$object = "";
      } elsif (/^\s*(object|simple)\s+(\S+)/) {
	$object = $2;
	$table = "";
      } elsif (/^\s*\S+\s+(\S+);/) {
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
	}
	$as = $table = $object = $fields = "";
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
  } elsif ($fields eq $tASRef->{"genePred"}->{fields}) {
    return $tASRef->{"genePred"};
  } elsif ($fields eq $tASRef->{"lfs"}->{fields}) {
    return $tASRef->{"lfs"};
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


# cross-ref tables and .as -> hgFixed.autoSqlDefs table -> def; 
#  complain about .as-less tables

# cross-ref tables and gbdD -> hgFixed.gbdAnchors table -> anchor ;
#  suggest addition to gbdD of table&.as if not already in there;
#  complain about gbdD tables not in any active db.  

############################################################################
# MAIN

my %tableAutoSql = slurpAutoSql($kentSrc);
my %fieldsAutoSql = indexAutoSqlByFields(\%tableAutoSql);
my %tableAnchors = parseGbdDescriptions($gbdDPath);
my $hgConf = HgConf->new();
my @dbs = &getActiveDbs($hgConf);
foreach my $db (@dbs) {
  next if ($db !~ /^\w\w\d+$/);
  open(SQL, ">tableDescriptions.sql") || die "Can't open .sql for writing";
  print SQL "use $db;\n";
  open(F, "$kentSrc/hg/lib/tableDescriptions.sql") || die "Can't open tableDescriptions.sql";
  while (<F>) {
    print SQL;
  }
  close (F);
  my $trackDb = TrackDb->new($db);
  my %tableTypes = $trackDb->getTrackNamesTypes();
  my %tableFields = &getTableFields($hgConf, $db);
  foreach my $table (sort keys %tableFields) {
    next if ($table =~ /^trackDb_/);
    if (! defined $tableAutoSql{$table}) {
      $tableAutoSql{$table} =
	&matchAutoSqlByFields($tableFields{$table}, \%tableAutoSql,
			      \%fieldsAutoSql);
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
    my $asd = (defined $as) ? $as->{autoSql} : "";
    $asd =~ s/'/\\'/g;
    print SQL "INSERT INTO tableDescriptions (tableName, autoSqlDef, gbdAnchor)"
      . " values ('$table', '$asd', '$anchor');\n";
  }
  close(SQL);
  system("echo drop table tableDescriptions | hgsql $db");
  (! system("hgsql $db < tableDescriptions.sql"))
    || die "hgsql error for $db tableDescriptions.sql";
  print "Loaded $db.tableDescriptions.\n";
}

