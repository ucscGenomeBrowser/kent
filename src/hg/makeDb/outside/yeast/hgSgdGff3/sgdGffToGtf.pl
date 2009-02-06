#!/usr/bin/env perl

use strict;
use warnings;

##	"$Id: sgdGffToGtf.pl,v 1.1 2009/02/06 21:43:15 hiram Exp $";

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "/sgdGffToGtf.pl - transform SGD gff format to UCSC gtf\n";
    printf STDERR "usage: ./sgdGffToGtf.pl <file.gff>\n";
    printf STDERR "\te.g.: ./sgdGffToGtf.pl S.cerevisiae.gff\n";
    exit 255;
}

#### decode subroutine found at:
####	http://www.troubleshooters.com/codecorn/littperl/perlcgi.htm
sub decode($)
  {
  $_[0] =~ s/\+/ /g;   ### Change + to space
  my(@parts) = split /%/, $_[0];
  my($returnstring) = "";

  (($_[0] =~ m/^\%/) ? (shift(@parts)) : ($returnstring = shift(@parts)));

  my($part);
  foreach $part (@parts)
    {
    $returnstring .= chr(hex(substr($part,0,2)));
    my($tail) = substr($part,2);
    $returnstring .= $tail if (defined($tail));
    }
  return($returnstring);
  }

my $file = shift;

my $geneCount = 0;
my %geneNames;
my %exonCount;
my %noteTypes;
my %noteCategories;

open (FH,"<$file") or die "can not open $file";

open (OT,">leftOvers.gff") or die "can not write to leftOvers.gff";
open (NT,"|sort -u > notes.txt") or die "can not write to notes.txt";

while (my $line = <FH>) {
    next if ($line =~ m/^\s*#/);
    chomp $line;
    my (@fields) = split('\t',"$line");
    if (scalar(@fields) != 9) {
	printf STDERR "field count != 9 ? at:\n";
	printf STDERR "$line\n";
	exit 255;
    }
    my (@field9) = split(';',$fields[8]);
    my $field9Items = scalar(@field9);
    if ($fields[2] =~ m/^gene$|^transposable_element_gene$|^pseudogene$/) {
	for (my $i = 0; $i < $field9Items; ++$i) {
	    if ($field9[$i] =~ m/^ID=/) {
		my ($id, $name) = split('=',$field9[$i]);
		if (exists($geneNames{$name})) {
		    printf STDERR "duplicate gene name: '$name'\n";
		    $geneNames{$name} += 1;
		} else {
		    $geneNames{$name} = 1;
		    ++$geneCount;
		}
	    }
	}
    } elsif ($fields[2] =~ m/^CDS$/) {
	for (my $i = 0; $i < 8; ++$i) {
	    printf "$fields[$i]\t";
	}
	my $geneId = "noGeneId";
	my $transcriptId = "noTranscriptId";
	my $otherFields = "";
	for (my $i = 0; $i < $field9Items; ++$i) {
	    my ($noteType, $rest) = split('=',$field9[$i],2);
	    $noteCategories{$noteType} += 1;
	    if ($field9[$i] =~ m/^Parent=/) {
		my ($id, $name) = split('=',$field9[$i]);
		if (!exists($geneNames{$name})) {
		    printf STDERR "CDS with no 'gene' name: '$name'\n";
		}
		$geneId = $name;
		$exonCount{$name} += 1;
	    } elsif ($field9[$i] =~ m/^Name=/) {
		my ($id, $name) = split('=',$field9[$i]);
		$transcriptId = $name;
	    } else {
		my ($id, $name) = split('=',$field9[$i]);
		if (!defined($name) || length($name)<1) {
		    printf STDERR "#\tundefined name for id: '$id' at line\n";
		    printf STDERR "# '%s'\n", $line;
		} else {
		    $otherFields .= " $id \"";
		    my $decoded = decode($name);
		    $decoded =~ s/;/ /g;
		    $otherFields .= $decoded;
		    $otherFields .= '";';
		    $noteTypes{$id} += 1;
		    if ($id =~ m/^Note$/) {
			printf NT "%s\t%s\t%s\n", $geneId, $fields[2],$decoded;
		    }
		}
	    }
	}
	if ($geneId =~ m/noGeneId/) {
	    printf STDERR "no geneId found: %s\n", $line;
	    exit 255;
	}
	if ($transcriptId =~ m/noTranscriptId/) {
	    printf STDERR "no transcriptId found: %s\n", $line;
	    exit 255;
	}
	if (length($otherFields)>0) {
	    printf "gene_id \"$geneId\"; transcript_id \"$transcriptId\";%s\n", $otherFields;
	} else {
	    printf "gene_id \"$geneId\"; transcript_id \"$transcriptId\";\n";
	}
    } else {
	printf OT "%s\n", $line;
    }
#    print $line;
}

close (FH);
close (OT);
close (NT);

printf STDERR "#\tfound $geneCount gene identifiers\n";
printf STDERR "############################ multiple exons ################\n";
foreach my $key (keys %exonCount) {
    if ($exonCount{$key} > 1) {
	printf STDERR "#\tmultiple exons: $key: $exonCount{$key}\n";
    }
}
printf STDERR "############################ note types ####################\n";
foreach my $key (keys %noteTypes) {
    printf STDERR "#\tnote:'%s' count:'%d'\n", $key, $noteTypes{$key};
}
printf STDERR "#################  note types done  ########################\n";

printf STDERR "############################ all note categories ############\n";
foreach my $key (keys %noteCategories) {
    printf STDERR "#\tnote:'%s' count:'%d'\n", $key, $noteCategories{$key};
}

printf STDERR "#################  all done  ###########################\n";


__END__

chrI	SGD	gene	335	649	.	+	.	ID=YAL069W;Name=YAL069W;Ontology_term=GO:0003674,GO:0005575,GO:0008150;Note=Dubious%20open%20reading%20frame%20unlikely%20to%20encode%20a%20protein%2C%20based%20on%20available%20experimental%20and%20comparative%20sequence%20data;dbxref=SGD:S000002143;orf_classification=Dubious
chrI	SGD	CDS	335	649	.	+	0	Parent=YAL069W;Name=YAL069W;Ontology_term=GO:0003674,GO:0005575,GO:0008150;Note=Dubious%20open%20reading%20frame%20unlikely%20to%20encode%20a%20protein%2C%20based%20on%20available%20experimental%20and%20comparative%20sequence%20data;dbxref=SGD:S000002143;orf_classification=Dubious

############################ all note categories ############
#       note:'Parent' count:'7077'
#       note:'Name' count:'7077'
#       note:'gene' count:'5143'
#       note:'Note' count:'7077'
#       note:'dbxref' count:'7077'
#       note:'orf_classification' count:'6923'
#       note:'Alias' count:'5271'
#       note:'Ontology_term' count:'7077'
#################  all done  ###########################

