#!/usr/bin/perl -w
# Digest BGI files into bgiGeneInfo and bgiSnp tables.
# Use SQL INSERT statements for bgiGeneInfo because of longblobs;
# tab-sep file will do for bgiSnp.

use Carp;
use strict;

my $infoOut = 'bgiGeneInfo.sql';
my $snpOut = 'bgiSnp.bed';
my $geneSnpOut = 'bgiGeneSnp.tab';

my $strainExpr = '(Broiler|Layer|Silkie)';
my $geneSrcExpr = '(GenBank_complete|GenBank_partial|HumanDiseaseGene|IMS-GSF|UMIST\(orf lessthan 300\)|UMIST\(orf morethan 300\)|UMIST|Ensembl|Hans_Cheng|Leif_Andersson|Martien_Groenen)';
my %geneSrcAbbrev = ('GenBank_complete' => 'GBC',
		     'GenBank_partial' => 'GBP',
		     'HumanDiseaseGene' => 'HDG',
		     'IMS-GSF' => 'IG',
		     'UMIST(orf lessthan 300)' => 'UM',
		     'UMIST(orf morethan 300)' => 'UM',
		     'UMIST' => 'UM',
		     'Ensembl' => 'ENS',
		     'Hans_Cheng' => 'HC',
		     'Leif_Andersson' => 'LA',
		     'Martien_Groenen' => 'MG',
		    );

# 2 args:  file containing list of SNPlist-chr*.txt[.gz] files, and 
# file containing list of GeneSNPcon*.txt[.gz] files.  
if (scalar(@ARGV) != 2) {
  die "Usage: $0 SNPlists.list GeneSNPcons.list\n";
}
my $SNPlists = shift @ARGV;
my $GeneSNPlists = shift @ARGV;


#######################################################################
# SUBs

# SNP position is either a single 1-based coord or 1-based start,end
sub parsePos {
  my $pos = shift;
  my ($start, $end);
  if ($pos =~ /^(\d+),(\d+)$/) {
    $start = $1 - 1;
    $end = $2;
  } elsif ($pos =~ /^\d+$/) {
    $start = $pos - 1;
    $end = $pos;
  } else {
    confess "Bad pos $pos";
  }
  return($start, $end);
}

# 3-character code for whether each strain has SNP, doesn't, or wasn't covered:
sub parseStrains {
  my $strains = shift;
  my ($inB, $inL, $inS);
  if ($strains =~ /^([B\-X])([L\-X])([S\-X])$/) {
    $inB = &parseStrain($1);
    $inL = &parseStrain($2);
    $inS = &parseStrain($3);
  } else {
    confess "Bad strains $strains";
  }
  return($inB, $inL, $inS);
}
sub parseStrain {
  my $strain = shift;
  if ($strain eq '-') {
    return('no');
  } elsif ($strain eq 'X') {
    return('n/a');
  } else {
    return('yes');
  }
}

# Since we have GO in a db, pare down to just the numeric IDs:
sub parseGo {
  my $line = shift;
  return undef if (! defined $line);
  while ($line =~ s/GO:(\d+): [^;]*;/$1,/) {}
  return($line);
}

# Some Splice site annotations have duplicate Intron annotations; ignore.
my %ignoreAssocs = ("GBC.AF330009	snp.11.169.100039.D.3	Intron" => 1,
		    "GBC.AY434090	snp.13.273.21715.D.2	Intron" => 1,
		    "GBC.CHKRPK1	snp.3.1043.108926.S.3	Intron" => 1,
		    "GBP.GGA278103	snp.40.83.30594.D.3	Intron" => 1,
		    "GBP.GGCD8BMR	snp.84.35.2761.D.3	Intron" => 1,
		    "GBP.GGSCII	snp.129.135.3889.S.1	Intron" => 1,
		    "HDG.ENST0220888.1	snp.91.32.10037.I.3	Intron" => 1,
		    "HDG.ENST0243222.1	snp.0.76.46742.S.2	Intron" => 1,
		    "HDG.ENST0243222.1	snp.0.76.46742.S.2	Intron" => 1,
		    "HDG.ENST0259463.1	snp.24.85.74842.S.2	Intron" => 1,
		    "HDG.ENST0265312.3	snp.35.126.134274.S.2	Intron" => 1,
		    "HDG.ENST0265312.3	snp.35.126.134274.S.3	Intron" => 1,
		    "HDG.ENST0265517.1	snp.343.17.13202.S.1	Intron" => 1,
		    "HDG.ENST0265517.1	snp.343.17.13202.S.1	Intron" => 1,
		    "HDG.ENST0272425.1	snp.41.264.34914.S.1	Intron" => 1,
		    "HDG.ENST0272425.1	snp.41.264.34914.S.1	Intron" => 1,
		    "HDG.ENST0273049.2	snp.35.126.134274.S.2	Intron" => 1,
		    "HDG.ENST0273049.2	snp.35.126.134274.S.3	Intron" => 1,
		    "HDG.ENST0306737.4	snp.23.381.16095.S.1	Intron" => 1,
		    "HDG.ENST0306737.4	snp.23.381.16095.S.1	Intron" => 1,
		    "HDG.ENST0306737.4	snp.23.381.16095.S.2	Intron" => 1,
		    "HDG.ENST0306737.4	snp.23.381.16095.S.2	Intron" => 1,
		    "HDG.ENST0306805.2	snp.158.95.1563.I.3	Intron" => 1,
		    "HDG.ENST0315357.3	snp.23.381.16095.S.1	Intron" => 1,
		    "HDG.ENST0315357.3	snp.23.381.16095.S.1	Intron" => 1,
		    "HDG.ENST0315357.3	snp.23.381.16095.S.2	Intron" => 1,
		    "HDG.ENST0315357.3	snp.23.381.16095.S.2	Intron" => 1,
		    "HDG.ENST0318471.1	snp.54.44.40605.S.3	Intron" => 1,
		    "HDG.ENST0318471.1	snp.54.44.40605.S.3	Intron" => 1,
		    "HDG.ENST0318662.2	snp.11.169.100039.D.3	Intron" => 1,
		    "HDG.ENST0318662.2	snp.11.169.100039.D.3	Intron" => 1,
		    "HDG.ENST0334149.2	snp.62.137.12320.S.1	Intron" => 1,
		    "HDG.ENST0334149.2	snp.62.137.12320.S.1	Intron" => 1,
		    "HDG.ENST0336463.1	snp.18.589.34206.D.1	Intron" => 1,
		    "HDG.ENST0336828.1	snp.35.126.134274.S.2	Intron" => 1,
		    "HDG.ENST0336828.1	snp.35.126.134274.S.3	Intron" => 1,
		    "HDG.ENST0337218.1	snp.23.381.16095.S.1	Intron" => 1,
		    "HDG.ENST0337218.1	snp.23.381.16095.S.1	Intron" => 1,
		    "HDG.ENST0337218.1	snp.23.381.16095.S.2	Intron" => 1,
		    "HDG.ENST0337218.1	snp.23.381.16095.S.2	Intron" => 1,
		    "HDG.ENST0337851.1	snp.66.124.30871.S.2	Intron" => 1,
		    "HDG.ENST0337851.1	snp.66.124.30871.S.2	Intron" => 1,
		    "HDG.ENST0337851.1	snp.66.124.30872.D.1	Intron" => 1,
		    "HDG.ENST0338081.1	snp.11.169.100039.D.3	Intron" => 1,
		    "HDG.ENST0338081.1	snp.11.169.100039.D.3	Intron" => 1,
		    "IG.riken1_14g12	snp.153.70.10298.S.2	Intron" => 1,
		    "IG.riken1_2i19	snp.10.221.108177.S.3	Intron" => 1,
		    "IG.riken1_6l10	snp.14.28.57558.I.3	Intron" => 1,
		    "UM.BX929831	snp.265.71.21998.S.1	Intron" => 1,
		    "UM.BX933790	snp.103.61.11006.D.3	Intron" => 1,
		    "UM.BX934145	snp.5.532.89195.S.1	Intron" => 1,
		    "UM.BX935596	snp.32.191.7074.S.1	Intron" => 1,
		    "UM.BX930936	snp.9.461.7584.D.2	Intron" => 1,
		    "UM.BX931260	snp.10.218.129574.D.3	Intron" => 1,
		    "HC.bgf_chr4_909	snp.3.661.14775.S.1	Intron" => 1,
		    "HC.bgf_chr4_909	snp.3.661.14775.S.2	Intron" => 1,
		    "LA.riken1_2i19	snp.10.221.108177.S.3	Intron" => 1,
		    "MG.bgf_chr2_1915	snp.264.6.522.S.2	Intron" => 1,
		    "MG.bgf_chr2_2004	snp.21.141.15231.S.1	Intron" => 1,
		    "MG.bgf_chr2_2096_conform	snp.21.301.16681.S.3	Intron" => 1,
		   );

my %assocs = ();
sub gsOut {
  confess "Wrong #words to gsOut" if ($#_ != 5);
  if (! defined $ignoreAssocs{join("\t", $_[0], $_[1], $_[2])}) {
    my $line = join("\t", @_);
    print GS "$line\n";
    my $assoc = $_[0] . "\t" . $_[1];
    if (defined $assocs{$assoc} && $assocs{$assoc} ne $line) {
      print STDERR "dup assoc $assoc:\n  $assocs{$assoc}\n  $line\n";
    }
    $assocs{$assoc} = $line;
  }
}

#######################################################################
# MAIN

#
# Get core info about SNPs from the files listed in $SNPlists,
# dump out $snpOut as we go.
#
my %snpIds = ();
open(L, "$SNPlists") || die "Can't open $SNPlists: $!\n";
open(SNP, ">$snpOut") || die "Can't open $snpOut for writing: $!\n";
while (<L>) {
  chop;  my $fname = $_;
  if ($fname =~ m@$strainExpr.*SNPlist-(chr\w+)\.txt(.*)$@) {
    my ($strain, $chr, $gz) = ($1, $2, $3);
    $chr =~ s/_[A-Z]$//;
    if ($gz ne "") {
      open(IN, "gunzip -c $fname|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    while (<IN>) {
      chop;
      my @words = split(/\t/);
      if (scalar(@words) != 18) {
	die "Wrong #words (" .scalar(@words). ") for line $. of $fname:\n$_\n";
      }
      my ($snpId, $snpType, $pos, $rPos, $qual, $rQual, $change,
	  $flankL, $flankR, $rName, $readDir, $strains, $primerL, $primerR,
	  $ampSize, $distL, $conf, $extra)
	= @words;
      my ($start, $end) = &parsePos($pos);
      my ($rStart, $rEnd) = &parsePos($rPos);
      my ($inB, $inL, $inS) = &parseStrains($strains);
      $readDir = ($readDir eq 'F') ? '+' : '-';
      $extra = "" if (! defined $extra);
      if (defined $snpIds{$snpId}) {
	die "Duplicate def for $snpId, line $. of $fname:\n$_\n";
      }
      $snpIds{$snpId} = 1;
      print SNP "$chr\t$start\t$end\t$snpId\t$snpType\t$rStart\t$rEnd\t" .
        "$qual\t$rQual\t$change\t" .
        "$rName\t$readDir\t$inB\t$inL\t$inS\t$primerL\t" .
	"$primerR\t$conf\t$extra\n";
    }
    close(IN);
  } else {
    die "Can't get strain info and/or chrom from filename $fname\n";
  }
}
close(L);
close(SNP);

#
# Get gene info and SNP-gene relationships from the files listed in 
# GeneSNPlists.  Dump out $infoOut and $geneSnpOut as we go.
#
my %gene2src = ();
open(L, "$GeneSNPlists") || die "Can't open $GeneSNPlists: $!\n";
open(INFO, ">$infoOut") || die "Can't open $infoOut for writing: $!\n";
open(GS, ">$geneSnpOut") || die "Can't open $geneSnpOut for writing: $!\n";
while (<L>) {
  chop;  my $fname = $_;
  if ($fname =~ m@$geneSrcExpr.*\.txt(.*)$@) {
    my ($geneSrc, $gz) = ($1, $2);
    my $abbrev = $geneSrcAbbrev{$geneSrc};
    die "Can't abbreviate $geneSrc" if (! defined $abbrev);
    if ($gz ne "") {
      open(IN, "gunzip -c '$fname'|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    my ($geneName, $chr, $start, $end, $strand, $pctId, $go, $ipr);
    while (<IN>) {
      chop;
      next if (/^\s*$/);
      my @words = split("\t");
      if ($words[0] eq "GN") {
	if (defined $geneName) {
	  print INFO "INSERT INTO bgiGeneInfo VALUES ( \"$geneName\", \"$geneSrc\", \"$go\", \"$ipr\");\n";
	  $chr = $start = $end = $strand = $pctId = $go = $ipr = undef;
	}
	($geneName, $go, $ipr) = ($words[1], &parseGo($words[2]), $words[3]);
	$geneName =~ s/ENST0000(.*)\|ENSG.*/ENST$1/;
	$geneName = $abbrev . "." . $geneName;
	$go = "" if (! defined $go);
	$ipr = "" if (! defined $ipr);
	if (defined $gene2src{$geneName}) {
	  print STDERR "dup:\t$geneName\t$gene2src{$geneName}\t$geneSrc\n"
	}
	$gene2src{$geneName} = $geneSrc;
      } elsif ($words[0] eq "GP") {
	(undef, $chr, $start, $end, $strand, $pctId) = @words;
	$start -= 1;
      } elsif ($words[0] eq "S5") {
	my $snpId = $words[1];
	&gsOut($geneName, $snpId, "5' upstream", "", "", "");
      } elsif ($words[0] eq "S3") {
	my $snpId = $words[1];
	&gsOut($geneName, $snpId, "3' downstream", "", "", "");
      } elsif ($words[0] eq "U5") {
	my $snpId = $words[1];
	&gsOut($geneName, $snpId, "5' UTR", "", "", "");
      } elsif ($words[0] eq "U3") {
	my $snpId = $words[1];
	&gsOut($geneName, $snpId, "3' UTR", "", "", "");
      } elsif ($words[0] eq "CS") {
	my ($snpId, $ccNuc, $ccPept, $phase) = ($words[1], $words[8],
						$words[9], $words[10]);
	my $cc = "$ccNuc ($ccPept)";
	&gsOut($geneName, $snpId, "Coding (Synonymous)", $cc, $phase, "");
      } elsif ($words[0] eq "CN") {
	my ($snpId, $ccNuc, $ccPept, $phase) = ($words[1], $words[8],
						$words[9], $words[10]);
	my $cc = "$ccNuc ($ccPept)";
	my $sift = "";
	$sift  =  $words[19] if (defined $words[19]);
	$sift .= " $words[20]" if (defined $words[20]);
	$sift .= " $words[21]" if (defined $words[21]);
	&gsOut($geneName, $snpId, "Coding (Nonsynonymous)", $cc, $phase,
	       $sift);
      } elsif ($words[0] eq "CF") {
	my $snpId = $words[1];
	&gsOut($geneName, $snpId, "Coding (Frameshift)", "", "", "");
      } elsif ($words[0] eq "IR") {
	my $snpId = $words[1];
	&gsOut($geneName, $snpId, "Intron", "", "", "");
      } elsif ($words[0] eq "SS") {
	my ($snpId, $cc) = ($words[1], $words[8]);
	$cc = "" if ($cc eq "SSIndels");
	&gsOut($geneName, $snpId, "Splice Site", $cc, "", "");
      } else {
	die "Unrecognized first word $words[0] line $. of $fname:\n$_\n";
      }
    }
    if (defined $geneName) {
      print INFO "INSERT INTO bgiGeneInfo VALUES ( \"$geneName\", \"$geneSrc\", \"$go\", \"$ipr\");\n";
    }
    close(IN);
  } else {
    die "Can't get gene source info from filename $fname\n";
  }
}
close(L);
close(INFO);
close(GS);

