#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: gc5Description.pl <buildDirectory>\n";
  printf STDERR "where <buildDirectory> is where the assembly hub is under construction.\n";
  exit 255;
}

my $wrkDir = shift;

if ( ! -d $wrkDir ) {
  printf STDERR "ERROR: can not find work directory:\n\t'%s'\n", $wrkDir;
  exit 255;
}

chdir $wrkDir;
# print STDERR `ls`;
my $accAsm = basename($wrkDir);

exit 0 if ( ! -s "bbi/${accAsm}.gc5Base.ncbi.bw" );

my $groupName = $wrkDir;
$groupName =~ s#.*/genbank/##;
$groupName =~ s#/.*##;
my $selfUrl = "${groupName}/${accAsm}";

# printf STDERR "%s\n", $accAsm;
my $htmlFile="${accAsm}.gc5Base.html";
open (FH, ">$htmlFile") or die "can not write to $htmlFile";

print FH <<_EOF_
<h2>Description</h2>
<p>
The GC percent track shows the percentage of G (guanine) and C (cytosine) bases
in 5-base windows.  High GC content is typically associated with
gene-rich areas.
</p>
<p>
This track may be configured in a variety of ways to highlight different aspects 
of the displayed information. Click the &quot;Graph configuration help&quot; link
for an explanation of the configuration options.

<h2>Credits</h2>
<p> The data and presentation of this graph were prepared by
<a href="mailto:&#104;&#105;&#114;a&#109;&#64;&#115;&#111;&#101;
.&#117;&#99;&#115;&#99;.&#101;&#100;u">Hiram Clawson</a>.
</p>
_EOF_
   ;

close (FH);
