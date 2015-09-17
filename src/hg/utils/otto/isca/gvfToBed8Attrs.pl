#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/gvfToBed8Attrs.pl instead.

while (<>) {
  next if (/^(#|browser|track)/); chomp;
  ($c, undef, $varType, $s, $e, $score, $strand, undef, $attrs) = split("\t");
  ($thickS, $thickE) = ($s, $e);
  ($attrCount, $attrTags, $attrVals, $gotVarTypeAttr) = (0, "", "", 0);
  foreach my $tagVal (split(";", $attrs)) {
    ($tag, $val) = split("=", $tagVal);
    if ($tag eq "Name") {
      $name = $val;
    } elsif ($tag eq "Start_range") {
      ($outerStart, $innerStart) = split(",", $val);
      $s = $outerStart if ($outerStart =~ /^\d+$/ && $outerStart < $s);
      $thickS = $innerStart if ($innerStart =~ /^\d+$/ && $innerStart > $thickS);
    } elsif ($tag eq "End_range") {
      ($innerEnd, $outerEnd) = split(",", $val);
      $thickE = $innerEnd if ($innerEnd =~ /^\d+$/ && $innerEnd < $thickE);
      $e = $outerEnd if ($outerEnd =~ /^\d+$/ && $outerEnd > $e);
    } elsif ($tag eq "var_type") {
      $gotVarTypeAttr = 1;
    }
    # Escape commas so the val list parses correctly in autoSql:
    $val =~ s/%/%25/g;   $val =~ s/,/%2C/g;
    $attrTags .= "$tag,";  $attrVals .= "$val,";  $attrCount++;
  }
  next if ($name =~ /^[ne]sv\d/); # keep only supporting variants (ssv)
  if (! $gotVarTypeAttr) {
    $varType =~ s/%/%25/g;   $varType =~ s/,/%2C/g;
    $attrTags .= "var_type,";  $attrVals .= "$varType,";  $attrCount++;
  }
  $score = 0 if ($score eq ".");
  @bed8Attrs = ($c, $s-1, $e, $name, $score, $strand, $thickS-1, $thickE,
		$attrCount, $attrTags, $attrVals);
  print join("\t", @bed8Attrs) . "\n";
}
