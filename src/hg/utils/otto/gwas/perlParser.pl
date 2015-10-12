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
                foreach $i (0 .. $#w) { 
                  $w[$i] =~ s/^\s*//;  $w[$i] =~ s/\s*$//; 
                  # ugh, clean out non-utf8 stuff before decoding utf8 into unicode: 
                  $w[$i] =~ s/\222/'/g;
                  $w[$i] =~ s/\366/o/g;
                  $w[$i] =~ s/\337/B/g;
                  $w[$i] =~ s/\226/-/g;
                  $w[$i] =~ s/\265/u/g;
                  $w[$i] =~ s/\374/u/g;
                  $w[$i] =~ s/\240/ /g;
                  $w[$i] =~ s/\302/A/g;
                  $w[$i] =~ s/\303/A/g;
                  $w[$i] =~ s/\237/B/g;
                  $w[$i] =~ s/\274/4/g;
                  $w[$i] =~ s/\357/i/g;
                  $w[$i] =~ s/\277/?/g;
                  $w[$i] =~ s/\266/P/g;
                  $w[$i] = decode_utf8($w[$i], Encode::FB_CROAK); 
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
