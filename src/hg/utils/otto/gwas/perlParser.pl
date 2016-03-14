    use Time::Piece;
    while (<>) { 
                next if (/^\s*$/); 
                s/\r$//; 
                @w = split("\t"); 
                next if ($w[21] !~ /^rs\d+/); 
		$w[3] =  Time::Piece->strptime($w[3], '%d-%b-%Y')->strftime('%Y-%m-%d');

                #if ($w[3] =~ /^(\d+)\/(\d+)\/(\d+)$/) { # transform to mysql DATE 
             #     ($month, $day, $year) = ($1, $2, $3); 
             #     $w[3] = "$year-$month-$day"; 
             #   } else { die "Cant parse date ($w[3])\t" } 
                $w[21] =~ s/ //g; 
                my @snps = split(",", $w[21]); 
                # discard columns (use descending order): 
                foreach $i (28, 25, 24, 23, 22, 21, 19, 18, 17, 16, 15, 14, 12, 11, 5, 0) { 
                  splice(@w, $i, 1); 
                } 
                # trim leading/trailing spaces if any; 
                # convert the Unicode in titles to HTML because non-ASCII gives Galaxy trouble. 
                # NB: this conversion now handled externally by iconv.  If Galaxy can handle
                # HTML entities, though, that might be even better - we could replace use of
                # iconv with calls to HTML::Entities
                foreach $i (0 .. $#w) { 
                  $w[$i] =~ s/^\s*//;  $w[$i] =~ s/\s*$//; 
                  @chars = split(//, $w[$i]); 
                  $w[$i] = ""; 
                  foreach $c (@chars) { 
                    if (ord($c) > 127) { 
                      $c = sprintf "&#%d;", ord($c); 
                    } 
                  $w[$i] .= $c; 
                  } 
                } 
                foreach $s (@snps) { 
                  print join("\t", $s, @w) . "\n"; 
                } 
              }
