#!/usr/bin/perl

use warnings;

use strict;

my $type = $ARGV[0];

my $folder = $ARGV[1];

if ($type == 1){
	print <<TEXT;
You win the internet
Enough internets for today
TEXT
}
if ($type == 2){
	print <<TEXT;
There is no spoon
I know kung fu
TEXT
}
if ($type == 3){
	print <<TEXT;
Run forrest run
Stupid is as stupid does
TEXT
}
