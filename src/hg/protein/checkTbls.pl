#!/usr/local/bin/perl -w

use strict;

my @kgTbls = ("cgapAlias", "cgapBiocDesc", "cgapBiocPathway",
	"dupSpMrna", "keggMapDesc", "keggPathway", "kgAlias",
	"kgProtAlias", "kgXref", "knownGene", "knownGeneLink",
	"knownGeneMrna", "knownGenePep", "mrnaRefseq", "spMrna");

my @kgTempTbls = ("geneName", "keggList", "knownGene", "knownGene0",
	"locus2Acc0", "locus2Ref0", "mrnaGene", "pepSequence",
	"productName", "refGene", "refLink", "refMrna", "refPep", "spMrna");

my $argc = @ARGV;

if ($argc < 1) {
	print "usage: $0 <db> [kg|kgTemp]\n";
	print "\t<db> - database to dump all tables\n";
	print "\t[kg|kgTemp] - limit dump to only knownGene tables (kg)\n";
	print "\t\tor the tables that should be in the kgTemp database\n";
	exit 255;
}

my $db = shift;

my $tblList = "";

if ($argc > 1) {
	$tblList = shift;
}

my @tbls = @kgTbls;

if ($tblList =~ /kgTemp/) {
	@tbls = @kgTempTbls;
} else { if ($tblList !~ /kg/) {
	@tbls = split('\s', `hgsql -e "show tables;" $db`);
    }
}

my $tblCount = @tbls;

for (my $i = 0; $i < $tblCount; ++$i) {
	if ($tbls[$i] !~ /Tables_in_/) {
		my $c = `hgsql -e "select count(*) from $tbls[$i];" $db 2> /dev/null | tail -1`;
		chomp $c;
		if (length($c) > 0) {
			printf "%3d %s %s\n", $i, $tbls[$i], $c;
		} else {
			printf "%3d %s TBL DOES NOT EXIST\n", $i, $tbls[$i];
		}
	}
}

