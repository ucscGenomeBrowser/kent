#!/usr/bin/perl -w

# BGI's exon_intron.txt files are almost genePred.tab, but they have an 
# extra column that needs to be clipped, and their start coords need to 
# be translated to 0-based.  
# Also, get annotation source from filename, abbreviate & make it a 
# suffix to uniquify the gene names.  Shorten huge ENST|ENSG gene names.

my $geneSrcExpr = '(GenBank_complete|GenBank_partial|HumanDiseaseGene|IMS-GSF|UMIST\(orf lessthan 300\)|UMIST\(orf morethan 300\)|UMIST|Hans_Cheng|Leif_Andersson|Martien_Groenen)';
my %geneSrcAbbrev = ('GenBank_complete' => 'GBC',
		     'GenBank_partial' => 'GBP',
		     'HumanDiseaseGene' => 'HDG',
		     'IMS-GSF' => 'IG',
		     'UMIST(orf lessthan 300)' => 'UM',
		     'UMIST(orf morethan 300)' => 'UM',
		     'UMIST' => 'UM',
		     'Hans_Cheng' => 'HC',
		     'Leif_Andersson' => 'LA',
		     'Martien_Groenen' => 'MG',
		    );

use strict;

foreach my $fname (@ARGV) {
  if ($fname =~ m@$geneSrcExpr.*\.txt(.*)$@) {
    my ($geneSrc, $gz) = ($1, $2);
    my $abbrev = $geneSrcAbbrev{$geneSrc};
    die "Can't abbreviate $geneSrc" if (! defined $abbrev);
    if ($gz ne "") {
      open(IN, "gunzip -c '$fname'|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    while (<IN>) {
      chop;
      my @words = split();
      $words[0] =~ s/ENST0000(.*)\|ENSG.*/ENST$1/;
      $words[0] = $abbrev . "." . $words[0];
      $words[3] -= 1;
      $words[5] -= 1 if ($words[5] != 0);
      my @starts = split(',', $words[8]);
      $words[8] = "";
      foreach my $s (@starts) {
	$s -= 1;
	$words[8] .= "$s,";
      }
      $#words = 9;
      print join("\t", @words) . "\n";
    }
    close(IN);
  } else {
    die "Can't get gene source info from filename $fname\n";
  }
}
