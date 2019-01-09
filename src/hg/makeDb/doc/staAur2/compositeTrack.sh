#!/bin/bash

printf "track aaCounts
compositeTrack on
shortLabel AA counts
longLabel amino acid profile
subGroup1 view Views aliHydro=Aliphatic_Hydrophic aroHydro=Aromatic_Hydrophic neut=Neutral pos=Positive neg=Negative unq=Unique
dragAndDrop subTracks
visibility dense
group compGeno
priority 4
color 0,0,128
altColor 128,0,0
type bigBed 9 +
mouseOverField popUp
itemRgb on
labelOnFeature off
sortOrder view=+
html aaCounts

"

printf "    track aaCountAliHydro
    shortLabel Aliphatic Hydrophobic
    view aliHydro
    visibility dense
    parent aaCounts

"

for N in ALA_A ILE_I LEU_L VAL_V
do
  aaa=`echo $N | sed -e 's/_.//;'`
  aa=`echo $N | sed -e 's/.*_//;'`
  printf "        track aaCounts${aaa}
        parent aaCountAliHydro
        subGroups view=aliHydro
        shortLabel ${aaa} - ${aa}
        longLabel ${aaa} - ${aa} amino acid profile 
        type bigBed 9 +
        bedNameLabel ${aaa} - ${aa}

"
done

printf "    track aaCountAroHydro
    shortLabel Aromatic Hydrophobic
    view aroHydro
    visibility dense
    parent aaCounts

"

for N in PHE_F TRP_W TYR_Y
do
  aaa=`echo $N | sed -e 's/_.//;'`
  aa=`echo $N | sed -e 's/.*_//;'`
  printf "        track aaCounts${aaa}
        parent aaCountAroHydro
        subGroups view=aroHydro
        shortLabel ${aaa} - ${aa}
        longLabel ${aaa} - ${aa} amino acid profile 
        type bigBed 9 +
        bedNameLabel ${aaa} - ${aa}

"
done

printf "    track aaCountNeutral
    shortLabel Neutral
    view neut
    visibility dense
    parent aaCounts

"

for N in ASN_N CYS_C GLN_Q MET_M SER_S THR_T
do
  aaa=`echo $N | sed -e 's/_.//;'`
  aa=`echo $N | sed -e 's/.*_//;'`
  printf "        track aaCounts${aaa}
        parent aaCountNeutral
        subGroups view=neut
        shortLabel ${aaa} - ${aa}
        longLabel ${aaa} - ${aa} amino acid profile 
        type bigBed 9 +
        bedNameLabel ${aaa} - ${aa}

"
done

printf "    track aaCountPositive
    shortLabel Positive
    view pos
    visibility dense
    parent aaCounts

"

for N in ARG_R HIS_H LYS_K
do
  aaa=`echo $N | sed -e 's/_.//;'`
  aa=`echo $N | sed -e 's/.*_//;'`
  printf "        track aaCounts${aaa}
        parent aaCountPositive
        subGroups view=pos
        shortLabel ${aaa} - ${aa}
        longLabel ${aaa} - ${aa} amino acid profile 
        type bigBed 9 +
        bedNameLabel ${aaa} - ${aa}

"
done

printf "    track aaCountNegative
    shortLabel Negative
    view neg
    visibility dense
    parent aaCounts

"

for N in ASP_D GLU_E
do
  aaa=`echo $N | sed -e 's/_.//;'`
  aa=`echo $N | sed -e 's/.*_//;'`
  printf "        track aaCounts${aaa}
        parent aaCountNegative
        subGroups view=neg
        shortLabel ${aaa} - ${aa}
        longLabel ${aaa} - ${aa} amino acid profile 
        type bigBed 9 +
        bedNameLabel ${aaa} - ${aa}

"
done

printf "    track aaCountUnique
    shortLabel Unique
    view unq
    visibility dense
    parent aaCounts

"

for N in GLY_G PRO_P
do
  aaa=`echo $N | sed -e 's/_.//;'`
  aa=`echo $N | sed -e 's/.*_//;'`
  printf "        track aaCounts${aaa}
        parent aaCountUnique
        subGroups view=unq
        shortLabel ${aaa} - ${aa}
        longLabel ${aaa} - ${aa} amino acid profile 
        type bigBed 9 +
        bedNameLabel ${aaa} - ${aa}

"
done

exit $?

# Aliphatic Hydrophobic
$aaIndex{'A'} = 0;	# ALA Alanine
$aaIndex{'I'} = 1;	# ILE Isoleucine
$aaIndex{'L'} = 2;	# LEU Leucine
$aaIndex{'V'} = 3;	# VAL Valine
# Aromatic Hydrophobic
$aaIndex{'F'} = 4;	# PHE Phenylalanine
$aaIndex{'W'} = 5;	# TRP Tryptophan
$aaIndex{'Y'} = 6;	# TYR Tyrosine
# Neutral side chain
$aaIndex{'N'} = 7;	# ASN Asparagine
$aaIndex{'C'} = 8;	# CYS Cysteine
$aaIndex{'Q'} = 9;	# GLN Glutamine
$aaIndex{'M'} = 10;	# MET Methionine
$aaIndex{'S'} = 11;	# SER Serine
$aaIndex{'T'} = 12;	# THR Threonine
# Positive side chain
$aaIndex{'R'} = 13;	# ARG Arginine
$aaIndex{'H'} = 14;	# HIS Histidine
$aaIndex{'K'} = 15;	# LYS Lysine
# Negative side chain
$aaIndex{'D'} = 16;	# ASP Aspartic Acid
$aaIndex{'E'} = 17;	# GLU Glutamic Acid
# Unique
$aaIndex{'G'} = 18;	# GLY Glycine
$aaIndex{'P'} = 19;	# PRO Proline

