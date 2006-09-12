#!/usr/bin/env perl 

# $Id: updateBed.pl,v 1.4 2006/08/30 00:04:06 hiram Exp $

##Author: Yontao Lu
##Date  06/29/03
##
##Desc: update the infobed file for mouse and rat

## This script was rescued from obscurity in /cluster/store5/mouseMarker/code/
## updated extensively by Hiram, August 2006 to make more correct.
##  This script is used during the construction of the mouse STS markers.
##  It works from the previous STS marker file adding in new markers for
##  the next release.

use warnings;
use strict;

my $verbose = 0;

#  global variables used everywhere
my %leftPrimer;		# key is a primer identifier from PRB_PrimerSeq.rpt
my %rightPrimer;	# key is a primer identifier from PRB_PrimerSeq.rpt
my %productSize;	# key is a primer identifier from PRB_PrimerSeq.rpt
my %uniName;		# key is a UniSTS Id sequence # from UniSTS_mouse.sts
			# value is the UniSTS name
my %uniId;		# key is a UniSTS name from UniSTS_mouse.sts
			# value is the UniSTS Id sequence #
			#  !* NOT USED FOR ANYTHING *!
my %mgiId;		# key is a marker name, value is the MGI:number
			# from MRK_Dump2.rpt or PRB_PrimerSeq.rpt
my %mgiName;		# key is MGI:number value is marker name
			# from MRK_Dump2.rpt or PRB_PrimerSeq.rpt
my %mgiAlias;		# key is alias name, value is an alias list
			# from MRK_Sequence.rpt - !* NOT USED FOR ANYTHING *!
my %chr;		# key is MGI:number, value is chrom number
			# from either MRK_Dump2.rpt or PRB_PrimerSeq.rpt
my %cm;			# key is MGI:number
			# from either MRK_Dump2.rpt or PRB_PrimerSeq.rpt
my %alias;		# key is a marker alias name, value is an alias list
			# from UniSTS_mouse.alias
my %gChr;		# key is a UniSTS Id sequence # and
			# Marker Name from 10090.WI_Mouse_Genetic.txt
			# value is chrom number
my %gCm;		# key is a UniSTS Id sequence # and
			# Marker Name from 10090.WI_Mouse_Genetic.txt
			# value is chrom number
my %rhChr;		# key is a UniSTS Id sequence # and
			# Marker Name from 10090.Whitehead-MRC_RH.txt
			# value is chrom number
my %rhCm;		# key is UniSTS Id sequence # and
			# Marker Name from 10090.Whitehead-MRC_RH.txt
			# value is Whitehead RH Coordinate

sub usage()
{
    print STDERR "usage updateBed.pl <orig bed> <mgd map> <mgd primerSeq> <mgd seqInfo>  <unists alias> <unists seq> -r rh_map -g gentic_map [-verbose]
-g: Genetic map (10090.WI_Mouse_Genetic.txt)
-r: RH map   (10090.Whitehead-MRC_RH.txt)
-verbose: extra error printouts on data verifications
	-verbose second time more printouts
Example:
~/kent/src/hg/stsMarkers/updateBed.pl \
        /cluster/data/<previous mouse>/bed/STSmarkers/stsInfoMouse.bed \
        downloads/MRK_Dump2.rpt downloads/PRB_PrimerSeq.rpt \
        downloads/MRK_Sequence.rpt downloads/UniSTS_mouse.alias \
        downloads/UniSTS_mouse.sts \
        -g downloads/10090.WI_Mouse_Genetic.txt \
        -r downloads/10090.Whitehead-MRC_RH.txt \
        -verbose 2> dbg | sed -e 's/\\t*\$//' > newbedfile
\n";
    exit(1);
}

sub updateAlias()
{##This will update alias by the offical name used by ucsc. If unable to find keep original
    my ($nameRef, $sIdRef, $snameRef, $uistsidRef, $aliasRef, $aliasCountRef) = @_;
    my @curNames = ();
    
    ## get all current available name from sts bed
    push(@curNames, $$nameRef);
    push(@curNames, $$uistsidRef);
    push(@curNames, $$sIdRef);
    push(@curNames, $$snameRef);
    my $ttAlias = $$aliasRef;
    my $aliasNum = $$aliasCountRef;
    my $curAlias = $$aliasRef;
    my @curAliases = split(/\;/, $ttAlias); 
    push(@curNames, (@curAliases));
    
    ## get newest alias if available.
    my $i = 0;
    for(; $i < @curNames; $i++)
    {##find alias
	last if(exists($alias{$curNames[$i]}) && ${$alias{$curNames[$i]}} ne "");
    }
    my $newAliasList = "";
    if (defined($curNames[$i])) {
	if (exists($alias{$curNames[$i]})) {
	    $newAliasList = ${$alias{$curNames[$i]}};
	}
    }
    my @all = split(/\;/, $newAliasList);
    my $newUniId = shift(@all);

    ## no newest aliase, keep the old one
    if(@all == 0)
    {
	return @curNames;
    }
    ## if name is in the newest and not in the old one, replace it
    if(($$aliasRef !~ $$nameRef && $newAliasList =~ $$nameRef) || $aliasNum == 0)
    {
	my $newAlias = join("\;", @all);
	$$aliasRef = $newAlias;
	$$aliasCountRef = @all;
   }

    ## update rest elements.
    $$uistsidRef = $newUniId;
    if($$sIdRef eq "")
    {
	$newAliasList =~ /MGI\:\d*/;
	$$sIdRef = $&;
    }
    if($$snameRef eq "" && $$sIdRef ne "")
    {
	$$snameRef = $mgiName{$$sIdRef};
    }
    return($$nameRef, $$sIdRef, $$snameRef, $$uistsidRef, @all);
	
}

sub removePrimerFromHash()
{   
    my @names = @_;   
    for(my $i = 0; $i < @names; $i++)
    {
	if (defined($names[$i])) {
	    delete $leftPrimer{$names[$i]} if exists($leftPrimer{$names[$i]});
	    delete $rightPrimer{$names[$i]} if exists($rightPrimer{$names[$i]});
	    delete $productSize{$names[$i]} if exists($productSize{$names[$i]});
	}
    }
}

sub searchPrimer()
{
    my @names = @_;
    my $left = ""; my $right = ""; my $size="";
    for(my $i = 0; $i < @names; $i++)
    {
	if (defined($names[$i])) {
	    if(exists($leftPrimer{$names[$i]}) &&
		($leftPrimer{$names[$i]} ne ""))
	    {
		$left = $leftPrimer{$names[$i]};
		$right = $rightPrimer{$names[$i]};
		$size = $productSize{$names[$i]};
	    }
	}
    }
    &removePrimerFromHash(@names);
    # return($leftPrimer{$names[$i]}, $rightPrimer{$names[$i]}, $productSize{$names[$i]});
    return($left, $right, $size);
}


sub searchGeneralMap()
{
    my @names = @_;
    for(my $i = 0; $i <@names; $i++) {
	if(defined($names[$i]) && exists($chr{$names[$i]}) && ($chr{$names[$i]} ne ""))
	{
	    return($chr{$names[$i]}, $cm{$names[$i]});
	}
    }
}
sub searchGeneticMap(@)
{
    my @names = @_;
    for(my $i = 0; $i <@names; $i++) {
	if (defined($names[$i])) {
	    if(exists($gChr{$names[$i]}) && ($gChr{$names[$i]} ne ""))
		{
		return($gChr{$names[$i]}, $gCm{$names[$i]});
		}
	}
    }
}
sub  searchRhMap(@)
{
    my @names = @_;
    for(my $i = 0; $i <@names; $i++) {
	if (defined($names[$i])) {
	    if(exists($rhChr{$names[$i]}) && ($rhChr{$names[$i]} ne ""))
		{
		return($rhChr{$names[$i]}, $rhCm{$names[$i]});
		}
	}
    }
}
if(@ARGV < 5 )
{
    &usage();
}

my $rh = 0;
my $rhFile = "";
my $genetic = 0;
my $geneticFile = "";
my @files = ();

while(my $arg = shift)
{
    if($arg eq "-verbose")
    {
	++$verbose;
    }
    elsif($arg eq "-r")
    {
	$rh = 1;
	$rhFile = shift;
    }
    elsif($arg eq "-g")
    {
	$genetic = 1;
	$geneticFile = shift;
    }
    else
    {
	push(@files, $arg);
    }
}

my $bedF = shift(@files);
my $mgdMarkerF = shift(@files);
my $mgdSeqF = shift(@files);
my $mgdAlias = shift(@files);
my $uniAliasF = shift(@files);
my $uniSeqF = shift(@files);


open(BED, "<$bedF") || die "Can't open original bed file: $bedF\n\t$!";
open(MGDM, "<$mgdMarkerF") || die "Can't open mgd marker file: $mgdMarkerF\n\t$!";
open(MGDS, "<$mgdSeqF") || die "Can't open mgd primer seq file: $mgdSeqF\n\t$!";
open(MGDA, "<$mgdAlias") || die "Can't opem mgd seq info file: $mgdAlias\n\t$!";
open(UNIA, "<$uniAliasF") || die "Can't open uni alias file: $uniAliasF\n\t$!";
open(UNIS, "<$uniSeqF") || die "Can't open uni primer seq file: $uniSeqF\n\t$!";
if($rh)
{
    open(RH, "<$rhFile") || die "can't open rh map: $rhFile\n\t$!";
}
if($genetic)
{
    open(GEN, "<$geneticFile") || die "can't open genetic map: $geneticFile\n\t$!";
}

if ($verbose) {print STDERR "reading mgd marker file $mgdMarkerF\n";}
while(my $line = <MGDM>)
{
    chomp($line);
    $line = uc($line);
    my (@eles) = split(/\t/, $line);
    next if($eles[3] < 0);	# there are over 32,000 items < 0.0
    next if ($eles[4] =~ m/UN/);	#	we don't do chrUN
    next if ($eles[4] =~ m/XY/);	#	we don't do chrXY

    #  check if we are stomping on existing mgiName entries
    if (exists($mgiName{$eles[0]}) ) {
	if ($mgiName{$eles[0]} ne $eles[1]) {
	    if ($verbose) {
	    printf STDERR "ERROR: duplicate mgiName key $eles[0] (values $mgiName{$eles[0]} $eles[1]) in $mgdMarkerF at line $.\n";
	    }
	next;
	}
    } else {
	#  check if we are stomping on existing mgiId entries
	if (exists($mgiId{$eles[1]})) {
	    if ($mgiId{$eles[1]} ne $eles[0]) {
		if ($verbose) {
		printf STDERR "ERROR: duplicate migId key $eles[1] (values $mgiId{$eles[1]} $eles[0]) in $mgdMarkerF at line $.\n";
		}
	    }
	    next;
	} else {
	    $eles[4] =~ s/MT/M/;	# chrom name change MT to M
	    $eles[3] =~ s/\s//g;	# remove white space from cm
	    if ($eles[3] =~ m/^0.00$/) { $eles[3] = 0.0; }
	    $mgiName{$eles[0]} = $eles[1];  # eles[0] is the MGI:Id number
	    $mgiId{$eles[1]} = $eles[0];  # eles[1] is the Name field
	    $chr{$eles[0]} = $eles[4];	# eles[4] is the chrom number
	    $cm{$eles[0]} = $eles[3];	# eles[3] is cM distance
	}
    }
}
close(MGDM);

if ($verbose) {print STDERR "reading mgd primer seq file $mgdSeqF\n";}
while(my $line = <MGDS>)
{
    chomp($line);
    $line = uc($line);
    my (@eles) = split(/\t/, $line);
    next if($eles[9] < 0);
    next if ($eles[8] =~ m/UN/);	#	we don't do chrUN
    next if ($eles[8] =~ m/XY/);	#	we don't do chrXY
    #  check if we are stomping on existing mgiId entries
    if (exists($mgiId{$eles[0]})) {
	if ($mgiId{$eles[0]} ne $eles[3]) {
	    if ($verbose) {
		printf STDERR "ERROR: duplicate mgiId key $eles[3] (values $mgiId{$eles[0]} $eles[3]) in $mgdSeqF at line $.\n";
	    }
	next;
	}
    } else {
	#  check if we are stomping on existing mgiName entries
	if (exists($mgiName{$eles[3]}) ) {
	    if ($mgiName{$eles[3]} ne $eles[0]) {
		if ($verbose) {
		printf STDERR "ERROR: duplicate mgiName key $eles[0] (values $mgiName{$eles[3]} $eles[0]) in $mgdSeqF at line $.\n";
		}
	    next
	    }
	} else {
	    $eles[8] =~ s/MT/M/;	#	chr name change MT to M
	    $eles[9] =~ s/\s//g;	# remove white space from cm
	    if ($eles[9] =~ m/^0.00$/) { $eles[9] = 0.0; }
	    $mgiId{$eles[0]} = $eles[3];
	    $mgiName{$eles[3]} = $eles[0];
	    $chr{$eles[0]} = $eles[8];
	    $cm{$eles[0]} = $eles[9];
	    $leftPrimer{$eles[0]} = $eles[5];
	    $rightPrimer{$eles[0]} = $eles[6];
	    $productSize{$eles[0]} = $eles[7]; 
	}
    }
}
close(MGDS);

if (1 == 0) {
if ($verbose) {print STDERR "reading mgd seq info file $mgdAlias\n";}
while(my $line = <MGDA>)
{
    chomp($line);
    $line = uc($line);
    my @eles = split(/\t/, $line);
    my $aliasList = "";
    if (defined($eles[7])) {
	$aliasList = "$eles[0];$eles[1];$eles[7]";
    } else {
	$aliasList = "$eles[0];$eles[1]";
    }
    my @aliases = split(/\;/, $aliasList);
    for(my $i = 0; $i < @aliases; $i++)
    {
	if (exists($mgiAlias{$aliases[$i]})) {
	    if ($verbose) {
		print STDERR "ERROR: duplicate mgiAlias key $aliases[$i] (values '${$mgiAlias{$aliases[$i]}}' '$aliasList') in $mgdAlias at line $.\n";
	    }
	} else {
	    $mgiAlias{$aliases[$i]} = \$aliasList;
	}
    }
}    
} else {
    if ($verbose) {
    print STDERR "not reading mgd seq info file $mgdAlias since it is not used\n";
    }
}
close(MGDA);

if ($verbose) {print STDERR "reading uni alias file $uniAliasF\n";}
while(my $line = <UNIA>)
{
    chomp($line);
    $line = uc($line);
    my @eles = split(/\t/, $line);
    my $aliasList = $eles[0]."\;"."$eles[1]";
    my @aliases = split(/\;/, $aliasList);
    for(my $i = 0; $i < @aliases; $i++)
    {
	$alias{$aliases[$i]} = \$aliasList;
    }
}
close(UNIA);

if ($verbose) {print STDERR "reading uni primer seq file $uniSeqF\n";}
while(my $line = <UNIS>)
{
    chomp($line);
    $line = uc($line);
    my @eles = split(/\t/, $line);
    if (exists($leftPrimer{$eles[0]})) {
	if ($leftPrimer{$eles[0]} ne $eles[1]) {
	    if ($verbose) {
	    printf STDERR "ERROR: duplicate uniSTS key $eles[0] different leftPrimer line $.\n";
	    }
	next
	}
    }
#  The uniId hash is UNUSED for anything
if (1 == 0) {
    if (exists($uniId{$eles[4]})) {
	if ($uniId{$eles[4]} ne $eles[0]) {
	    if ($verbose) {
	    printf STDERR "ERROR: duplicate uniSTS Id key '$eles[4]' (values $uniId{$eles[4]} $eles[0]) line $.\n";
	    }
	next
	}
    }
    $uniId{$eles[4]} = $eles[0];
}
    $leftPrimer{$eles[0]} = $eles[1];
    $rightPrimer{$eles[0]} = $eles[2];
    $productSize{$eles[0]} = $eles[3];
    if (exists($uniName{$eles[0]})) {
	if ($uniName{$eles[0]} ne $eles[4]) {
	    if ($verbose) {
	    printf STDERR "ERROR: duplicate uniName key $eles[0] (values $uniName{$eles[0]} $eles[4]) line $.\n";
	    }
	next
	}
    }
    $uniName{$eles[0]} = $eles[4];
}
close(UNIS);

if($rh)
{
    if ($verbose) {print STDERR "reading rh map file $rhFile\n";}
    while(my $line = <RH>)
    {
	next if($line =~ /^\#/);
	chomp($line);
	$line = uc($line);
	my (@eles)= split(/\t/, $line);
	$rhChr{$eles[0]} = $eles[2];
	$rhChr{$eles[1]} = $eles[2];
	$rhCm{$eles[0]} = $eles[3];
	$rhCm{$eles[1]} = $eles[3];
    }
    close(RH);
}

if($genetic)
{
    if ($verbose) {print STDERR "reading genetic map file $geneticFile\n";}
    while(my $line = <GEN>)
    {
	next if($line =~ /^\#/);
	chomp($line);
	$line = uc($line);
	my (@eles)= split(/\t/, $line);
	$gChr{$eles[0]} = $eles[2];
	$gChr{$eles[1]} = $eles[2];
	$gCm{$eles[0]} = $eles[3];
	$gCm{$eles[1]} = $eles[3];
    }
    close(GEN);
}

##udate current bed 
my $lastId = 0;
if ($verbose) {print STDERR "reading previous bed $bedF\n";}
while(my $line = <BED>)
{
    chomp($line);
    $line = uc($line);
    my @allName = ();
    my ($id, $name, $sId, $sname, $uistsid, $aliasCount, $alias, $primer1, $primer2, $distance, $isSeq, $organism, $geneticMap, $gChr, $gPosi, $generalMap, $uChr, $uPosi, $RHMap, $rhChr, $rhPosi) = split(/\t/, $line);
    $lastId = $id;
    $sId = "MGI:".$sId;

    if (!defined($rhChr)) { $rhChr = ""; }
    if (!defined($rhPosi)) { $rhPosi = ""; }
    if (!defined($geneticMap)) { $geneticMap = ""; }
    if (!defined($gPosi)) { $gPosi = -1; }
    if (!defined($generalMap)) { $generalMap = ""; }
    if (!defined($uPosi)) { $uPosi = -1; }
    if (!defined($RHMap)) { $RHMap = ""; }
    if (!defined($rhPosi)) { $rhPosi = -1; }
    if (!defined($aliasCount)) { $aliasCount = 0; }
    if (!defined($sname)) { $sname = ""; }

    if (length($aliasCount) < 1) { $aliasCount = 0; }
    ## get all names for seaching other info
    if($aliasCount < 1)
    {
	if ($verbose > 1) {
	printf STDERR "creating allName since aliasCount < 1 == $aliasCount\n";
	}
	@allName = &updateAlias(\$name, \$sId, \$sname, \$uistsid, \$alias, \$aliasCount);
    }
    else
    {
	my @tempName = split(/\;/, $alias);
	if ($verbose > 1) {
	printf STDERR "creating allName since aliasCount >= 1 == $aliasCount\n";
	}
	push(@allName, ($name, $sId, $sname, $uistsid, @tempName));
    }

if ($verbose > 1) {
    for(my $i=0; $i < scalar(@allName); ++$i) {
	printf STDERR "name $i '$allName[$i]'\n";
    }
}

    if($primer1 eq "")
    {
	if ($verbose > 1) {
	printf STDERR "searchPrimer since primer1 == NULL\n";
	}
	($primer1, $primer2, $distance) = &searchPrimer(@allName);
	 #next if($primer1 eq "");
    }
    
    ## remove rest seq links of marker if it is already in bed
 
    &removePrimerFromHash(@allName);

    ## verify map info
    my ($tChr, $tPosi) = &searchGeneralMap(@allName);

    ##update map info
    if (defined($tChr) && defined($tPosi)) {
	if (length($uPosi) < 1) { $uPosi = -1; }
	if($generalMap eq "" || ($tChr ne $uChr) || ($tPosi != $uPosi))
	{
	    ($uChr, $uPosi) = &searchGeneralMap(@allName);
	    if($uChr ne "")
	    {
		$generalMap = "MGC";
	    }
	    if ($verbose > 1) {
    printf STDERR "searchGeneralMap since generalMap == NULL: $uChr $uPosi\n";
	    }
	}
    }
    if (!defined($uChr)) { $uChr = ""; }
    if (!defined($uPosi) || (length($uPosi) < 0)) { $uPosi = ""; }

    if ($genetic)
	{
	($tChr, $tPosi) = &searchGeneticMap(@allName);
	if (defined($tChr) && defined($tPosi)) {
	    if($geneticMap eq "" || ($tChr ne $gChr) || ($tPosi != $gPosi))
		{
		($gChr, $gPosi) = &searchGeneticMap(@allName);
		if($gChr ne "")
		    {
		    $geneticMap =  "WI_MOUSE_GENETIC";
		    }
	    if ($verbose > 1) {
    printf STDERR "searchGeneticMap since geneticMap == NULL: $gChr $gPosi\n";
	    }
		}
	}
	}
    if (!defined($gChr)) { $gChr = ""; }
    if (!defined($gPosi) || (length($gPosi) < 1)) { $gPosi = ""; }

    if($rh)
    {
	($tChr, $tPosi) = &searchRhMap(@allName);
	if (defined($tChr) && defined($tPosi)) {
	    if($RHMap eq "" || ($tChr ne $rhChr) || ($tPosi != $rhPosi))
		{
		($rhChr, $rhPosi) = &searchRhMap(@allName);
		if($rhChr ne "")
		    {
		    $RHMap =  "WHITEHEAD-MRC_RH";
		    }
	    if ($verbose > 1) {
    printf STDERR "searchRhMap since RHMap == NULL: $rhChr $rhPosi\n";
	    }
		}
	}
    }
    if (!defined($rhChr)) { $rhChr = ""; }
    if (!defined($rhPosi) || (length($rhPosi) < 1)) { $rhPosi = ""; }

    $sId =~ s/MGI\://;
    if (!defined($sname)) { $sname = ""; }
    my @distanceList = split(',\s*|-',$distance);
    my $distanceSum = 0;
    for (my $j = 0; $j < scalar(@distanceList); ++$j)
	{
	$distanceList[$j] =~ s/\~//g;
	$distanceList[$j] =~ s/BP//i;
	if ($distanceList[$j] =~ m/KB/) {
	    $distanceList[$j] =~ s/\s*KB\s*//i;
	    $distanceList[$j] *= 1000;
	}
	$distanceSum += $distanceList[$j];
	}
    if (scalar(@distanceList) > 0) {
	$distance = $distanceSum / scalar(@distanceList);
    } else {
	$distance = $distanceSum;
    }

if (!defined($id)) { print STDERR "ERROR: not defined: id\n"; }
if (!defined($name)) { print STDERR "ERROR: not defined: name\n"; }
if (!defined($sId)) { print STDERR "ERROR: not defined: sId\n"; }
if (!defined($sname)) { print STDERR "ERROR: not defined: sname\n"; }
if (!defined($uistsid)) { print STDERR "ERROR: not defined: uistsid\n"; }
if (!defined($aliasCount)) { print STDERR "ERROR: not defined: aliasCount\n"; }
if (!defined($alias)) { print STDERR "ERROR: not defined: alias\n"; }
if (!defined($primer1)) { print STDERR "ERROR: not defined: primer1\n"; }
if (!defined($primer2)) { print STDERR "ERROR: not defined: primer2\n"; }
if (!defined($distance)) { print STDERR "ERROR: not defined: distance\n"; }
if (!defined($isSeq)) { print STDERR "ERROR: not defined: isSeq\n"; }
if (!defined($organism)) { print STDERR "ERROR: not defined: organism\n"; }
if (!defined($geneticMap)) { print STDERR "ERROR: not defined: geneticMap line $.\n"; }
if (!defined($gChr)) { print STDERR "ERROR: not defined: gChr line $.\n"; }
if (!defined($gPosi)) { print STDERR "ERROR: not defined: gPosi line $.\n"; }
if (!defined($generalMap)) { print STDERR "ERROR: not defined: generalMap line $.\n"; }
if (!defined($uChr)) { print STDERR "ERROR: not defined: uChr line $.\n"; }
if (!defined($uPosi)) { print STDERR "ERROR: not defined: uPosi line $.\n"; }
if (!defined($RHMap)) { print STDERR "ERROR: not defined: RHMap line $.\n"; }
if (!defined($rhChr)) { print STDERR "ERROR: not defined: rhChr line $.\n"; }
if (!defined($rhPosi)) { print STDERR "ERROR: not defined: rhPosi line $.\n"; }
    if ((length($gPosi)>0) && ($gPosi < 0)) { $gPosi = ""; }
    if ((length($uPosi)>0) && ($uPosi < 0)) { $uPosi = ""; }
    if ((length($rhPosi)>0) && ($rhPosi < 0)) { $rhPosi = ""; }
    if (length($name)>0) {
	my $newline = join("\t", ($id, $name, $sId, $sname, $uistsid, $aliasCount, $alias, $primer1, $primer2, $distance, $isSeq, $organism, $geneticMap, $gChr, $gPosi, $generalMap, $uChr, $uPosi, $RHMap, $rhChr, $rhPosi));
	print "$newline\n";
    }
}
close(BED);

## add markers that have primer info to the bed.
 
my $id = $lastId+1;

if ($verbose) {print STDERR "adding new markers starting at # $id\n";}

if ($verbose) {print STDERR "cleaning leftPrimers of blanks\n";}
foreach my $key (keys(%leftPrimer)) {
    if($leftPrimer{$key} eq "") { &removePrimerFromHash($key); }
}

my @allkeys = (keys(%leftPrimer));
my $keyCount = scalar(@allkeys);

if ($verbose) {print STDERR "working on $keyCount leftPrimers\n";}
while(my $key = shift(@allkeys))
{
    if (!exists($leftPrimer{$key})) { next; }
    if ($leftPrimer{$key} eq "")
    {
    printf STDERR "ERROR: key $key should have been removed from leftPrimer\n";
	exit 255
    }
    my @allNames = ();
    my ($name, $sId, $sname, $uistsid, $aliasCount, $alias, $primer1, $primer2, $distance, $isSeq, $organism, $geneticMap, $gChr, $gPosi, $generalMap, $uChr, $uPosi, $RHMap, $rhChr, $rhPosi) =();
    
    ## update static part of bed
    $primer1 = $leftPrimer{$key};
    $primer2 = $rightPrimer{$key};
    $distance = $productSize{$key};
    $name = $key;
    $isSeq = 0;
    $aliasCount = 0;
    $alias = "";
    $organism = "MUS MUSCULUS";

    ## find as many names info as possible
    if(defined($uniName{$key}))
    {
	$name = $uniName{$key};
	$uistsid = $key;
    } else {
	$uistsid = "";
    }
    if(defined($mgiId{$key}))
    {
	$sId = $mgiId{$key};
	$sname = $key;
    }
    push(@allNames, ($name, $uistsid, $sId, $sname));

    ## try to find all names
    for(my $i = 0; $i < @allNames; $i++)
    {
	if(defined($alias{$allNames[$i]}))
	{
	    $alias = ${$alias{$allNames[$i]}};
	    my @temp = split(/\;/, $alias);
	    $aliasCount = @temp;
	    push(@allNames, @temp);
	    last;
	}  
    }

    ##update maps with all names
    ($uChr, $uPosi) = &searchGeneralMap(@allNames);
    if($uChr ne "")
    {
	$generalMap = "MGC";
    }
    if ($genetic)
	{
	if (!defined($geneticMap)) { $geneticMap = ""; }
	my ($tChr, $tPosi) = &searchGeneticMap(@allNames);
	if (defined($tChr) && defined($tPosi)) {
	    if($geneticMap eq "" || ($tChr ne $gChr) || ($tPosi != $gPosi))
		{
		($gChr, $gPosi) = &searchGeneticMap(@allNames);
		if($gChr ne "")
		    {
		    $geneticMap =  "WI_MOUSE_GENETIC";
		    }
	    if ($verbose > 1) {
    printf STDERR "searchGeneticMap since geneticMap == NULL: $gChr $gPosi\n";
	    }
		}
	}
	}
    if (!defined($gChr)) { $gChr = ""; }
    if (!defined($gPosi)) { $gPosi = ""; }
    if (!defined($geneticMap)) { $geneticMap = ""; }

    if($rh)
    {
	if (!defined($RHMap)) { $RHMap = ""; }
	my ($tChr, $tPosi) = &searchRhMap(@allNames);
	if (defined($tChr) && (length($tChr) > 0) && defined($tPosi)) {
	    if (!defined($rhChr)) { $rhChr = ""; }
	    if($RHMap eq "" || ($tChr ne $rhChr) || ($tPosi != $rhPosi))
		{
		($rhChr, $rhPosi) = &searchRhMap(@allNames);
		if($rhChr ne "")
		    {
		    $RHMap =  "WHITEHEAD-MRC_RH";
		    }
	    if ($verbose > 1) {
    printf STDERR "searchRhMap since RHMap == NULL: $rhChr $rhPosi\n";
	    }
		}
	}
    }
    if (!defined($rhChr)) { $rhChr = ""; }
    if (!defined($rhPosi)) { $rhPosi = ""; }
    if (!defined($RHMap)) { $RHMap = ""; }

    &removePrimerFromHash(@allNames);
    if (defined($sId) && (length($sId) > 0)) { $sId =~ s/MGI\://; }

    my @distanceList = split(',\s*|-',$distance);
    my $distanceSum = 0;
    for (my $j = 0; $j < scalar(@distanceList); ++$j)
	{
	$distanceList[$j] =~ s/\~//g;
	$distanceList[$j] =~ s/BP//i;
	if ($distanceList[$j] =~ m/KB/) {
	    $distanceList[$j] =~ s/\s*KB\s*//i;
	    $distanceList[$j] *= 1000;
	}
	$distanceSum += $distanceList[$j];
	}
    if (scalar(@distanceList) > 0) {
	$distance = $distanceSum / scalar(@distanceList);
    } else {
	$distance = $distanceSum;
    }

if (!defined($id)) { print STDERR "ERROR: not defined: id\n"; }
if (!defined($name)) { print STDERR "ERROR: not defined: name\n"; }
if (!defined($sId)) { $sId = ""; }
if (!defined($sname)) { $sname = ""; }
if (!defined($uistsid)) { print STDERR "ERROR: not defined: uistsid\n"; }
if (!defined($aliasCount)) { print STDERR "ERROR: not defined: aliasCount\n"; }
if (!defined($alias)) { print STDERR "ERROR: not defined: alias\n"; }
if (!defined($primer1)) { print STDERR "ERROR: not defined: primer1\n"; }
if (!defined($primer2)) { print STDERR "ERROR: not defined: primer2\n"; }
if (!defined($distance)) { print STDERR "ERROR: not defined: distance\n"; }
if (!defined($isSeq)) { print STDERR "ERROR: not defined: isSeq\n"; }
if (!defined($organism)) { print STDERR "ERROR: not defined: organism\n"; }
if (!defined($geneticMap)) { print STDERR "ERROR: not defined: geneticMap line $.\n"; }
if (!defined($gChr)) { print STDERR "ERROR: not defined: gChr line $.\n"; }
if (!defined($gPosi)) { print STDERR "ERROR: not defined: gPosi line $.\n"; }
if (!defined($generalMap)) { $generalMap = ""; }
if (!defined($uChr)) { print STDERR "ERROR: not defined: uChr line $.\n"; }
if (!defined($uPosi)) { $uPosi = ""; }
if (!defined($RHMap)) { print STDERR "ERROR: not defined: RHMap line $.\n"; }
if (!defined($rhChr)) { print STDERR "ERROR: not defined: rhChr line $.\n"; }
if (!defined($rhPosi)) { print STDERR "ERROR: not defined: rhPosi line $.\n"; }

	if (length($name)>0) {
	    my $newline = join("\t", ($id, $name, $sId, $sname, $uistsid, $aliasCount, $alias, $primer1, $primer2, $distance, $isSeq, $organism, $geneticMap, $gChr, $gPosi, $generalMap, $uChr, $uPosi, $RHMap, $rhChr, $rhPosi));
	    print "$newline\n";
	    $id++;
	}
#	@allkeys = (keys(%leftPrimer));
	#print STDERR "$#allkeys\n";
}
