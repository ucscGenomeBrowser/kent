my @saveIdx = (
    1, # PUBMEDID
    2, # FIRST AUTHOR
    3, # DATE
    4, # JOURNAL
    6, # STUDY
    7, # DISEASE/TRAIT
    8, # INITIAL SAMPLE SIZE
    9, # REPLICATION SAMPLE SIZE
    10, # REGION
    13, # REPORTED GENE(S)
    20, # STRONGEST SNP-RISK ALLELE
    26, # RISK ALLELE FREQUENCY
    27, # P-VALUE
    29, # P-VALUE (TEXT)
    30, # OR or BETA
    31, # 95% CI (TEXT)
    32, # PLATFORM [SNPS PASSING QC]
    33, # CNV
);
    my $snpIdx = 21;
    my $riskIdx = 20;

# Restore this if they change time formats again
#    use Time::Piece;
    while (<>) { 
                next if (/^\s*$/); 
                s/\r$//; 
                @w = split("\t"); 
                # Skip if SNPs column is empty
                next if ($w[$snpIdx] !~ /^rs\d+/); 
# Restore this if they change time formats again
#		$w[3] =  Time::Piece->strptime($w[3], '%d-%b-%Y')->strftime('%Y-%m-%d');

                $w[$snpIdx] =~ s/ //g; 
                my @snps = split(",", $w[$snpIdx]); 

                $w[$riskIdx] =~ s/\s+//g;

                # Keep only the columns we care about
                my @savedCols = ();
                foreach $i (@saveIdx) { 
                  $savedCols[@savedCols] = $w[$i];
                } 
                # trim leading/trailing spaces if any; 
                # convert the Unicode in titles to HTML because non-ASCII gives Galaxy trouble. 
                # NB: this conversion now handled externally by iconv.  If Galaxy can handle
                # HTML entities, though, that might be even better - we could replace use of
                # iconv with calls to HTML::Entities
                foreach $i (0 .. $#savedCols) { 
                  $savedCols[$i] =~ s/^\s*//;  $savedCols[$i] =~ s/\s*$//; 
                  @chars = split(//, $savedCols[$i]); 
                  $savedCols[$i] = ""; 
                  foreach $c (@chars) { 
                    if (ord($c) > 127) { 
                      $c = sprintf "&#%d;", ord($c); 
                    } 
                  $savedCols[$i] .= $c; 
                  } 
                } 
                foreach $s (@snps) { 
                  print join("\t", $s, @savedCols) . "\n"; 
                } 
              }
