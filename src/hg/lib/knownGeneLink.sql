# This table store the seqType info for Known Genes track entries
# The seqType for mRNA based genes is 'm'  
# The seqType for DNA based genes is 'g'  
CREATE TABLE knownGeneLink (
  name varchar(40) NOT NULL default '',		# Known Genes entry ID
  seqType char(1) NOT NULL default 'u',		# Known Gene sequence type
  proteinID varchar(40) NOT NULL default '',	# Corresponding Protein ID
  KEY name (name(10)),				
  KEY proteinID (proteinID(10))
) TYPE=MyISAM;

