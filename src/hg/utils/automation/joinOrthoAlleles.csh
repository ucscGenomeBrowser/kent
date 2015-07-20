#!/bin/csh -ef

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/joinOrthoAlleles.csh instead.

set chimpFile = $1
set orangFile = $2
set macFile = $3
set outFile = $4

if ($outFile == "") then
    echo "usage:"
    echo "joinOrthoAlleles.csh chimpFile orangFile macFile outFile"
    echo ""
    echo "This is intended for use by the 'join' step of doDbSnpOrthoAlleles.pl."
    echo "chimpFile, orangFile and macFile are the result of liftOver of human SNPs"
    echo "to chimp, orangutan and macaque."
    echo "This joins the files by their specially formatted names and unpacks those"
    echo "names into output columns in outFile."
    exit 1
endif

set tmpFile = `mktemp`

# Use the glommed name field as a key to join up chimp, orang and macaque
# allele data.  Include glommed name from both files because if only
# file 2 has a line for the key in 2.1, then 1.1 is empty.  Then plop
# in the orthoGlom fields from each file, which are in the same order
# as the chimp and macaque columns of hg18.snp128OrthoPanTro2RheMac2.
join -o '1.1 2.1 1.2 1.3 1.4 1.5 1.6 2.2 2.3 2.4 2.5 2.6' \
  -a 1 -a 2 -e '?' \
  $chimpFile $orangFile \
| awk '{if ($1 != "?") { print $1, $3,$4,$5,$6,$7,$8,$9,$10,$11,$12; } \
        else           { print $2, $3,$4,$5,$6,$7,$8,$9,$10,$11,$12; }}' \
  > $tmpFile
join -o '1.1 2.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 1.10 1.11 2.2 2.3 2.4 2.5 2.6' \
  -a 1 -a 2 -e '?' \
  $tmpFile $macFile \
| perl -wpe 'chomp; \
    ($glom12, $glom3, $o1Chr, $o1Start, $o1End, $o1Al, $o1Strand, \
     $o2Chr, $o2Start, $o2End, $o2Al, $o2Strand, \
     $o3Chr, $o3Start, $o3End, $o3Al, $o3Strand) = split; \
    $glomKey = ($glom12 ne "?") ? $glom12 : $glom3; \
    ($rsId, $hChr, $hStart, $hEnd, $hObs, $hAl, $hStrand) = \
      split(/\|/, $glomKey); \
    $o1Start =~ s/^\?$/0/;  $o2Start =~ s/^\?$/0/;  $o3Start =~ s/^\?$/0/; \
    $o1End   =~ s/^\?$/0/;  $o2End   =~ s/^\?$/0/;  $o3End   =~ s/^\?$/0/; \
    print join("\t", $hChr, $hStart, $hEnd, $rsId, $hObs, $hAl, $hStrand, \
                     $o1Chr, $o1Start, $o1End, $o1Al, $o1Strand, \
                     $o2Chr, $o2Start, $o2End, $o2Al, $o2Strand, \
                     $o3Chr, $o3Start, $o3End, $o3Al, $o3Strand) . "\n"; \
    s/^.*$//;' \
    > $outFile
rm $tmpFile
