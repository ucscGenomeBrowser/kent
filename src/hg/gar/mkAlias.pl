#!/usr/bin/env perl

use strict;
use warnings;

my %alreadyDone;	# a record of lc(names) already used, can not have
		# duplicate alias names pointing to different destinations
                # value is lc(destination)
my %ambiguous;	# alias names that match to more than one target, don't use
		# them.  key is lc(alias) value is destination
my %goodToGo;	# key is lc(alias), value is alias<tab>destination neither lc()
                # any ambiguous ones have been removed if so discovered later

my %protectedAliases;	# key is the lc(alias) name, value is their destination
                # if any other aliases come in after they are in this list,
                # don't use them or knock anything out if they are ambiguous
                # these aliases are pointing to database browser instances
                # on the RR, they take precedence over any other relationships
## establish a list of active database browsers
my %rrActive;	# key is db name from hgcentral where active=1, value is 1
my %rrCiActive;	# key is lc(db) name for case insensitive matches

sub outOne($$) {
  my ($alias, $destination) = @_;
  my $lcAlias = lc($alias);
  my $lcDest = lc($destination);
  return if ($lcAlias eq $lcDest);	# no identity functions
  return if (defined($ambiguous{$lcAlias}));	# ignore ambiguous aliases
  return if (defined($ambiguous{$lcDest}));	# ignore ambiguous aliases
  return if (defined($rrCiActive{$lcAlias}));	# no aliases for UCSC db names
  if (defined($protectedAliases{$lcAlias})) {
    ## warning if ambiguous
    if ($protectedAliases{lc($alias)} ne lc($destination)) {
      printf STDERR "# WARNING: protected and ambiguous ? %s -> %s %s\n", $alias, $destination, $protectedAliases{lc($alias)};
    }
    return;
  }
  if (defined($alreadyDone{$lcAlias})) {	# check for ambiguous dups
     if ($alreadyDone{$lcAlias} ne $lcDest) {
        $ambiguous{$lcAlias} = $destination;
        $ambiguous{$lcDest} = $alias;
        printf STDERR "# ambiguous %s %s %s\n", $alias, $destination, $alreadyDone{$lcAlias};
        delete($goodToGo{$lcDest}) if (defined($goodToGo{$lcDest}));
        delete($goodToGo{$lcAlias}) if (defined($goodToGo{$lcAlias}));
     }	# else it is not ambiguous, only an exact duplicate
  } else {	# this is a new one
     $alreadyDone{$lcAlias} = $lcDest;
     $alreadyDone{$lcDest} = $lcAlias;  # no circular definitions
     $goodToGo{$lcAlias} = sprintf("%s\t%s", $alias, $destination);
  }
}

# just for verficatiuon, see if any ambiguous names come in here
# there should not be anything like that
sub priorityAmbiguous($$) {
  my ($id, $db) = @_;
  if ($protectedAliases{lc($id)} ne lc($db)) {
     printf STDERR "# ERROR: ambiguous protectedAlias: %s -> %s != %s\n", $id, $db, $protectedAliases{lc($id)};
     exit 255;
  }
  return;
}

# special relationships for database browsers on the RR, they take
# precedence over all other relationships, check for ambiguous confusion
sub dbPriority($$) {
  my ($asmId, $db) = @_;
  if (defined($protectedAliases{lc($asmId)})) {
     priorityAmbiguous($asmId, $db);
  }
  outOne($asmId, $db);
  $protectedAliases{lc($asmId)} = lc($db);
  my @a = split('_', $asmId, 3);
  my $accession = "$a[0]_$a[1]";
  if (defined($protectedAliases{lc($accession)})) {
     priorityAmbiguous($accession, $db);
  }
  outOne($accession, $db);
  $protectedAliases{lc($accession)} = lc($db);
  if (defined($protectedAliases{lc($a[2])})) {
     priorityAmbiguous($a[2], $db);
  }
  outOne($a[2], $db);
  $protectedAliases{lc($a[2])} = lc($db);
}


open (FH, "hgsql -N -hgenome-centdb -e 'select name from dbDb where active=1;' hgcentral|") or die "can not hgsql select from dbDb.hgcentral";
while (my $dbName = <FH>) {
  chomp $dbName;
  $rrActive{$dbName} = 1;
  $rrCiActive{lc($dbName)} = 1;
}
close (FH);

printf STDERR "# first preference, specific high priority database assemblies\n";
# first preference are the high priority database assemblies
# these should override GenArk hubs or any other relations, these are
# manually verified
dbPriority("GCA_009914755.4_T2T-CHM13v2.0", "hs1");
dbPriority("GCF_009914755.1_T2T-CHM13v2.0", "hs1");

dbPriority("GCF_000001405.40_GRCh38.p14", "hg38");
dbPriority("GCA_000001405.29_GRCh38.p14", "hg38");

dbPriority("GCA_000001405.14_GRCh37.p13", "hg19");
dbPriority("GCF_000001405.25_GRCh37.p13", "hg19");

dbPriority("GCA_000001635.1_MGSCv37", "mm9");
dbPriority("GCF_000001635.18_MGSCv37", "mm9");

dbPriority("GCA_000001635.8_GRCm38.p6", "mm10");
dbPriority("GCF_000001635.26_GRCm38.p6", "mm10");

dbPriority("GCA_000001635.9_GRCm39", "mm39");
dbPriority("GCF_000001635.27_GRCm39", "mm39");

# second priority, see if we can add more aliases for equivalent
#  active browsers on the RR

printf STDERR "# second priority, any NCBI name perfect match to active RR database\n";

open (FH, "hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE ((sourceAuthority=\"refseq\" OR sourceAuthority=\"genbank\") AND destinationAuthority=\"ucsc\") AND ABS(matchCount-sourceCount)<1;' hgFixed|") or die "can not SELECT from asmEquivalent";
while (my $line=<FH>) {
  chomp $line;
  my ($asmId, $db) = split('\t', $line);
  if (defined($rrActive{$db})) {
    dbPriority($asmId, $db);
  }
#  printf STDERR "# '%s'\t'%s'\n", $asmId, $db;
}
close (FH);

# third priority, any NCBI name with a match count difference of 1 matching
#  any active RR database, but do NOT override or conflict with any of the
#   previously established relationships.
printf STDERR "# third priority, any NCBI name with a close match count\n";

open (FH, "hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE ((sourceAuthority=\"refseq\" OR sourceAuthority=\"genbank\") AND destinationAuthority=\"ucsc\") AND ABS(matchCount-sourceCount)=1;' hgFixed|") or die "can not SELECT from asmEquivalent";
while (my $line=<FH>) {
  chomp $line;
  my ($asmId, $db) = split('\t', $line);
  next if (defined($protectedAliases{lc($asmId)}));
  if (defined($rrActive{$db})) {
    printf STDERR "# secondary %s -> %s\n", $asmId, $db;
    dbPriority($asmId, $db);
  }
}
close (FH);

printf STDERR "# fourth GenArk hubs\n"; 
# fourth preference will be GenArk hubs
open (FH, "grep -v '^#' UCSC_GI.assemblyHubList.txt|grep -v GCA_009914755.4|") or die "can not read UCSC_GI.assemblyHubList.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($accession, $asmName, $rest) = split('\t', $line, 3);
  my $asmId = "${accession}_${asmName}";
  outOne($accession, $accession);
  outOne($asmName, $accession);
  outOne($asmId, $accession);
}
close (FH);

printf STDERR "# fifth equivalent genbank assemblies\n"; 
# fourth set, equivalent genbank to refseq only one difference and
# destination is an assembly hub

# extract ordered list of asmId from GenArk listing:
`grep -v "^#" UCSC_GI.assemblyHubList.txt | \
  awk -F\$'\\t' '{printf "%s_%s\\n", \$1,\$2}' | sort -u > genArk.asmId.list`;

# and extract genbank to refseq equivalents with only one difference:
# join with genArk list to select only those for a destination:
`hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE (sourceAuthority="genbank" AND destinationAuthority="refseq") AND ABS(matchCount-sourceCount)<2;' hgFixed | sort -k2 > asmEquiv.gbk.rs.list`;

open (FH, "join -t\$'\\t' -1 2 asmEquiv.gbk.rs.list genArk.asmId.list | awk '{printf \"%s\\t%s\\n\", \$2, \$1}'|") or die "can not join asmEquiv.gbk.rs.list genArk.asmId.list";
while (my $line = <FH>) {
  chomp $line;
  my ($src, $dest) = split('\t', $line);
  my @d = split('_', $dest, 3);
  my $genArk = "$d[0]_$d[1]";
  my @a = split('_', $src, 3);
  my $accession = "$a[0]_$a[1]";
  outOne($accession, $genArk);
  outOne($src, $genArk);
}
close (FH);

if ( 0 == 1 ) {
printf STDERR "# fifth no equivalents\n"; 
# fifth set are those we have no equivalent, can be requested

open (FH, "awk -v FS=\$'\t' -v OFS=\$'\t' '\$2 = \"request\"' primates.tableData.txt mammals.tableData.txt birds.tableData.txt fish.tableData.txt fungi.tableData.txt invertebrate.tableData.txt plants.tableData.txt vertebrate.tableData.txt|") or die "can not read *.tableData.txt";
while (my $line = <FH>){
  my (undef, undef, $comName, $sciName, $asmId, $rest) = split('\t', $line, 6);
  my ($gcX, $id, $name) = split('_', $asmId, 3);
  my $accession = "${gcX}_$id";
  outOne($accession, "n/a");
  outOne($asmId, "n/a");
  outOne($name, "n/a");
}
close (FH);
}

# all done, output the table
foreach my $lcAlias(sort keys %goodToGo) {
  printf "%s\n", $goodToGo{$lcAlias};
}
