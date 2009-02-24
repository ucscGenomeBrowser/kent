#!/usr/bin/env perl

use strict;
use warnings;

##	"$Id: sgdGffToGtf.pl,v 1.4 2009/02/10 20:26:08 hiram Exp $";

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "/sgdGffToGtf.pl - transform SGD gff format to UCSC gtf\n";
    printf STDERR "usage: ./sgdGffToGtf.pl <file.gff>\n";
    printf STDERR "\te.g.: sgdGffToGtf.pl S.cerevisiae.gff\n";
    printf STDERR "also creates files:\n";
    printf STDERR "'otherFeatures.bed', 'leftOvers.gff' and 'descriptions.txt'\n";
    exit 255;
}

#### decode subroutine found at:
####	http://www.troubleshooters.com/codecorn/littperl/perlcgi.htm
sub decode($) {
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

#### hashUp - take all the strings in the array, split them on the '='
####	delimiter, and turn that split into a hash key and value
sub hashUp(@){
    my(@f9) = @{(shift)};
    my $size = scalar(@f9);
    my %hash9;
    for (my $i = 0; $i < $size; ++$i) {
	my ($key, $value) = split('=',$f9[$i],2);
	$hash9{uc($key)} = $value;
    }
    return %hash9;
}

my $file = shift;

my $geneCount = 0;
my %geneNames;
my %exonCount;
my %noteTypes;
my %noteCategories;

open (FH,"<$file") or die "can not open $file";

open (LO,">leftOvers.gff") or die "can not write to leftOvers.gff";
open (OT,"| sort -u | sort -k1,1 -k2,2n > otherFeatures.bed") or die "can not write to otherFeatures.bed";
open (NT,"|sort -u > notes.txt") or die "can not write to notes.txt";
open (DS,"|sort -u > descriptions.txt") or die "can not write to descriptions.txt";

while (my $line = <FH>) {
    next if ($line =~ m/^\s*#/);
    chomp $line;
    my (@fields) = split('\t',"$line");
    next if ($fields[2] eq "chromosome");
    if (scalar(@fields) != 9) {
	printf STDERR "field count != 9 ? at:\n";
	printf STDERR "$line\n";
	exit 255;
    }
    my (@field9) = split(';',$fields[8]);
    my (%field9Hash) = hashUp(\@field9);
    my $field9Items = scalar(@field9);
    if ($fields[2] =~ m/^gene$|^transposable_element_gene$|^pseudogene$/) {
	if (exists($field9Hash{'ID'})) {
	    my $name = $field9Hash{'ID'};
	    if (exists($geneNames{$name})) {
		printf STDERR "duplicate gene name: '$name'\n";
		$geneNames{$name} += 1;
	    } else {
		$geneNames{$name} = 1;
		++$geneCount;
	    }
	}
    } elsif ($fields[2] =~ m/^CDS$|^five_prime_UTR_intron$|^intron$/) {
    	if ($fields[2] =~ m/^five_prime_UTR_intron$/) {
	    $fields[2] = "5utr";
	}
	for (my $i = 0; $i < 8; ++$i) {
	    printf "$fields[$i]\t";
	}
	my $geneId = "noGeneId";
	my $transcriptId = "noTranscriptId";
	my $otherFields = "";
	foreach my $noteType (keys %field9Hash) {
	    $noteCategories{$noteType} += 1;
	}
	if (exists($field9Hash{'PARENT'})) {
	    my $name = $field9Hash{'PARENT'};
	    if (!exists($geneNames{$name})) {
		printf STDERR "CDS with no 'gene' name: '$name'\n";
	    }
	    $geneId = $name;
	    $exonCount{$name} += 1;
	}
	if (exists($field9Hash{'NAME'})) {
	    my $name = $field9Hash{'NAME'};
	    $transcriptId = $name;
	}
	foreach my $key (keys %field9Hash) {
	    next if ($key =~ m/^NAME$|^PARENT$/);
	    my $name = $field9Hash{$key};
	    if (!defined($name) || length($name)<1) {
	     printf STDERR "#\tundefined name for id: '$key' at line\n";
		printf STDERR "# '%s'\n", $line;
	    } else {
		$otherFields .= " $key \"";
		my $decoded = decode($name);
		$decoded =~ s/;/ /g;
		$otherFields .= $decoded;
		$otherFields .= '";';
		$noteTypes{$key} += 1;
		if ($key =~ m/^NOTE$/) {
		 printf DS "%s\t%s\t%s\n", $geneId, $fields[2],$decoded;
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
	printf LO "%s\n", $line;
	my $name = "noName";
	if (exists($field9Hash{'ID'})) {
	    $name = $field9Hash{'ID'};
	} elsif (exists($field9Hash{'PARENT'})) {
	    $name = $field9Hash{'PARENT'};
	}
	my $note = "";
	if (exists($field9Hash{'NOTE'})) {
	    $note = $field9Hash{'NOTE'};
	}
	my $decoded = "";
	if (length($note) > 0) {
	    $decoded = decode($note);
	    $decoded =~ s/;/ /g;
	    $note = $decoded;
	}
	my $alias = "";
	if (exists($field9Hash{'ALIAS'})) {
	    $alias = $field9Hash{'ALIAS'};
	}
	if (length($alias) > 0) {
	    $decoded = decode($alias);
	    $decoded =~ s/;/ /g;
	    $alias = $decoded;
	}
	my $noteLine = "$fields[1], $fields[2]";
	if ((length($alias) > 0) && (length($note) > 0)) {
	    $noteLine = "${alias}, ${note}";
	} elsif (length($alias) > 0) {
	    $noteLine = $alias;
	} elsif (length($note) > 0) {
	    $noteLine = $note;
	}
	if (length($noteLine) > 254) {
	    printf STDERR "WARN: notes > 254 chars $fields[0], $fields[3], $fields[4], $name, $noteLine\n";
	    $noteLine = substr($noteLine, 0, 253);
	}
	printf OT "%s\t%d\t%d\t%s\t0\t%s\t%s\n",
	    $fields[0], $fields[3], $fields[4], $name, $fields[6], $noteLine;
	printf NT "%s\t%s\t%s\n", $name, $fields[2], $noteLine;
    }
#    print $line;
}

close (FH);
close (LO);
close (OT);
close (NT);
close (DS);

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

