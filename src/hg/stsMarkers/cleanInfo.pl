#!/usr/bin/perl

# This script was originally written by Yontao Lu and was copied from
# /cluster/store4/ratMarker/code/ into the kent/src tree in order
# to get it under CVS control; added warnings, Getopt to parse -mouse and 
# -rat args.

# $Id: cleanInfo.pl,v 1.1 2006/03/03 19:56:17 angie Exp $

use Getopt::Long;
use warnings;

use vars qw/
    $opt_mouse
    $opt_rat
    $opt_help
    /;

##Des: Take a look how many RH marker has primers info
sub merge()
{
    my ($cur,$curName, $pre, $preName, @rest)= @_;
    my @items1 = @{$cur};
    my @items2 = @{$pre};
    my @names1 = @{$curName};
    my @names2 = @{$preName};
    my $newName = join(",", @names2);
    for(my $i = 0; $i < @names1; $i++)
    {
	if($newName =~ "$names1[$i]")
	{
	    next;
	}
	else
	{
	    $newName .= ", $names1[$i]";
	}
    }
    $items2[5] = $newName;
    for(my $i = 6; $i < @items1 && $i < @items2; $i++)
    {
	if(!$items2[$i] && $items1[$i])
	{
	    $items2[$i] = $items1[$i];
	}
    }
}


sub compMerge()
{
    my ($cur,$curName, $pre, $preName, @rest)= @_;
    my @items1 = @{$cur};
    my @items2 = @{$pre};
    my @names1 = @{$curName};
    my $preNamesList = ${$preName};
    my $match = 0;
    my $merged = 0;
    for(my $i = 0; $i < @names1; $i++)
    {
	next if($names1[$i] eq "");
	if($preNamesList =~ "$names1[$i]\;")
	{
	    if(!$merged)
	    {
		for(my $j = 0; $j < @items1; $j++)
		{
		    if($items1[$j] && ! $items2[$j])
		    {
			$items2[$j] = $items1[$j];
		    }
		}
		$merged = 1;
	    }
	}
	elsif($items2[6] !~ /$names1[$i]/i)
	{
	    $items2[6] .= ";$names1[$i]";
	}
    }
    my @a = split(/\;/, $items2[6]);
    my $aliasCount = scalar(@a);
    $items2[5] = $aliasCount;
    $pre = \@items2 if ($merged == 1);
    
    return ($merged, $pre);
}


my $ok = GetOptions("mouse",
		    "rat",
		    "help");

if(!$ok || $opt_help || (@ARGV != 1))
{
    print STDERR "usage: cleanInfo.pl [-mouse | -rat] stsInfoFile\n";
    exit(! $opt_help);
}


##0-id, 1-name, 2-rgdid,3-rgdname, 4-uistsid, 5-#ofalias, 6-alias, 7-primer1, 8-primer2, 9-distance, 10-isSeq, 11-organism, 12-FFHXACI, 13-chr, 14-posi, 15-SHRSPXBN, 16-chr, 17-posi, 18-RH, 19-chr, 20-posi, 21-RHLOD, 22-Gene_name, 23-Gene_Id, 24-cloSeq


$info = shift;
open(INF, "<$info") || die "Can't open $info: $!";
$id = 0;
while(my $line = <INF>)
{
    chomp($line);
    $line = uc($line);
    my $merged = 0;
    my (@eles) = split(/\t/, $line);
    my $sId = "";
    $sId = "RGD".$eles[2] if($opt_rat && ($eles[2] ne ""));
    $sId = "MGI:".$eles[2] if($opt_mouse && ($eles[2] ne ""));
    my $namesList;
    foreach my $temp  ($eles[1], $sId, $eles[3], $eles[4], split(/\;/, $eles[6]))
    {
	$namesList .= "$temp;" if($temp && $temp ne "-");
    }
    next if($namesList !~ /\w/o);
    my @nameRef = ($eles[1], $sId, $eles[3], $eles[4]);
    my $leftP = $eles[7];
    my $rightP = $eles[8];

    #if($leftP !~ /\w/o )
    #{
#	for(my $i = 0; $i<@nameRef; $i++)
#	{
#	    next if($nameRef[$i] eq "");
#	    if(defined($nameHash{$nameRef[$i]}))
#	    {
#		my $compId  = $nameHash{$nameRef[$i]};
#		$merged = &compMerge(\@eles, \@nameRef, $allInfo[$compId], $allNames[$compId]);
#		last;
#		
#	    }
#	}
#
#    }
#    elsif(defined($primerHash{$leftP}) && defined($primerHash{$rightP}))
#    {
#	if($primerHash{$leftP} == $primerHash{$rightP})
#	{
#	    my $compId = $primerHash{$leftP};
#	    ($merged,$allInfo[$compId]) = &compMerge(\@eles, \@nameRef, $allInfo[$compId], $allNames[$compId]);
	#}
    #}

    for(my $i = 0; $i<@nameRef; $i++)
    {
	next if($nameRef[$i] eq "");
	if(defined($nameHash{$nameRef[$i]}))
	{
	    my $compId  = $nameHash{$nameRef[$i]};
	    ($merged, $allInfo[$compId]) = &compMerge(\@eles, \@nameRef, $allInfo[$compId], $allNames[$compId]);
	    last;
	}
    }
    
    if($merged == 0)
    {
	for(my $i = 0; $i<@nameRef; $i++)
	{
	    next if($nameRef[$i] eq "");
	    $nameHash{$nameRef[$i]} = $id;
	}
	$primerHash{$leftP} = $id;
	$primerHash{$rightP} = $id;
	$allInfo[$id] = \@eles;
	$allNames[$id] = \$namesList;
	$id++;
    }
}

for(my $i = 0; $i < $id; $i++)
{
    die "nada allInfo[$i]" if (! $allInfo[$i]);
    for (my $j=0;  $j < scalar(@{$allInfo[$i]});  $j++) {
	$allInfo[$i]->[$j] = "" if (! defined $allInfo[$i]->[$j]);
    }
    my $newLine = join("\t", @{$allInfo[$i]});
    print "$newLine\n" if($newLine =~ /\w/o);
}

