#!/usr/bin/env perl
#
#	countRmsk.pl - produce a summary statistics table of the
#	RepeatMasker elements.  Give it a <db> argument.
#	Output in plain text or optionally html
#
#	 $Id: countRmsk.pl,v 1.6 2005/04/19 17:37:03 hiram Exp $
#
use warnings;
use strict;
use DBI;
#	See also: http://search.cpan.org/~timb/DBI-1.47/DBI.pm

sub usage()
{
print "usage: $0 [options] <db>\n";
print "\toptions:\n";
print "\t-verbose - extra information during processing to STDERR\n";
print "\t-html - output in html format (plain text is default)\n";
print "\n";
print "\tper-chrom stats will be done when the assembly has less than\n";
print "\t   100 chrom names.  Scaffold based assemblies will only have\n";
print "\t   a single overall summary.\n";
print "\n";
print "\tExample usage: countRmsk -verbose -html ce2 > ce2.html\n";
print "\n";
print "\twill look for /usr/local/apache/cgi-bin/hg.conf\n";
print "\tto find connection information.  This can be overridden by\n";
print "\tthe environment variable: HGDB_CONF\n";
print "\te.g.: HGDB_CONF=~/.hg.conf.read-only $0 <db>\n";
print "\t(this script last updated: ", '$Date: 2005/04/19 17:37:03 $', ")\n";
exit 255;
}

#	chrom name sort, strip chr or scaffold prefix, compare
#	numerically when possible, see also, in kent source:
#	hg/lib/hdb.c: int chrNameCmp(char *str1, char *str2)
sub chrSort()
{
# the two items to compare are already in $a and $b
my $c1 = $a;
my $c2 = $b;
#	strip any chr or scaffold prefixes
$c1 =~ s/^chr//i;
$c1 =~ s/^scaffold_*//i;
$c2 =~ s/^chr//i;
$c2 =~ s/^scaffold_*//i;
#	see if only numbers are left
my $isNum1 = 0;	#	test numeric each arg
my $isNum2 = 0; #	test numeric each arg
$isNum1 = 1 if ($c1 =~ m/^[0-9]+$/);
$isNum2 = 1 if ($c2 =~ m/^[0-9]+$/);
#	both numeric, simple compare for result
if ($isNum1 && $isNum2)
    {
    return $c1 <=> $c2;
    }
elsif ($isNum1)	#	only one numeric, it comes first
    {
    return -1;
    }
elsif ($isNum2)	#	only one numeric, it comes first
    {
    return 1;
    }
else
    {
    return $c1 cmp $c2;	#	not numeric, compare strings
    }			#	could do more here for X,Y,M and Un
}

#	check the number of arguments
my $argc = scalar(@ARGV);
usage if ($argc < 1);

my $verbose = 0;		#	command line flags
my $html = 0;			#	command line flags
my $db_name = "";		#	db must be specified on command line

#	check the command line
for (my $i = 0; $i < $argc; ++$i)
    {
    if ($ARGV[$i] =~ m/-verbose/)
	{
	$verbose = 1;
	next;
	}
    if ($ARGV[$i] =~ m/-html/)
	{
	$html = 1;
	next;
	}
    $db_name = $ARGV[$i];
    }

#	the usual business of finding contact information from hg.conf
#	although, avoid the user's .hg.conf since we want read-only
#	permissions.
my $hg_conf="/usr/local/apache/cgi-bin/hg.conf";

#	recognize HGDB_CONF environment as does the kent source
if (exists $ENV{'HGDB_CONF'})
    {
    $hg_conf=$ENV{'HGDB_CONF'};
    }

my $db_host="localhost";	#	default read-only user and password
my $db_user="hguser";
my $db_pass="hguserstuff";

#	read and parse the hg.conf file to find host, user and password
open (FH,"<$hg_conf") or die "Can not open $hg_conf";
while (my $line = <FH>)
    {
    chomp($line);
    next if ($line =~ m/\s*#.*/);
    next if ($line =~ m/^\s*$/);
    $line =~ s/^\s*//;
#    print "$line\n";
    my @a = split('=',$line);
    $db_host = $a[1] if ($a[0] eq "db.host");
    $db_user = $a[1] if ($a[0] eq "db.user");
    $db_pass = $a[1] if ($a[0] eq "db.password");
    }
close (FH);

print STDERR "$db_host $db_user $db_pass $db_name\n" if ($verbose);

my $DB_TYPE="mysql";

#	establish MySQL connection
my $dbh = DBI->connect("DBI:$DB_TYPE:dbname=$db_name:dbhost=$db_host",
	$db_user,$db_pass) or die "Cannot access DB server";

#	fetch a chrom list from chromInfo
my $sth = $dbh->prepare(qq{select chrom from chromInfo;});
my $rc = $sth->execute() or die "select chrom from chromInfo.$db_name failed";

my $chromName="";
$rc = $sth->bind_col(1,\$chromName);

my %repClassCounts = ();	# overall repClass counts, key is repClass
my %perChromRepClassCounts = ();	# per-chrom counts, key is repClass
my @chromNames;			#	list of chrom names
my %chromNameHash;		#	to facilitate chrom name searches,
		#	key is chromName, value is chromNames[] index

my $chromCount = 0;

#	let's check the chrom list to see if it is manageable, might be
#	thousands of scaffolds in which case we are not going to do them
#	individually.
while(my $R = $sth->fetchrow_hashref)
    {
    $chromNameHash{$chromName} = $chromCount;
    $chromNames[$chromCount++] = $chromName;
    }

#	process individual chroms if there are 100 or less
my $processChroms = 1;
$processChroms = 0 if ($chromCount > 100);

my @rmskTables;		# the list of rmsk tables
my $rmskTableCount = 0; # and a count of them
my %tableToChrom;	# to relate rmsk table to its corresponding chrom name
	# key is rmskTable, value is chromNames[] index

$sth = $dbh->prepare(q{show tables like "%rmsk";});
$rc = $sth->execute() or die "show tables like rmsk failed";

my $rmskTable="";
$rc = $sth->bind_col(1,\$rmskTable);

while(my $R = $sth->fetchrow_hashref)
    {
    my $stripRmsk = $rmskTable;
    $stripRmsk =~ s/_*rmsk//;	#	leaving chrom name if any
    $rmskTables[$rmskTableCount++] = $rmskTable;
    if (length($stripRmsk) && exists($chromNameHash{$stripRmsk}))
	{
	$tableToChrom{$rmskTable} = $chromNameHash{$stripRmsk};
	}
    }

#	now walk through the rmsk tables and process them
for (my $i = 0; $i < $rmskTableCount; ++$i)
    {
    $rmskTable = $rmskTables[$i];
    $chromName = "chr";
    $chromName = $chromNames[$tableToChrom{$rmskTable}]
	if (exists($tableToChrom{$rmskTable}));
    print STDERR "$rmskTable\n" if ($verbose && $processChroms);
    my $sth_rmsk = $dbh->prepare(qq{select repClass,count(*) from $rmskTable group by repClass;});
    $rc = $sth_rmsk->execute() or die "select repClass from $rmskTable failed";
    my $repClass="";
    my $count=0;
    $rc = $sth_rmsk->bind_col(1,\$repClass);
    $rc = $sth_rmsk->bind_col(2,\$count);
    my %chromRepClassCounts = ();
    while(my $R1 = $sth_rmsk->fetchrow_hashref)
	{
	$chromRepClassCounts{$repClass} += $count;
	}
    while((my $key, my $value) = each(%chromRepClassCounts))
	{
	$repClassCounts{"$key"} += $value;
	}
    $perChromRepClassCounts{$chromName} =
	\%chromRepClassCounts if ($processChroms);
    }


if ($html)
    {
    print "<HTML><HEAD><TITLE>$db_name Repeat Masker Summary Counts</TITLE>\n";
    print "</HEAD><BODY>"
    }
#	print out the column headers
print "<TABLE BORDER=1><CAPTION>$db_name Repeat Masker Summary Counts</CAPTION>\n<TR><TH>" if ($html);
print "chrom";
print "</TH>" if ($html);
for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
    {
    if ($html) { print "<TH>"; } else {print "\t"; }
    #	for HTML output, Simple_repeat and Low_complexity become two lines
    if ($html) { $repClass =~ s/_/<BR>/; }
    print "$repClass";
    print "</TH>" if ($html);
    }
if ($html) { print "<TH>Total<BR>Item Count</TH></TR>\n"; }
    else {print "\tTotal Items\n"; }

#	print the over-all total row
print "<TR><TH ALIGN=LEFT>" if ($html);
print "overall";
print "</TH>" if ($html);
my $totalItems = 0;
for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
    {
    if ($html) { print "<TD ALIGN=RIGHT>$repClassCounts{$repClass}</TD>"; } else
	{ print "\t$repClassCounts{$repClass}"; }
    $totalItems += $repClassCounts{$repClass};
    }
if ($html) { print "<TD ALIGN=RIGHT>$totalItems</TD></TR>\n"; }
    else {print "\t$totalItems\n"; }

#	print the per-chrom lines if there are not too many
if ($processChroms)
    {
    for my $chr (sort chrSort @chromNames)
	{
	my $chrRepClassCounts = $perChromRepClassCounts{$chr};
	$totalItems = 0;
	print "<TR><TH ALIGN=LEFT>" if ($html);
	print "$chr";
	print "</TH>" if ($html);
	for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} }
		keys(%repClassCounts))
	    {
	    if (exists $chrRepClassCounts->{$repClass})
		{
		if ($html)
		   {
		   print "<TD ALIGN=RIGHT>$chrRepClassCounts->{$repClass}</TD>";
		   }
		else
		   {
		   print "\t$chrRepClassCounts->{$repClass}";
		   }
		$totalItems += $chrRepClassCounts->{$repClass};
		}
	    else
		{
		if ($html) { print "<TD ALIGN=RIGHT>0</TD>"; }
		    else { print "\t0"; }
		}
	    }
	if ($html) { print "<TD ALIGN=RIGHT>$totalItems</TD></TR>\n"; }
	    else {print "\t$totalItems\n"; }
	}	#	for my $chr (sort chrSort @chromNames)
    }	#	if ($processChroms)

if ($html) { print "</TABLE></BODY></HTML>\n"; } else {print "\n"; }

$dbh->disconnect;
