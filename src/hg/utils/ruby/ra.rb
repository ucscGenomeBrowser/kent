
# ra.rb - read .ra file into memory

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit the CVS'ed source at:
# $Header: /projects/compbio/cvsroot/kent/src/hg/utils/ruby/ra.rb,v 1.4 2008/04/10 19:50:59 larrym Exp $

require '/cluster/bin/scripts/err.rb'
require '/cluster/bin/scripts/verbose.rb'

def readRaFile(file)
  inRecord = false
  ra = []
  hash = {}
  keyword = nil
  f = File.open(file)
  f.readlines.each do |line|
    line.chomp!
    verbose 2, "#{line}"
    if line[0,1] == '#'
      next
    end
    if line == ""
      if inRecord
        ra << [keyword,hash]
        hash = {}
      end
      inRecord = false
    else
      spc = line.index(' ')
      if spc == nil
        errAbort "syntax error: invalid line in .ra file: #{line}\n"
      end
      word = line[0,spc]
      value = line[spc+1,line.length]
      #STDERR.puts "debug: #{word}=#{value}\n"  #debug
      unless inRecord
        inRecord = true
        keyword = value
      end
      hash[word] = value
    end
  end
  if inRecord
    ra << [keyword,hash]
  end
  f.close
  return ra
end



