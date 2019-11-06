# write rename script to STDOUT, and trackDb to trackDb.lines

open my $fh, '>', 'trackDb.chromHmm.lines';
my $track = "encode3RenChromHmm";
my $path = "/gbdb/mm10/encode3/chromHmm";

print $fh "track $track";
print $fh "|compositeTrack on";
print $fh "|longLabel Chromatin state of embryonic tissue from ENCODE 3 (UCSD/LICR)";
print $fh "|shortLabel Chromatin State";
print $fh "|type bigBed 9";
print $fh "|itemRgb on";
print $fh "|dragAndDrop subTracks";
print $fh "|dimensions dimX = tissue dimY=age";
print $fh "|subGroup1 tissue Tissue forebrain=forebrain midbrain=midbrain hindbrain=hindbrain neural=neural facial=facial limb=limb heart=heart kidney=kidney lung=lung liver=liver stomach=stomach intestine=intestine";
print $fh "|subGroup2 age Age E10=E10 E11=E11 E12=E12 E13=E13 E14=E14 E15=E15 E16=E16 P0=P0";
print $fh "|sortOrder tissue=+ age=+";
print $fh "\n\n";

while (<>) {
    chomp;
    print $_ . "\t";
    my ($age, $tissue, $a, $b) = split('_');
    $age =~ s/\.5//;
    $tissue =~ s/facial-prominence/facial/;
    $tissue =~ s/neural-tube/neural/;
    my $subtrack = $track . ucfirst($tissue) . ucfirst($age);
    print "$subtrack.bb\n";

    print $fh "track $subtrack";
    print $fh "|bigDataUrl $path/$subtrack.bb";
    print $fh "|parent $track off";
    print $fh "|shortLabel ChromHMM $tissue $age";
    print $fh "|longLabel Chromatin state of embryonic day $age $tissue from ENCODE 3 (UCSD/LICR)";
    print $fh "|subGroups tissue=$tissue age=$age";
    print $fh "\n";
}
close $fh;
