#!/usr/bin/perl -w
# Extract phenotype-allele relationships from flatfile.
# Phenotype MP:* codes are translated to names using filenames of
# phenotype files -- a hack but it works.

use strict;

my %jax2ucsc = ('adipose_tissue' => 'Adipose',
		'behavior_neurological' => 'Behavior',
		'cardiovascular_system' => 'Cardiovascular',
		'cellular' => 'Cellular',
		'craniofacial' => 'Craniofacial',
		'digestive_alimentary' => 'Digestive',
		'embryogenesis' => 'Embryogenesis',
		'endocrine_exocrine_gland' => 'Gland',
		'growth_size' => 'Growth Size',
		'hearing_ear' => 'Hearing/Ear',
		'hematopoietic_system' => 'Hematopoietic',
		'homeostasis_metabolism' => 'Homeostasis',
		'immune_system' => 'Immune',
		'integument' => 'Integument',
		'lethality-embryonic_perinatal' => 'Embryonic Lethal',
		'lethality-postnatal' => 'Postnatal Lethal',
		'life_span-post-weaning_aging' => 'Life Span',
		'limbs_digits_tail' => 'Limbs and Tail',
		'liver_biliary_system' => 'Liver and Bile',
		'mortality' => 'Mortality',
		'muscle' => 'Muscle',
		'nervous_system' => 'Nervous System',
		'no_phenotypic_analysis' => 'n/a',
		'normal' => 'Normal',
		'other' => 'Other',
		'pigmentation' => 'Pigmentation',
		'renal_urinary_system' => 'Renal/Urinary',
		'reproductive_system' => 'Reproductive',
		'respiratory_system' => 'Respiratory',
		'skeleton' => 'Skeleton',
		'skin_coat_nails' => 'Skin/Coat/Nails',
		'taste_olfaction' => 'Taste/Smell',
		'touch_vibrissae' => 'Touch',
		'tumorigenesis' => 'Tumorigenesis',
		'vision_eye' => 'Vision/Eye',
	       );

# Look for MP* files, map codes to names:
my %mp2name;
opendir(D, '.') || die;
foreach my $f (readdir(D)) {
  next if ($f !~ /^MP_(\d+)_([\w-]+)/);
  my ($code, $name) = ("MP:$1", $2);
  $mp2name{$code} = $jax2ucsc{$name};
}
closedir(D);

# Get mapping of MGI ID to allele names (most lines of
# MGI_PhenotypicAllele.rpt include enough info to reconstruct allele
# names -- but not all lines).
my %mgiId2allele;
my %allele2mgiId;
open(F, "jaxAlleleInfo.tab") || die;
while (<F>) {
  my ($allele, $mgiId) = split;
  $mgiId2allele{$mgiId} = $allele;
  $allele2mgiId{$allele} = $mgiId;
}
close(F);

open(E, ">err") || die;

# Now extract the relationships:
while (<>) {
  next if (/^#/);
  chomp;  my @w = split("\t");
  my ($mgiId, $alleleSuffix, $gene, $mps) = ($w[0], $w[1], $w[7], $w[9]);
  next if (! $mps);
  my @phenos = grep {s/^(MP:\d+)$/$mp2name{$1}/ || die;} split(",", $mps);
  next if ((scalar(@phenos) == 1) && $phenos[0] eq 'n/a');
  my $allele;
  if ($gene) {
    $allele = "${gene}_$alleleSuffix";
    my $mgiIdFromInfo = $allele2mgiId{$allele};
    if (! $mgiIdFromInfo) {
      print E "MGI ID for $allele not found in gff\n";
      next;
    } elsif ($mgiIdFromInfo ne $mgiId) {
      print E "MGI ID mismatch for $allele: gff has $mgiIdFromInfo, rpt has $mgiId\n";
    }
  } else {
    print E "No gene for $alleleSuffix.\n";
    $allele = $mgiId2allele{$mgiId};
  }
  if (! $allele) {
    print E "No allele for $mgiId\n";
    next;
  }
  my $transcript = $allele;
  $transcript =~ s/\<\S+\>$// || print E "funny allele $allele\n";
  print join("\t", $allele, $transcript, join(",", @phenos)) . "\n";
}
close(E);
