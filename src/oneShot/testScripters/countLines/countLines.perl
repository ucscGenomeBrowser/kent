# Count lines in file
use strict;

sub usage {
# Explain usage and exit
print <<EOF;
countLines - count lines in files
usage:
    countLines files[s]
EOF
}

sub countFile {
    # Count lines in one file
    my $name = shift;
    open(FH, $name);
    my $count = 0;
    while (<FH>) {
	$count += 1;
    }
    print "total: $count\n";
}

usage if ($#ARGV != 0);
countFile $ARGV[0];
