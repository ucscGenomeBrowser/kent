# wordUse - Make dictionary of all words and count usages. 
use strict;

sub usage {
# Explain usage and exit
print <<EOF;
wordUse - words in file and print top ten
usage:
    countLines file
EOF
}

sub cmpCount 
# Compare two wordCounts in decreasing order of count.
{ 
$b->{count} - $a->{count} 
}

sub wordUse 
# wordUse - Make dictionary of all words and count usages, report top ten.
{
my $wc;
my $name = shift;
my %hash;
open(FH, $name);
my $count = 0;
while (<FH>) 
    {
    my @words = split;
    my $word;
    foreach $word (@words) 
	{
	if (!exists $hash{$word}) 
	    {
	    $wc = {};
	    $wc->{name} = $word;
	    $wc->{count} = 1;
	    $hash{$word} = $wc;
	    } 
	else 
	    {
	    $wc = $hash{$word};
	    $wc->{count} += 1;
	    }
	}
    }
my @list = sort cmpCount values %hash;
my $i;
for ($i=0; $i<10; ++$i) 
    {
    $wc = $list[$i];
    print "$wc->{name}\t$wc->{count}\n";
    }
}

usage if ($#ARGV != 0);
wordUse $ARGV[0];
