
# err.rb - handle errors 

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit the CVS'ed source at:
# $Header: /projects/compbio/cvsroot/kent/src/hg/utils/ruby/err.rb,v 1.1 2008/01/24 18:38:08 galt Exp $

def errAbort(msg)
  STDERR.puts msg
  exit 1
end


