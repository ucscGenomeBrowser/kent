
# verbose.rb - handle verbose messages

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit the CVS'ed source at:
# $Header: /projects/compbio/cvsroot/kent/src/hg/utils/ruby/verbose.rb,v 1.1 2008/01/24 18:38:08 galt Exp $

# Global variables
$opt_verbose = 1

def verbose(level, string)
  if level <= $opt_verbose
    STDERR.puts string
  end
end

