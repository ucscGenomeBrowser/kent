package julDate;

#	rcsid "$Id: julDate.pm,v 1.1 2007/01/19 00:42:24 hiram Exp $";

require Exporter;
@ISA = ('Exporter');
@EXPORT = qw(CalDate JulDate FormatCalDate);

##############################################################################
#	CalDate - pass in a pointer to a UTInstant
#	which has its julian decimal date set.  This routine
#	will calculate and set all the other elements of the
#	hash.  As a convenience, it will also return the year,
#	an integer.
#

sub CalDate() {
	my ($date) = @_;	#	pointer to the UTInstant
	#	These locals are all integers except for $frac
	my ($jd, $ka, $kb, $kc, $kd, $ke, $ialp, $frac);
	#	Thus, to be safer, we will use int() around all calculations

	$jd = int($date->{'J_DATE'} + 0.5);	# integer julian date
	$frac = $date->{'J_DATE'} + 0.5 - $jd + 1.0e-10; # fraction of day
	$ka = int($jd);
	
	if ( $jd >= 2299161 ) {
		$ialp = int(( $jd - 1867216.25 ) / ( 36524.25 ));
		$ka = int($jd + 1 + $ialp - ( $ialp >> 2 ));
	}
	$kb = int($ka + 1524);
	$kc = int(( $kb - 122.1 ) / 365.25);
	$kd = int($kc * 365.25);
	$ke = int(( $kb - $kd ) / 30.6001);

	$date->{'DAY'} = $kb - $kd - int( $ke * 30.6001 );

	if ( $ke > 13 ) {	$date->{'MONTH'} = int($ke - 13); }
	else {		$date->{'MONTH'} = int($ke - 1); }

	if ( ($date->{'MONTH'} == 2) && ($date->{'DAY'} > 28) ) {
		$date->{'DAY'} = 29;
	}

	if ( ($date->{'MONTH'} == 2) && ($date->{'DAY'} == 29) && ($ke == 3) ){
					 $date->{'YEAR'} = int($kc - 4716);
	} elsif ( $date->{'MONTH'} > 2 ) { $date->{'YEAR'} = int($kc - 4716);
	} else {			 $date->{'YEAR'} = int($kc - 4715); }

	$date->{'D_HOUR'} = $frac * 24.0;	#  d_hour includes min, sec as fraction
	$date->{'HOUR'} = int($date->{'D_HOUR'});	#	integer hour
	$date->{'D_MINUTE'} =		#  d_minute includes sec as fraction
		( $date->{'D_HOUR'} - $date->{'HOUR'} ) * 60.0;
	$date->{'MINUTE'} = int($date->{'D_MINUTE'});	#	integer minute
	$date->{'SECOND'} =		#  float second, int() it if you want
		( $date->{'D_MINUTE'} - $date->{'MINUTE'} ) * 60.0;
	$date->{'WEEKDAY'} = int(($jd + 1) % 7);	#	day of week
	if ( $date->{'YEAR'} == (($date->{'YEAR'} >> 2) << 2) ) {
		$date->{'DAY_OF_YEAR'} =
			int( ( 275 * $date->{'MONTH'} ) / 9)
			- int(($date->{'MONTH'} + 9) / 12)
			+ $date->{'DAY'} - 30;
	} else {
		$date->{'DAY_OF_YEAR'} =
			int( ( 275 * $date->{'MONTH'}) / 9)
			- ((($date->{'MONTH'} + 9) / 12) << 1)
			+ $date->{'DAY'} - 30;
	}
	return( $date->{'YEAR'} );
}	#	end of	 int CalDate( date )

############################################################################
#
#	JulDate - computes the julian decimal date from
#		the gregorian (or Julian) calendar date.
#	for astronomical purposes, The Gregorian calendar reform occurred
#	on 15 Oct. 1582.  This is 05 Oct 1582 by the julian calendar.
#
#	Single argument input: pointer to a hash that is a UTInstant
#		The elements that must be set are:
#		YEAR, MONTH, DAY, HOUR, MINUTE, SECOND
#		(if they have not been set, they appear as zero here)
#
#	Output result will be the setting of the UTInstant elements:
#		J_DATE, D_HOUR, D_MINUTE, WEEKDAY, DAY_OF_YEAR
#		and for convenience, the J_DATE value will be returned.
#
#	Reference: Astronomial formulae for calculators, meeus, p 23
#	from fortran program by F. Espenak - April 1982 Page 276,
#	50 Year canon of solar eclipses: 1986-2035
#

sub JulDate() {
	my ($date) = @_;	#	pointer to the UTInstant
	#	These locals are all integers except for $frac and $gyr
	my ($iy0, $im0, $ia, $ib, $jd, $frac, $gyr);
	#	Thus, to be safer, we will use int() around all calculations
	#		for the integers

#	the fraction of a day

	$frac = ($date->{'HOUR'}/ 24.0)
		+ ($date->{'MINUTE'} / 1440.0)
		+ ($date->{'SECOND'} / 86400.0);

#	convert the cal date to format YYYY.MMDDdd
	$gyr = $date->{'YEAR'}
		+ (0.01 * $date->{'MONTH'})
		+ (0.0001 * $date->{'DAY'})
		+ (0.0001 * $frac) + 1.0e-9;

#	conversion factors
	if ( $date->{'MONTH'} <= 2 ) {
		$iy0 = int($date->{'YEAR'} - 1);
		$im0 = int($date->{'MONTH'} + 12);
	} else {
		$iy0 = int($date->{'YEAR'});
		$im0 = int($date->{'MONTH'});
	}

	$ia = int($iy0 / 100);
	$ib = int(2 - $ia + ($ia >> 2));
#	calculate julian date
	if ( $date->{'YEAR'} <= 0 ) {
		$jd = int( (365.25 * $iy0) - 0.75 )
			+ int( (30.6001 * ($im0 + 1) ) )
			+ int( $date->{'DAY'} + 1720994 );
	} else {
		$jd = int(365.25 * $iy0)
			+ int( 30.6001 * ($im0 + 1) )
			+ int( $date->{'DAY'} + 1720994 );
	}
	if ( $gyr >= 1582.1015 ) {	#	on or after 15 October 1582
		$jd += $ib;
	}
	$date->{'J_DATE'} = $jd + $frac + 0.5;
	$jd = int( $date->{'J_DATE'} + 0.5 );
	$date->{'WEEKDAY'} = int( ($jd + 1) % 7 );
	return( $date->{'J_DATE'} );
}	#	end of	JulDate(

############################################################################
#	FormatCalDate - pass in a pointer to a UTInstant
#	Will format and return a string in standard format:
#	YYYY-MM-DD HH:MM:SS
sub FormatCalDate() {
	my ($t) = @_;	#	pointer to the UTInstant
	return( sprintf "%d-%02d-%02d %02d:%02d:%02d",
		$t->{'YEAR'}, $t->{'MONTH'}, $t->{'DAY'}, $t->{'HOUR'},
		$t->{'MINUTE'}, int($t->{'SECOND'} + 0.5));
}

1;

=head1 NAME

CalDate	-	Calculate calendar date from Julian date

JulDate	-	Calculate Julian date from calendar date

FormatCalDate	-	Return string in ISO 8601 format: YYYY/MM/DD hh:mm:ss
			http://www.cl.cam.ac.uk/~mgk25/iso-time.html

=head1 SYNOPSIS

    use CalDate;

    my %Date;
    $Date{'J_DATE'} = 2452589.105822;
    my $U=\%Date;		# $U is a pointer to %Date
    &CalDate($U);
    printf "%s\n",  &FormatCalDate($U);

    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(time);
    $Date{'YEAR'} = $year + 1900;
    $Date{'MONTH'} = $mon + 1;
    $Date{'DAY'} = $mday;
    $Date{'HOUR'} = $hour;
    $Date{'MINUTE'} = $min;
    $Date{'SECOND'} = $sec;
    &JulDate($U);
    printf "%s = JD: %.6f\n",  &FormatCalDate($U), $Date{'J_DATE'};


=head1 DESCRIPTION

=cut
###########################################################################
#
#	The fundamental structure is a UTInstant
#	Its elements will be the keys in a hash and they are:
#
#	J_DATE	float range (0 to the limit of perl installation)
#		J_DATE is the Julian decimal date
#	YEAR	integer range dependent upon machine implementation of perl
#		-4712 lower limit to upper limit size of integer
#	MONTH	integer range (1..12)
#	DAY	integer range (1..31)
#	HOUR	integer range (0..23)
#	MINUTE	integer range (0..61)
#	SECOND	float range (0..60) use int() if you want to consider it an int
#	D_HOUR	float range (0..23.999...) includes min and second as fraction
#	D_MINUTE	float range (0..60.9999) includes second as fraction
#	WEEKDAY	integer range (0..6)
#	DAY_OF_YEAR	integer range (1..366)
#
