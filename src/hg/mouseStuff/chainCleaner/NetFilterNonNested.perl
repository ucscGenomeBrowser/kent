#!/usr/bin/perl
# Michael Hiller, 2013-2016, MPI-CBG & MPI-PKS, Dresden, Germany

# non-nested net filtering
# 	netFilter from the kent source removes all nested nets if the top level net does not pass the filters. 
# 	As nets are ordered by the start coordinates (fill) and not by score, this can lead to excluding some good nets just because they are spanned by a larger but weak net.
# This script would keep nested nets that pass the filters, even if higher level nets are excluded. 
# The level of the nets is adjusted. 
# 
# See the usage message for more details. 
# output goes to stdout

use strict;
use warnings;
use Getopt::Long qw(:config no_ignore_case no_auto_abbrev);
use POSIX;

$| = 1;		# == fflush(stdout)
my $verbose = 0;
my $minScore = "";		# sets of parameters for score, size on query and size on target; separated with commas
my $minSizeT = "";		
my $minSizeQ = "";

my $minScore1 = 0;              # two sets of parameters for score, size on query and size on target; set 1 is used in combination; same for set 2
my $minSizeT1 = 0;              # to get chains with score > 50K OR chains with Qsize > 100K set minScore1 and minSizeQ2
my $minSizeQ1 = 0;
my $minScore2 = 0;
my $minSizeT2 = 0;
my $minSizeQ2 = 0;

my $keepSynNetsWithScore = INT_MAX;      # keep nets classified as syn if the are above this score threshold
my $keepInvNetsWithScore = INT_MAX;      # keep nets classified as inv if the are above this score threshold

my $doScoreFilter = 0;          # flag: Filter for minScore1 (must be given) but keep all syn/inv nets with score >= keepSynNetsWithScore/keepInvNetsWithScore if there parent is kept

my $doUCSCSynFilter = 0;          # apply the UCSC syntenic net filter, but non-nested
# UCSC parameters from netFilter.c
my $UCSCminTopScore = 300000;  # Minimum score for top level alignments
my $UCSCminSynScore = 200000;  # Minimum score for block to be syntenic  regardless.  On average in the human/mouse net a score of 200,000 will cover 27000 bases including 9000 aligning bases - more than all but the heartiest of processed pseudogenes. 
my $UCSCminSynSize = 20000;    # Minimum size for syntenic block.
my $UCSCminSynAli = 10000;     # Minimum alignment size.
my $UCSCmaxFar = 200000;       # Maximum distance to allow synteny.


my $usage = "non-nested net filtering
$0 input.net[.gz]  [-v] [other parameters depending on filter mode]
\
There are several filtering modes: 
\tEITHER [-doUCSCSynFilter -keepSynNetsWithScore int -keepInvNetsWithScore int]
\t    OR [-doScoreFilter -minScore1 int -keepSynNetsWithScore int -keepInvNetsWithScore int]       
\t    OR [-minScore comma-separated-string -minSizeT comma-separated-string -minSizeQ comma-separated-string]
\t    OR [-minScore1 int -minSizeT1 int -minSizeQ1 int  -minScore2 int -minSizeT2 int -minSizeQ2 int]
\
This script works like netFilter from the kent source but would keep nested nets that pass the filters, even if higher-level (parent) nets are excluded. 
The level of the nets is adjusted. 
\
The first mode is UCSC syntenic net filtering (but non-nested),
which applies the minTopScore (300000) and minSynScore (200000) and a few other size, ali and maxFar thresholds to the nets. 
If you want to keep syntenic / inverted nets whose parent net is kept, set the -keepSynNetsWithScore and -keepInvNetsWithScore to the score threshold, E.g. 
   -doUCSCSynFilter -keepSynNetsWithScore 5000 -keepInvNetsWithScore 5000
keeps nets that are syntenic / inverted nets whose parent net is kept and have a score >= 5000.
\
The second mode is similar to UCSC syntenic filtering, but you just apply a user-selected score threshold
   -doScoreFilter -minScore1 10000 -keepSynNetsWithScore 3000 -keepInvNetsWithScore 3000 
This keeps all nets with a score of >= 10000 and keeps syntenic / inverted nets that score at least 3000. 
\
The third mode is batch, which does not consider the type of the net (top, syn, inv etc)
You can give any number of filters for score, target span and query span. 
Each threshold in a set will be evaluated using a logical AND. 
Different sets will be evaluated in a logical OR.
E.g. if you want to keep nets with score >=10000 and span of >=10000 OR nets with score of >=50000 and span >= 5000 OR nets with score >= 100000 of any span, do
   -minScore 10000,50000,100000 -minSizeT 10000,5000,0 -minSizeQ 10000,5000,0 
\
For the fourth mode, you give up to two sets of filters (implemented for backwards compatibility). It also does not consider the net type.
E.g. if you want to keep e.g. nets with a minscore of 10000 and a span of 10000  OR  nets with a minscore of 500000 (regardless of span) just set
   -minScore1 10000 -minSizeT1 10000 -minSizeQ1 10000 -minScore2 500000
If you give only one set (score1/T1/Q1) then no filtering is performed on the second set (implemented by setting the values for the second set to INT_MAX)
\n";

GetOptions ("v|verbose"  => \$verbose, 
	"minScore1=i" => \$minScore1, "minSizeT1=i" => \$minSizeT1, "minSizeQ1=i" => \$minSizeQ1, 
	"minScore2=i" => \$minScore2, "minSizeT2=i" => \$minSizeT2, "minSizeQ2=i" => \$minSizeQ2, 
	"minScore=s" => \$minScore, "minSizeT=s" => \$minSizeT, "minSizeQ=s" => \$minSizeQ,
	"keepSynNetsWithScore=i" => \$keepSynNetsWithScore, "keepInvNetsWithScore=i" => \$keepInvNetsWithScore, 
	"doUCSCSynFilter" => \$doUCSCSynFilter, 	"doScoreFilter" => \$doScoreFilter)
	|| die "$usage\n";
die "$usage\n" if ($#ARGV < 0);

die "ERROR: you have to set -minScore1 with -doScoreFilter\n" if ($doScoreFilter && $minScore1 == 0);

# check if the fourth ("12") filter mode applies
my @minScores;
my @minTsizes;
my @minQsizes;
my $filterMode = "";
if ($minScore1 != 0|| $minSizeT1 != 0|| $minSizeQ1 != 0|| $minScore2 != 0|| $minSizeT2 != 0|| $minSizeQ2 != 0) {
	$filterMode = "12";
	# if only values for set 1 are given, then set all values for set 2 to INTMAX. 
	# Otherwise our filter routine would say pass to every net because set 2 defaults are 0.
	if ($minScore2 == 0 && $minSizeT2 == 0 && $minSizeQ2 == 0) {
		$minScore2 = INT_MAX;
		$minSizeT2 = INT_MAX;
		$minSizeQ2 = INT_MAX;
	}
	if ($minScore1 == 0 && $minSizeT1 == 0 && $minSizeQ1 == 0) {
		$minScore1 = INT_MAX;
		$minSizeT1 = INT_MAX;
		$minSizeQ1 = INT_MAX;
	}
}
# check if batch mode applies
if ($minScore ne "" || $minSizeT ne "" || $minSizeQ ne "") {
	if ($filterMode eq "12") {
		die "ERROR: you have used BOTH batch filtering (minScore/minSizeT/minSizeQ) AND individual filtering (minScore1/minSizeT1/minSizeQ1 etc)\n
		     USE Either batch or individual\n";
	}
	$filterMode = "batch";
	@minScores = split(/,/, $minScore);
	@minTsizes = split(/,/, $minSizeT);
	@minQsizes = split(/,/, $minSizeQ);
	die "ERROR: number of minScores differ from minTsizes\n" if ($#minScores != $#minTsizes);
	die "ERROR: number of minScores differ from minQsizes\n" if ($#minScores != $#minQsizes);
}
print STDERR "FilterMode: $filterMode\n" if ($verbose);


##############################
# read the net file
if ($ARGV[0] =~ /.gz$/) {
	open(file1, "zcat $ARGV[0]|") || die "ERROR: cannot open $ARGV[0]\n";
}else{	
	open(file1, $ARGV[0]) || die "ERROR: cannot open $ARGV[0]\n";
}
my @file = <file1>;
chomp(@file);
close file1;

my %NetKeptLines;		# keeps a flag for all "net" lines whether we kept at least one entry for that net
my %SkipLines; 		# contains all lines that we will skip at the end
my %MinusSpaces; 		# contains all lines that we will skip at the end
my $curNet = 0;
my $maxNetLevel = 1;		# maximum level (depth) encountered; update while we read
my %Level2IsSkipped;	# hash that contain for each fill level 1 (this net=fill is skipped) or 0 (this net is kept). Is continously updated

my $index = 0;			# index in the net array
# a proper net file must start with a ^net line  (or comments)
for ($index=0; $index<=$#file; $index++) {
	next if ($file[$index] =~ /^#/);
	if ($file[$index] !~ /^net /) {
		die "ERROR: expect file to start with net, but got this line instead: $file[$index]\n";
	}else{
		last;
	}
}
$curNet = $file[$index];
$NetKeptLines{$file[$index]} = 0;
# now continue reading after the first 'net' line
for (my $i=$index+1; $i<=$#file; $i++) {
	my $line1 = $file[$i];

	next if ($line1 =~ / gap /);		# gap lines are handled when their corresponding net is handled
	if ($line1 =~ /^net /) {
		$curNet = $line1;
		$NetKeptLines{$curNet} = 0;
		next;
	}

	# determine the level of the current line = # spaces at the line beginning 	
	if ($line1 !~ /^([ ]+)([fill|gap].*)/) {
		print STDERR "ERROR: expect fill or gap in $line1\n";
		exit -1;
	}
	my $spaces = $1;
	my $lineRest = $2;
	my $level = length($spaces);
	$maxNetLevel = $level if ($maxNetLevel < $level);
	print "\ncurrent line: level $level (max $maxNetLevel)  rest: $lineRest\n" if ($verbose);

	# fill line --> check if we should keep that net
	if ($line1 =~ / fill /) {
		# fill 61168697 78 chrIV + 24270860 75 id 5545645 score 3359 ali 75 qDup 75 type nonSyn tN 0 qN 0 tR 0 qR 0 tTrf 0 qTrf 0
		my @f = split(/ /,$lineRest);
	
		# extract the type (ONLY if keepSyn/Inv nets is given or UCSC syntenic filtering). The type can be a variable position
		# fill 24258609 108 chr11 + 76807460 108 id 1579984 score 3220 ali 108 qOver 0 qFar 43429601 qDup 0 type inv tN 0 qN 0 tR 0 qR 0 tTrf 0 qTrf 0
		# fill 24260574 62 chr3 - 97726917 62 id 1686589 score 3151 ali 62 qDup 0 type nonSyn tN 0 qN 0 tR 0 qR 0 tTrf 0 qTrf 0
		my $type = "";
		if ($keepSynNetsWithScore < INT_MAX || $keepInvNetsWithScore < INT_MAX || $doUCSCSynFilter == 1) {
			if ($lineRest !~ /type (\w+) /) {
				if ($lineRest =~ /type (\w+)$/) {		# without netClass the type field is the line end fill 11489 26768 KL142851 - 995188 28731 id 14423 score 988018 ali 20213 qDup 28140 type top
					$type = $1;
				}else{
					die "ERROR: parameter -keepSynNetsWithScore/-keepInvNetsWithScore\-doUCSCSynFilter is given, but I cannot parse the net type from this fill line: $lineRest\n";
				}
			}else{
				$type = $1;
			}
		}

		# extract the ali field (ONLY if UCSC syntenic filtering)
		# fill 24260574 62 chr3 - 97726917 62 id 1686589 score 3151 ali 62 qDup 0 type nonSyn tN 0 qN 0 tR 0 qR 0 tTrf 0 qTrf 0
		my $ali = 0;
		if ($doUCSCSynFilter == 1) {
			if ($lineRest !~ /ali (\d+) /) {
				die "ERROR: parameter -doUCSCSynFilter is given, but I cannot parse the ali filed from this fill line: $lineRest\n";
			}else{
				$ali = $1;
			}
		}
		
		# extract the qFar field (ONLY if UCSC syntenic filtering). NOTE: only inv and syn nets have that field
		#    fill 60734 92 chr2 + 87809474 99 id 674214 score 1098 ali 91 qOver 0 qFar 23665927 qDup 99 type syn tN 0 qN 0 tR 0 qR 43 tTrf 0 qTrf 0
		my $qFar = 0;
		if ($doUCSCSynFilter == 1 && ($type eq "inv" || $type eq "syn")) {
			if ($lineRest !~ /qFar (\d+) /) {
				die "ERROR: parameter -doUCSCSynFilter is given, but I cannot parse the qFar filed from this syn/inv fill line: $lineRest\n";
			}else{
				$qFar = $1;
			}
		}

		# extract the score field 
		# (this should always be $f[10]; however netClass removes the score field if the score was 0. This gives fill lines like: 
		# 		fill 146299919 26 chr16 - 13866338 17 id 9729 ali 17 qDup 17 type nonSyn tN 0 qN 0 tR 0 qR 0 tTrf 0 qTrf 0
		#  in which case we would mistakingly take the number of aligning bases for the score) ==> properly extract it and error otherwise
		my $score = 0;
		if ($lineRest !~ /score (\d+) /) {
			die "ERROR: no score field is given in this fill line: $lineRest\n";
		}else{
			$score = $1;
		}

		# here is the filter: pass score, Tsize and Qsize to the function
		if ( passesFilter($score, $f[2], $f[6], $type, $ali, $qFar, $level) eq "true" ){		
			print "KEEP: $line1\n" if ($verbose);
			$NetKeptLines{$curNet}++;			# update the hash if we keep a fill block
			resetLevel2IsSkipped($level);		# reset hash from this level on
		}else{
			print "SKIP: $line1\n" if ($verbose);
			$SkipLines{$i} = 1;		# we will skip this line later
			eraseGapsMarkSkip($i+1, $level);
			$Level2IsSkipped{$level} = 1;
		}
	}
	
}
close file1;

# output the filtered nets
output();



######################################################
# set Level2IsSkipped = 0 for level $level to $maxLevel
# --> Meaning: If we keep a net at level 3, then reset this hash from level 3 on
######################################################
sub resetLevel2IsSkipped {
	my $level = shift;
	
	for (my $i=$level; $i <= $maxNetLevel; $i++) {
		$Level2IsSkipped{$i} = 0;
		print "\t\treset Level2IsSkipped{$i} = 0\n" if ($verbose);
	}
}


######################################################
# UCSC netFilter -syn but non-recursive (non-nested)
######################################################
sub UCSCsynFilter_nonRecursive {
	my ($score, $Tsize, $Qsize, $type, $ali, $qFar, $level) = @_;

	if ($type eq "") {
		die("No type field, please run input net through netSyntenic");
	}
	if ($score >= $UCSCminSynScore && $Tsize >= $UCSCminSynSize && $ali >= $UCSCminSynAli) {
		return "true";
	}
	if ($type eq "top") {
		if ($score >= $UCSCminTopScore) {
			return "true";
		}else{
			return "false";
		}
	}
	if ($type eq "nonSyn") {
		return "false";
	}
	if ($qFar > $UCSCmaxFar) {
		return "false";
	}

	# For all others, assume syntenic.  This keeps tandem dupes, small inversion, and translocations.
	# Because we filter non-recursively, we need to assure that the parent of a small inv/syn net is also kept ($Level2IsSkipped{$level-2} == 0). 
	# Otherwise, remove this inv/syn net. 
	return testInvSynNet($score, $type, $level);
}


######################################################
# simple score based filtering that keeps the inv/syn nets above $keepSynNetsWithScore / $keepInvNetsWithScore
######################################################
sub scoreFilter_nonRecursive {
	my ($score, $Tsize, $Qsize, $type, $ali, $qFar, $level) = @_;

	if ($type eq "") {
		die("No type field, please run input net through netSyntenic");
	}
	if ($score >= $minScore1) {
		return "true";
	}

	if ($type eq "top") {
		return "false";
	}

	if ($type eq "nonSyn") {
		return "false";
	}

	# For all others, assume syntenic.  This keeps tandem dupes, small inversion, and translocations.
	# Because we filter non-recursively, we need to assure that the parent of a small inv/syn net is also kept ($Level2IsSkipped{$level-2} == 0). Otherwise, remove this inv/syn net. 
	return testInvSynNet($score, $type, $level);
}


######################################################
# test if the parent of a syn/inv net is kept. If so, test if its score is above the threshold
######################################################
sub testInvSynNet {
	my ($score, $type, $level) = @_;

	if ($Level2IsSkipped{$level-2} == 0) {
		if ($type eq "inv") {
			if ($score >= $keepInvNetsWithScore) {
				print "The parent of this inv net is kept and its score $score is above the inv threshold ($keepInvNetsWithScore) ==> KEEP\n" if ($verbose);
				return "true";
			}else{
				print "The parent of this inv net is kept, but its score $score is below the inv threshold ($keepInvNetsWithScore) ==> REMOVE this net\n" if ($verbose);
				return "false";
			}
		}elsif ($type eq "syn") {
			if ($score >= $keepSynNetsWithScore) {
				print "The parent of this syn net is kept and its score $score is above the syn threshold ($keepSynNetsWithScore) ==> KEEP\n" if ($verbose);
				return "true";
			}else{
				print "The parent of this syn net is kept, but its score $score is below the syn threshold ($keepSynNetsWithScore) ==> REMOVE this net\n" if ($verbose);
				return "false";
			}
		}else{
			# can only be a non-syn net (but we returned false before calling that function)
			return "false";
		}
	}else {
		print "==> this syn/inv net does not pass the synteny/score threshold and its parent (level $level-2) IS skipped --> remove this syn/inv net\n" if ($verbose);
		return "false";
	}
}


######################################################
# batch or individual filtering
######################################################
sub passesFilter {
	my ($score, $Tsize, $Qsize, $type, $ali, $qFar, $level) = @_;
	
	return UCSCsynFilter_nonRecursive($score, $Tsize, $Qsize, $type, $ali, $qFar, $level) if ($doUCSCSynFilter == 1);
	return scoreFilter_nonRecursive($score, $Tsize, $Qsize, $type, $ali, $qFar, $level) if ($doScoreFilter == 1);
	
	# first check if syn or inv filter applies
	if ($type eq "syn" && $score >= $keepSynNetsWithScore) {
		return "true";
	}
	if ($type eq "inv" && $score >= $keepInvNetsWithScore) {
		return "true";
	}

	if ($filterMode eq "12") {
		if ( ($score >= $minScore1 && $Tsize >= $minSizeT1  && $Qsize >= $minSizeQ1) ||
		     ($score >= $minScore2 && $Tsize >= $minSizeT2  && $Qsize >= $minSizeQ2) ) {
			return "true";
		}else{
			return "false";
		}
	}elsif ($filterMode eq "batch") {
		my $passAnyFilter = "false";
		for (my $i=0; $i<=$#minScores; $i++) {
			$passAnyFilter = "true" if ($score >= $minScores[$i] && $Tsize >= $minTsizes[$i] && $Qsize >= $minQsizes[$i]);
		}
		return $passAnyFilter;
	}else{
		die "ERROR: unknown value for filterMode $filterMode\n";
	}
}


######################################################
# final output of that net structure
######################################################
sub output {
	for (my $i=0; $i<=$#file; $i++) {
		my $line1 = $file[$i];
		if ($line1 =~ /^net /) {
			print "$line1\n" if ($NetKeptLines{$line1} > 0);		
		}
		if (! exists $SkipLines{$i}) {
			if ($line1 =~ /^([ ]+)([fill|gap].*)/) {
				my $spaces = $1;
				my $lineRest = $2;
				my $curlevel = length($spaces);
				if (exists $MinusSpaces{$i} && $MinusSpaces{$i} != 0) {
					$curlevel -= $MinusSpaces{$i};
				}
				$spaces = " " x $curlevel;
				print "$spaces$lineRest\n";
			}
		}
	}
}


######################################################
# function starts reading at the given line number and stops when the same level is reached --> all nested lines are visited
# it marks the lines that we want to exclude (all gaps belonging to the fill block that we remove) and adjusts the level (MinusSpaces hash) of all other nested fill and gap blocks
######################################################
sub eraseGapsMarkSkip {
	my ($startLineNumber, $level) = @_;

	print "\tERASE  from line number $startLineNumber with level $level\n" if ($verbose);
	for (my $i=$startLineNumber; $i<=$#file; $i++) {
		my $line1 = $file[$i];

		if ($line1 =~ /^net /) {
			# stop
			return;
		}
	
		# determine the level of the current line = # spaces at the line beginning 	
		if ($line1 !~ /^([ ]+)([fill|gap].*)/) {
			print STDERR "ERROR: expect fill or gap in $line1\n";
			exit -1;
		}
		my $spaces = $1;
		my $lineRest = $2;
		my $curlevel = length($spaces);
		print "\t\tlevel $curlevel  rest: $lineRest\n" if ($verbose);
	
		if ($curlevel <= $level) {
			# we have reached the next net (next fill at the same level)
			print "\treached end\n" if ($verbose);
			return;
		}elsif ($curlevel == $level + 1) {		# gaps belonging to the net that we skip
			$SkipLines{$i} = 1;		# we will skip this line later
		}elsif ($curlevel > $level + 1) {		# nested nets that we will examine later --> now just remove two spaces at the beginning
			$MinusSpaces{$i} += 2;
			print "\t\tincrease level + 2 for $line1\n" if ($verbose);
		}
		
	}
}

