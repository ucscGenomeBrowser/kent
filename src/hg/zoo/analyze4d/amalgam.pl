#! /usr/bin/perl

#merges all files together

# first read all numbers into an array

$a=0;
$max=0;

while(<*.gff>)
{
    
    $array_name = $_;
    $array_name =~ s/\b.*_(.*)_T1.*/$1/;
    
    $names[$a]=$array_name;
    print "$names[$a]\n";
    
    open(IN_FILE, $_) or die "Can't open input $_";
    open(OUT_FILE, ">temp_list_4d") or die "Can't open temp_list_4d"; 
    $i=0;

    while($line = <IN_FILE>)
    {	
	$val = $line;
	$val =~ s/(\S+)\s+\S+\n/$1/;
	
	$vals[$a][$i]=$val;
	$i++;
    }
    
    $maxes[$a]=$i;
    if($i > $max){
	$max=$i;
    }
    $a++;
}

# Now merge into big output...

$limit = $a;

print "Limit is $limit\n";
@indices=(0) x $a;


$m=0;

$not_finished=1;

print OUT_FILE "Header $limit\n";

for ($i=0;$i<$limit;$i++){
    print OUT_FILE "$names[$i] $i\n";
}

print OUT_FILE "\n";
$size=0;
while($not_finished){
    
    #for each array
    $not_finished=0;
    $current_min=9999999999999;
    for ($m=0;$m< $limit;$m++){
	
	# if array has lower value than current min.. reset current_min
	# if index is valid...
	if ($indices[$m] < $maxes[$m] ){
	    if ($vals[$m][$indices[$m]] < $current_min){
		$current_min=$vals[$m][$indices[$m]];
	    }
	}
    }
    # Now increment appropriate counters...

    if($current_min != 9999999999999){
	print OUT_FILE"$current_min ";
	$size++;
    }
    
    for ($m=0;$m< $limit ; $m++){

	if ($indices[$m] < $maxes[$m] ){
	    if($vals[$m][$indices[$m]] == $current_min){
		$not_finished = 1;
		$indices[$m]=$indices[$m]+1;
		print OUT_FILE"$m ";
	    }
	}
    }
    if($current_min !=9999999999999){
	print OUT_FILE"\n";
    }
}

close IN_FILE;
close OUT_FILE;


open(IN_FILE, "<temp_list_4d") or die "Can't open for input temp_list_4d";
open(OUT_FILE, ">finished_list_4d") or die "Can't open for output finished_list_4d";
while($line = <IN_FILE>){
    
    $line =~ s/Header (.*)/$1 $size/;
    print OUT_FILE "$line";
}
` rm temp_list_4d`;
