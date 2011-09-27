package DataBrowser;
require Exporter;
use strict;
use vars qw(@ISA $VERSION @EXPORT);
$VERSION = "0.02";
@ISA = qw(Exporter);
@EXPORT = qw(browse); # exports browse name into caller's namespace

# browse takes a scalar argument
sub browse {
	my ($val, $level) = @_;
	$level = 0 unless defined $level;
	my $tab = '    ' x $level;
	my $ref = ref $val;
	if    (not defined $val) {print "undef\n"}
	elsif ($ref eq 'CODE' or $ref eq 'GLOB') {
		print $val, "\n";
	}
	elsif ($ref eq 'ARRAY') {
		print "$ref\n";
		$level++;
		for(my $i = 0; $i < @{$val}; $i++) {
			print "$tab    [$i] = ";
			browse($val->[$i], $level);
		}
	}
	elsif ($ref eq 'HASH' or $ref =~ /^[A-Z]/) {
		print "$ref\n";
		$level++;
		foreach my $key (keys %{$val}) {
			print "$tab    $key => ";
			browse($val->{$key}, $level);
		}
	}
	elsif ($ref) {
		print $ref, "\n";
	}
	else {
		print $val, "\n";
	}
}

1;
