#!/usr/local/bin/perl5

# Program : getJoinerKeyErrors.pl
# Purpose : parses output of joinerCheck and captures Errors and associated lines
# Results : http://hgwdev.cse.ucsc.edu/qa/test-results/joinerCheck_monitor/keys/
# Author  : Jennifer Jackson
# Int Date: 2005-03-04
# Rev Date: 2005-11-XX
# Author  : Ann Zweig
# Rev Date: 2006-02-13 - added results to HTML page
# Results : http://hgwdev.cse.ucsc.edu/qa/test-results/joinerCheck_monitor/keys/joinerKeyErrors.YYMMDD

use File::Copy;


if ($#ARGV != 0) { die
"\nUSAGE: $0 /hive/groups/qa/joinerCheck/keys/YYYYMM 
	run on dev, use path exactly as shown in usage (YYYYMM ex: 200503)
        parses output of 'joinerCheck -keys' and captures errors and associated lines
	parses all files in dir, input/ingored file names are listed in output file report 
       	report is in target directory: joinerKeyErrors.YYMMDD 
	STDERR is sent to screen
	reports can be viewed online at:
	http://hgwdev.cse.ucsc.edu/qa/test-results/joinerCheck_monitor/keys/YYYYMM/*\n\n";}

# State the program has started
print STDERR "PROCESSING: $0 program started, locating directory & input files\n";


# get input file names     
chomp($in_dir = $ARGV[0]);
if ($in_dir =~ /\/hive\/groups\/qa\/joinerCheck\/keys\/[0-9]{6}/) {
   $path = $in_dir;
}  else {
   die "ERROR: Path to directory incorrectly formatted $in_dir\n";
}

# get date
($day,$month,$year) = (localtime)[3,4,5];
$f_month = ($month+1);
$f_year  = (($year+1900)-2000);
if (length($day) != 2) { $p_day = "0$day"; } else { $p_day = $day; } 
if (length($f_year) != 2) { $p_year = "0$f_year"; } else {$p_year = $f_year; }
if (length($f_month) != 2) { $p_month = "0$f_month"; } else {$p_month = $f_month; }
#print "$p_year$p_month$p_day\n";


# open outfile for writing
$outfile = "$path/joinerKeyErrors.$p_year$p_month$p_day";
open OUT, ">$outfile" || die "ERROR: Cannot open $outfile for writing\n";
print OUT "$outfile\n\n";
print OUT "Files evaluated and associated errors----------\n\n";

$skipped_count     = 0;
$not_skipped_count = 0;


# open dir, open each file and extract error info
opendir(DIR, $path) || die "ERROR: Cannot open $path for reading\n";

while (defined($file = readdir(DIR))) {

if (($file =~ /^\./) || ($file =~ /^$/)) {
  next;
} else { 
 open(FILE,"$path/$file"); 

 $line        = 0;
 $errors      = 0;
 $non_errors  = 0;
 $examples    = 0;
 $skipped     = "";
 $not_skipped = "";
 $loops       = 0;
 $noloop      = 0;
 $notfirst    = 0;
 
 while(<FILE>) {
  chomp;
  s/^ //;
  @row = split(/ /, $_);
  $line++; 
  
  if (($line == 1) && ($row[0] eq "Checking")) {
     print OUT "\n$file errors found (if any):\n"; 
     $skipped = 1;
     $not_skipped_count++;
   } elsif (($line == 1) && ($row[0] ne "Checking")) {
     print OUT "\n$file skipped (not joinerCheck output)\n";
     $skipped = 2;
     $skipped_count++;
   } else {
     $notfirst++;
   }


  if (($line > 1) && ($skipped == 1)) {
      $loops++;
      if ($row[0] eq "Error:") {
         $last_error_line = $line;
         $errors++;
         print OUT "  $_\n";
       } elsif (($line == ($last_error_line + 1)) && ($row[0] eq "Example")) {
         $examples++;    
         print OUT "  $_\n";
       } else {
         $non_errors++;
       }
    } else {
      $noloop++;
    } 
  
  next;
 }

 if ($skipped == 1) {  
 print OUT "Total lines(including header):$line, errors:$errors, examples:$examples, non-errors:$non_errors\n\n";
 #print OUT "Loops:$loops, Ingored Lines in skipped files:$noloop, Not first: $notfirst\n\n";    
 }
}
}

print OUT "\n\nTotal number of files evaluated for errors: $not_skipped_count\n";
print OUT "Total number of files not evaluated (skipped): $skipped_count\n";


print STDERR "PROCESSING: $0 program completed successfully.\nOutput: $outfile\n";

closedir(DIR);

# copy results to html location
$oldlocation = "joinerKeyErrors.$p_year$p_month$p_day";
$newlocation = "/usr/local/apache/htdocs/qa/test-results/joinerCheck_monitor/keys/joinerKeyErrors.$p_year$p_month$p_day";
copy($oldlocation, $newlocation);

