#!/usr/bin/perl

if(@ARGV == 0) {
    print STDERR "makeLogosAndPostitionPlots.pl - Parse out different splicing\n";
    print STDERR "(alt5Prime, alt3Prime, cassette, control) types and create\n";
    print STDERR "different sequence logos and positional plots.\n";
    print STDERR "usage:\n";
    print STDERR "   makeLogosAndPostitionPlots.pl <bedPrefix> <db> <axtNetPrefix>\n";
    exit(1);
}

$bedPrefix = shift(@ARGV);
$db = shift(@ARGV);
$axtNetPrefix = shift(@ARGV);

if (! -e "all.upstream.out") {
    @chroms = `echo "select chrom from chromInfo" | mysql -uhguser -phguserstuff $db -A | egrep -v "chrom"`;
    while($c = shift(@chroms)) {
	chomp($c);
	print STDERR "Running axtCountBeds for $c\n";
	system("axtCountBeds $bedPrefix.upstream.bed $axtNetPrefix/axtNet/$c.axt $db $c.upstream.out $c");
	system("axtCountBeds $bedPrefix.downstream.bed $axtNetPrefix/axtNet/$c.axt $db $c.downstream.out $c");
	system("axtCountBeds $bedPrefix.alt.bed $axtNetPrefix/axtNet/$c.axt $db $c.alt.out $c");
	system("axtCountBeds $bedPrefix.const.bed $axtNetPrefix/axtNet/$c.axt $db $c.const.out $c");
    }
    print STDERR "Catting together.\n";
    system("cat chr*.upstream.out | egrep -v matchesSize > all.upstream.out");
    system("cat chr*.downstream.out | egrep -v matchesSize > all.downstream.out");
    system("cat chr*.alt.out | egrep -v matchesSize  > all.alt.out");
    system("cat chr*.const.out | egrep -v matchesSize > all.const.out");
}

# Note splice site types defined in altSpliceSite.h which corresponds 
# to 5th field or $p[4]
# enum altSpliceType
# /* Type of alternative splicing event. */
# {
#     alt5Prime,   /* 0 */
#     alt3Prime,   /* 1 */
#     altCassette, /* 2 */
#     altRetInt,   /* 3 */
#     altIdentity, /* 4 */
#     altOther,    /* 5 */
#     altControl   /* 6 */
# };

# Do the controls:
doNBasesInExon("control", 6, 30, "Constitutive", 50);
doNBasesEndExon("control", 6, 30, "Constitutive", 50);
doUpstream("control", 6, 20, 100, "Constitutive");
doDownstream("control", 6, 10, 100, "Constitutive");

# Do the cassettes
doNBasesInExon("cassette", 2, 30, "Cassette", 50);
doNBasesEndExon("cassette", 2, 30, "Cassette", 50);
doUpstream("cassette", 2, 20, 100, "Cassette");
doDownstream("cassette", 2, 10, 100, "Cassette");

# Alt3Prime
# upstream region
doUpstream("alt3Prime", 1, 20, 100, "Alternative 3'");
doDownstream("alt3Prime", 1, 10, 100, "Alternative 3'");

# 2nd upstream region
open(UP, "all.alt.out") or die "Can't open all.upstream.out to read.\n";
open(LOGO, ">upstream.alt3PrimeInExon.protseq") or die "Can't open upstream.alt3PrimeInExon.protseq to write.\n" ;
open(RDATA, ">upstream.alt3PrimeInExon.rdata")  or die "Can't open upstream.alt3PrimeInExon.rdata to write.\n" ;
print LOGO "*alt3PrimeInExon upstream\n";
while($cl = <UP>) {
    chomp($cl);
    @p = split(/\t/,$cl);
    @dna = split(//, $p[10]);
    $len = @dna;
    chop($p[12]);
    @pos = split(/,/, $p[12]);
    if($p[4] == 1 && $len > 20) {
	for($i=$len-20;$i<$len; $i++) {
	    print LOGO "$dna[$i]";
	    print RDATA "$pos[$i],";
	}
	print LOGO ".\n";
	print RDATA "\n";
    }
}
close(RDATA);
close(LOGO);
close(UP);
system("runMakelogo.pl upstream.alt3PrimeInExon.protseq 0 20 \"Sequences 20 Base Pairs 5' of Alt3PrimeInExon Exons\" alt3PrimeInExonUp.ps");

# 50bp of start and end of constant part.
open(CONST, "all.const.out") or die "Can't open all.const.out to read.\n";
open(RSTART, ">beginConstExon.50.alt3Prime.rdata")  or die "Can't open beginConstExon.50.alt3Prime.rdata to write.\n" ;
open(RFINISH, ">endConstExon.50.alt3Prime.rdata")  or die "Can't open endConstExon.50.alt3Prime.rdata to write.\n" ;
while($cl = <CONST>) {
    chomp($cl);
    @p = split(/\t/,$cl);
    @dna = split(//, $p[10]);
    $len = @dna;
    chop($p[12]);
    @pos = split(/,/, $p[12]);
    if($p[4] == 1 && $len > 25) {
	for($i=0;$i<25; $i++) {
	    print RSTART "$pos[$i],";
	}
	print RSTART "\n";
	for($i=$len-25;$i<$len; $i++) {
	    print RFINISH "$pos[$i],";
	}
	print RFINISH "\n";
    }
}
close(CONST);
close(RSTART);
close(RFINISH);

# Alt5Prime
# 3' region
doUpstream("alt5Prime", 0, 20, 100, "Alternative 5'");
doDownstream("alt5Prime", 0, 10, 100, "Alternative 5'");

# 2nd downstream region
open(UP, "all.alt.out") or die "Can't open all.downstream.out to read.\n";
open(LOGO, ">downstream.alt5PrimeInExon.protseq") or die "Can't open downstream.alt5PrimeInExon.protseq to write.\n" ;
open(RDATA, ">downstream.alt5PrimeInExon.rdata")  or die "Can't open downstream.alt5PrimeInExon.rdata to write.\n" ;
print LOGO "*alt5PrimeInExon downstream\n";
while($cl = <UP>) {
    chomp($cl);
    @p = split(/\t/,$cl);
    @dna = split(//, $p[10]);
    $len = @dna;
    chop($p[12]);
    @pos = split(/,/, $p[12]);
    if($p[4] == 0 && $len > 10) {
	for($i=0;$i<=10; $i++) {
	    print LOGO "$dna[$i]";
	}
	print LOGO ".\n";
    }
    if($p[4] == 0 && $len > 20) {
	for($i=0;$i<20; $i++) {
	    print RDATA "$pos[$i],";
	}
	print RDATA "\n";
    }
}
close(RDATA);
close(LOGO);
close(UP);
system("runMakelogo.pl downstream.alt5PrimeInExon.protseq 0 20 \"Sequences 20 Base Pairs 5' of Alt5PrimeInExon Exons\" alt5PrimeInExonDown.ps");

# 25bp of start and end of constant part.
open(CONST, "all.const.out") or die "Can't open all.const.out to read.\n";
open(RSTART, ">beginConstExon.50.alt5Prime.rdata")  or die "Can't open beginConstExon.50.alt5Prime.rdata to write.\n" ;
open(RFINISH, ">endConstExon.50.alt5Prime.rdata")  or die "Can't open endConstExon.50.alt5Prime.rdata to write.\n" ;
while($cl = <CONST>) {
    chomp($cl);
    @p = split(/\t/,$cl);
    @dna = split(//, $p[10]);
    $len = @dna;
    chop($p[12]);
    @pos = split(/,/, $p[12]);
    if($p[4] == 0 && $len > 25) {
	for($i=0;$i<25; $i++) {
	    print RSTART "$pos[$i],";
	}
	print RSTART "\n";
	for($i=$len-25;$i<$len; $i++) {
	    print RFINISH "$pos[$i],";
	}
	print RFINISH "\n";
    }
}
close(CONST);
close(RSTART);
close(RFINISH);

# Retained introns.
doNBasesInExon("retInt", 3, 11, "Retained Introns", 50);
doNBasesEndExon("retInt", 3, 20, "Retained Introns", 50);

sub doUpstream() {
    my $prefix = shift(@_);
    my $type = shift(@_);
    my $length = shift(@_);
    my $seqSize = shift(@_);
    my $name = shift(@_);
    my $cl;
    my @p;
    my $start;
    my $end;
    print STDERR "Doing $prefix\n";
    open(UP, "all.upstream.out") or die "Can't open all.upstream.out to read.\n";
    open(LOGO, ">upstream.$prefix.protseq") or die "Can't open upstream.$prefix.protseq to write.\n" ;
    open(RDATA, ">upstream.$prefix.rdata")  or die "Can't open upstream.$prefix.rdata to write.\n" ;
    print LOGO "*$prefix upstream\n";
    while($cl = <UP>) {
	chomp($cl);
	@p = split(/\t/,$cl);
	if($p[4] == $type) {
	    print LOGO "$p[10].\n";
	    print RDATA "$p[12]\n";
	}

    }
    close(LOGO);
    close(UP);
    close(RDATA);
    $start = $seqSize - $length;
    $end = $seqSize;
    system("runMakelogo.pl upstream.$prefix.protseq $start $end \"Sequences $length Base Pairs 5' of $name Exons\" $prefix.upstream.ps");
    print STDERR "Done\n";
}

sub doDownstream() {
    my $prefix = shift(@_);
    my $type = shift(@_);
    my $length = shift(@_);
    my $seqSize = shift(@_);
    my $name = shift(@_);
    my $cl;
    my @p;
    my $start;
    my $end;
    print STDERR "Doing $prefix\n";
    open(DOWN, "all.downstream.out") or die "Can't open all.downstream.out to read.\n";
    open(LOGO, ">downstream.$prefix.protseq") or die "Can't open downstream.$prefix.protseq to write.\n" ;
    open(RDATA, ">downstream.$prefix.rdata")  or die "Can't open downstream.$prefix.rdata to write.\n" ;
    print LOGO "*$prefix downstream\n";
    while($cl = <DOWN>) {
	chomp($cl);
	@p = split(/\t/,$cl);
	if($p[4] == $type) {
	    print LOGO "$p[10].\n";
	    print RDATA "$p[12]\n";
	}
    }
    close(LOGO);
    close(DOWN);
    close(RDATA);
    $start = 0;
    $end = $length;
    system("runMakelogo.pl downstream.$prefix.protseq $start $end \"Sequences $length Base Pairs 3' of $name Exons\" $prefix.downstream.ps");
    print STDERR "Done\n";
}

sub doNBasesInExon() {
    my $prefix = shift(@_);
    my $type = shift(@_);
    my $length = shift(@_);
    my $name = shift(@_);
    my $rLength = shift(@_);
    my $cl;
    my @p;
    my @pos;
    my $start;
    my $end;
    my $i;
    open(ALT, "all.alt.out") or die "Can't open all.alt.out to read.\n";
    open(LOGOSTART, ">alt.$length.bpStart.$prefix.protseq") or die "Can't open alt.$length.bpStart.$prefix.protseq to write.\n" ;
    open(RDATA, ">alt.$rLength.bpStart.$prefix.rdata") or die "Can't open alt.$length.bpStart.$prefix.rdata to write.\n" ;
    print LOGOSTART "*$prefix $length.bpStart start\n";
    while($cl = <ALT>) {
	chomp($cl);
	@p = split(/\t/,$cl);
	@dna = split(//, $p[10]);
	$len = @dna;
	chop($p[12]);
	@pos = split(/,/, $p[12]);
	$rLen = @pos;
	if($p[4] == $type && $len > $length) {
	    for($i=0;$i<$length; $i++) {
		print LOGOSTART "$dna[$i]";
	    }
	    print LOGOSTART ".\n";
	}
	if($p[4] == $type && $rLen > $rLength) {
	    for($i=0;$i<$rLength; $i++) {
		print RDATA "$pos[$i],";
	    }
	    print RDATA "\n";
	}
    }
    close(RDATA);
    close(LOGOSTART);
    close(ALT);
    system("runMakelogo.pl alt.$length.bpStart.$prefix.protseq 0 $length \"Frist $length Base Pairs $name Exons\" $prefix.$length.BpStart.ps");
}

sub doNBasesEndExon() {
    my $prefix = shift(@_);
    my $type = shift(@_);
    my $length = shift(@_);
    my $name = shift(@_);
    my $rLength = shift(@_);
    my $cl;
    my @p;
    my $start;
    my $end;
    my $i;
    open(ALT, "all.alt.out") or die "Can't open all.alt.out to read.\n";
    open(LOGOFINISH, ">alt.$length.bpFinish.$prefix.protseq") or die "Can't open alt.$length.bpFinish.$prefix.protseq to write.\n" ;
    print LOGOFINISH "*$prefix $length.bpFinish finish\n";
    open(RDATA, ">alt.$rLength.bpFinish.$prefix.rdata") or die "Can't open alt.$length.bpFinish.$prefix.rdata to write.\n" ;
    while($cl = <ALT>) {
	chomp($cl);
	@p = split(/\t/,$cl);
	@dna = split(//, $p[10]);
	$len = @dna;
	chop($p[12]);
	@pos = split(/,/, $p[12]);
	$rLen = @pos;
	if($p[4] == $type && $len > $length) {
	    for($i = $len-$length; $i < $len; $i++) {
		print LOGOFINISH "$dna[$i]";
	    }
	    print LOGOFINISH ".\n";
	}
	if($p[4] == $type && $rLen > $rLength) {
	    for($i= ($rLen - $rLength); $i < $rLen; $i++) {
		print RDATA "$pos[$i],";
	    }
	    print RDATA "\n";
	}
    }
    close(RDATA);
    close(LOGOFINISH);
    close(ALT);
    system("runMakelogo.pl alt.$length.bpFinish.$prefix.protseq 0 $length \"Last $length Base Pairs $name Exons\" $prefix.$length.BpFinish.ps");
}
