#!/usr/bin/perl

use warnings;
use strict;
use File::Copy "cp";

sub includeWordCount {
    my @lines = @{$_[0]};
    my $line = $_[1];
    unless (scalar (@lines) == 3 || scalar (@lines) == 2) {
        die "statement '$line' has improper spaces";
    }
}

sub processLines {

    my $new = $_[0];
    my $old = $_[1];
    my $stage = $_[2];

    my @return;

    if ($stage eq "beta") {
        $$new[2] = "alpha,beta";
        $$old[2] = "public";
    } elsif ($stage eq "public") {
        $$old[2] = "";
    }

}


sub processTrackDb {
    my $composite = $_[0];
    my $trackDb = $_[1];
    my $stage = $_[2];
    my $trackDbDir = $_[3];

    my @beforelines;
    my @afterlines;
    my @complines;
    my @returnlines;

    open TRACK, "$trackDb" or die "can't open trackDb file";
    
    my $arrayref = \@beforelines;
    while (<TRACK>){

        my $line = $_;
        chomp $line;
        if ($line =~ m/^\s*#/) {
            push @{$arrayref}, $line;
            next;
        }
        if ($line =~ m/$composite/){
            $arrayref = \@afterlines;
            push @complines, $line;
        } else {
            push @{$arrayref}, $line;
        }
    }
    close TRACK;
    if (scalar (@complines) == 1){
        my @lines = split /\s+/, $complines[0];
        &includeWordCount(\@lines, $complines[0]);
        if ($stage eq "beta") {
            $lines[2] = "alpha,beta";
        } elsif ($stage eq "public") {
            $lines[2] = "";
        }
        my $returnline = join " ", @lines;
        push @returnlines, $returnline;
    } elsif (scalar (@complines) == 2) {
        my @line1 = split /\s+/, $complines[0];
        my @line2 = split /\s+/, $complines[1];
        &includeWordCount(\@line1, $complines[0]);
        &includeWordCount(\@line2, $complines[1]);
        my $new;
        my $old;
        if ($line1[1] =~ m/wgEncode\S+new/) {
            $new = \@line1;
            $old = \@line2;
        } elsif ($line2[1] =~ m/wgEncode\S+new/) {
            $new = \@line2;
            $old = \@line1;
        } else {
            die "there are two include statements for $composite, neither of which have a .new";
        }
        if ($stage eq "public") {
            &moveForPublic($$new[1], $$old[1], $trackDbDir);
        }
        &processLines($new, $old, $stage);
        if ($stage eq "beta"){
            my $line1join = join " ", @line1;
            my $line2join = join " ", @line2;
            push @returnlines, $line1join;
            push @returnlines, $line2join;
        } elsif ($stage eq "public") {
            my $line1join = join " ", @line1;
            my $line2join = join " ", @line2;
            $line1join =~ s/\s+$//g;
            $line2join =~ s/\s+$//g;
            if ($line1join =~ m/wgEncode\S+new/) {
                push @returnlines, $line2join;
            } elsif ($line2join =~ m/wgEncode\S+new/) {
                push @returnlines, $line1join;
            }
        }
    } elsif (scalar (@complines) == 3) {
        die "you have a special situation where there are three include statements, you must proceed by hand";
    } elsif (scalar (@complines) > 3) {
        die "there are more than three include statements for $composite";
    }
    open TRACK, ">$trackDb";
    foreach my $printlines (@beforelines) {
        print TRACK "$printlines\n";
    } 
    foreach my $printlines (@returnlines) {
        print TRACK "$printlines\n";
    }
    foreach my $printlines (@afterlines) {
        print TRACK "$printlines\n";
    }
    close TRACK;
}

sub moveForPublic {
    my $new = $_[0];
    my $old = $_[1];
    my $dir = $_[2];
    my $newra = "$dir/$new";
    my $oldra = "$dir/$old";


    unless (-e $newra) {
        die "can't find $newra to copy to $oldra";
    }
    cp($newra,$oldra) or die "can't copy $newra to $oldra";

    open OLD, "$oldra";
    my @lines;

    while (<OLD>){

        my $line = $_;
        chomp $line;
        if ($line =~ m/^html (\S+)/) {
            my $html = "$dir/$1" . ".html";
            unless (-e $html) {
                die "html indicated by '$line' doesn't exist ($1.html)"
            }
            my $oldhtml = $oldra;
            $oldhtml =~ s/\.ra/\.html/g;
            cp($html,$oldhtml) or die "can't copy $html to $oldhtml";
            next
        }
        push @lines, $line;
    }
    close OLD;
    open OLD, ">$oldra";
    foreach my $printlines (@lines){
        print OLD "$printlines\n";
    }
    close OLD;

}

sub metaMakefileCheck {

    my $composite = $_[0];
    my $metaDbDir = $_[1];
    my $stage = $_[2];
    my $makefile = "$metaDbDir/$stage/makefile";

    open MAKE, "$makefile" or die "can't open makefile in $stage metaDb";
    
    print STDERR "checking makefile for $composite in $stage metaDb\n";

    my @lines;
    my $encode = 0;
    my $seen = 0;

    while (<MAKE>){

        my $line = $_;
        chomp $line;

        if ($line =~ m/^(wgEncode\S+[^\\])/){
            if ($1 =~ m/$composite.ra/){
                $seen = 1;
            }
            $encode = 1;
            push @lines, "$line";
            next;
        }
        if ($encode == 1 && $line =~ m/^$/){
            $encode = 0;
            unless ($seen) {
                push @lines, "$composite.ra \\";
                print STDERR "inserting $composite into makefile\n";
            } else {
                
                print "$composite found in makefile\n";
            }
            push @lines, $line;
        } else {
            push @lines, $line;
        }        
    }
    close MAKE;
    open MAKE, ">$makefile";

    foreach my $line (@lines) {
        print MAKE "$line\n";
    }
    close MAKE;
}

sub copyOverMetaDb {

    my $composite = $_[0];
    my $metaDbDir = $_[1];
    my $stage = $_[2];

    my $alpha = "$metaDbDir/alpha/$composite.ra";
    my $beta = "$metaDbDir/beta/$composite.ra";
    my $public = "$metaDbDir/public/$composite.ra";

    unless (-e $alpha) {
        die "$alpha does not exist";
    }
    my $status;

    #make a method to check if you're in a git repository

    if ($stage eq "beta" && !(-e $beta)) {
        cp($alpha, $beta) or die "can't copy $alpha to $beta";
        `git add $beta`;
        &getMetaDbDifferences($alpha, $beta, $composite, $stage);
        return;
    } elsif ($stage eq "beta" && (-e $beta)) {
        &getMetaDbDifferences($alpha, $beta, $composite, $stage);
        cp($alpha, $beta) or die "can't copy $alpha to $beta";
        return;
    }
    
    $status = &getMetaDbDifferences($alpha, $beta, $composite, $stage);
    if ($stage eq "public" && $status) {
        my $add = 0;
        unless (-e $public) {
            $add = 1;
        }
        cp($beta, $public) or die "can't copy $beta to $public";
        if ($add) {
            `git add $public`;
        }
    } elsif ($stage eq "public") {
        die "There are differences between the alpha and beta mdb, please resolve and then try again";
    }

}


#this needs work
sub getMetaDbDifferences {

    my $alpha = $_[0];
    my $beta = $_[1];
    my $composite = $_[2];
    my $stage = $_[3];
    my %alphastanzas = %{&readMdb($alpha)};
    my %betastanzas = %{&readMdb($beta)};
    

    my %seen;
    my %differences;
    my @newstanzas;
    my @droppedstanzas;
    foreach my $obj (sort keys %alphastanzas) {
        unless (exists $betastanzas{$obj}){
            push @newstanzas, "$obj";
        } else {
            my @differences;
            my %seenkeys;
            foreach my $key (sort keys %{$alphastanzas{$obj}}) {
                $seenkeys{$key} = 1;
                unless (exists ${$betastanzas{$obj}}{$key}) {
                    push @differences,  "added $key = ${$alphastanzas{$obj}}{$key} to alpha";
                } elsif (${$betastanzas{$obj}}{$key} ne ${$alphastanzas{$obj}}{$key}) {
                    push @differences, "changed $key from beta=${$betastanzas{$obj}}{$key} -> alpha=${$alphastanzas{$obj}}{$key}";
                }
            }
            foreach my $key (sort keys %{$betastanzas{$obj}}) {
                if (exists $seenkeys{$key}) {
                    next;
                }
                unless (exists ${$alphastanzas{$obj}}{$key}){
                        push @differences, "deleted $key = ${$betastanzas{$obj}}{$key} from alpha";
                    } else {
                    print "$obj $key\n";
                }
            }
            if (scalar @differences){
                $differences{$obj} = \@differences;
            }
        }
    }
    foreach my $obj (sort keys %betastanzas) {
        unless (exists $alphastanzas{$obj}) {
            push @droppedstanzas, "$obj";
        }
    }
    open DIFF, ">$composite.$stage.metaDb.diff";
    my $newnum = scalar(@newstanzas);
    print DIFF "New stanzas in alpha ($newnum):\n";
    foreach my $stanza (@newstanzas) {
    print DIFF "$stanza\n";
    }
    print DIFF "\n";
    my $droppednum = scalar(@droppedstanzas);
    print DIFF "Dropped stanzas from alpha ($droppednum):\n";
    foreach my $stanza (@droppedstanzas) {
        print DIFF "$stanza\n";
    }
    print DIFF "\n";
    my $changenum = scalar(keys %differences);
    print DIFF "Changed Stanzas between alpha and beta ($changenum):\n";
    foreach my $obj (sort keys %differences){
        print DIFF "$obj:\n";
        foreach my $val (sort @{$differences{$obj}}){
            print DIFF "\t$val\n";
        }
        print DIFF "\n";
    }
    close DIFF;

    if (scalar (keys %differences)){
        return (0);
    } else {
        return (1);
    }
}

sub readMdb {
    my $in = $_[0];
    open IN, "$in";
    my %stanzas;
    my $obj;
    while (<IN>) {
        my $line = $_;
        chomp $line;
        if ($line =~ m/MAGIC/) {
            next;
        }

        if ($line =~ m/metaObject (\S+)/) {
            $obj = $1;
            $stanzas{$obj} = {};
        } elsif ($line =~ m/(\S+) (.*)/) {
            ${$stanzas{$obj}}{$1} = $2;
        }
    }
    close IN;
    return (\%stanzas);
}


sub usage {
    
    print STDERR<<END;

usage: encodeQaPrepareRelease <db> <composite> <stage>
example: encodeQaPrepareRelease hg19 wgEncodeSydhTfbs beta

END
exit (1);

}



my $assm = $ARGV[0];
my $composite = $ARGV[1];
my $stage = $ARGV[2];
my $home = $ENV{"HOME"};

unless ((scalar @ARGV) == 3) {
    &usage();
}

my $species = "";
if ($assm =~ m/^mm\d+$/){
    $species = "mouse";
} elsif ($assm =~ m/^hg\d+$/) {
    $species = "human";
} else {
    die "wrong assembly name\n";
}


my $gitstat = `git status 2>&1`;

if ($gitstat =~ m/fatal/) {
    print STDERR<<END;
This program must be run in your git repository.
Please go to somewhere in the source tree.

END

    die
}


unless (($stage eq "beta") || ($stage eq "public")){
    die "stage must be either beta or public"

}
my $trackDb = "$home/kent/src/hg/makeDb/trackDb/$species/$assm/trackDb.wgEncode.ra";
my $trackDbDir = "$home/kent/src/hg/makeDb/trackDb/$species/$assm";
unless (-e $trackDb) {    
    die "cannot find $trackDb"
}

my $metaDbDir = "$trackDbDir/metaDb";

my $ls = `ls $trackDbDir`;
my @trackDbLs = split "\n", $ls;
my $hascomposite = 0;
foreach my $file (@trackDbLs) {
    if ($file =~ m/$composite/) {
        $hascomposite = 1;
    }
}
unless ($hascomposite) {
    die "cannot find any $composite .ra in $trackDbDir\n";
}

&metaMakefileCheck($composite, $metaDbDir, $stage);
&copyOverMetaDb($composite, $metaDbDir, $stage);
&processTrackDb($composite, $trackDb, $stage, $trackDbDir);


