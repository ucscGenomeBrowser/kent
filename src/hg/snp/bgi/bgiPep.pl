#!/usr/bin/perl -w

# Get annotation source from filename, abbreviate & make it a 
# suffix to uniquify the gene names.  Shorten huge ENST|ENSG gene names.

my $geneSrcExpr = '(GenBank_complete|GenBank_partial|HumanDiseaseGene|IMS-GSF|UMIST|Hans_Cheng|Leif_Andersson|Martien_Groenen)';
my %geneSrcAbbrev = ('GenBank_complete' => 'GBC',
		     'GenBank_partial' => 'GBP',
		     'HumanDiseaseGene' => 'HDG',
		     'IMS-GSF' => 'IG',
		     'UMIST' => 'UM',
		     'Hans_Cheng' => 'HC',
		     'Leif_Andersson' => 'LA',
		     'Martien_Groenen' => 'MG',
		    );

use strict;

foreach my $fname (@ARGV) {
  if ($fname =~ m@$geneSrcExpr.*\.pep(.*)$@) {
    my ($geneSrc, $gz) = ($1, $2);
    my $abbrev = $geneSrcAbbrev{$geneSrc};
    die "Can't abbreviate $geneSrc" if (! defined $abbrev);
    if ($gz ne "") {
      open(IN, "gunzip -c '$fname'|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    my $geneName;
    my $seq = "";
    while (<IN>) {
      if (/^>(\S+)/) {
	if (defined $geneName) {
	  print ">$geneName\n$seq";
	  $seq = "";
	}
	$geneName = $1;
	$geneName =~ s/ENST0000(.*)\|ENSG.*/ENST$1/;
	$geneName = $abbrev . "." . $geneName;
      } else {
	$seq .= $_;
      }
    }
    close(IN);
  } else {
    die "Can't get gene source info from filename $fname\n";
  }
}
