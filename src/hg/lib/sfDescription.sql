# This table stores domain descriptions for the Supfamily track
# A single Superfamily entry may contain multiple domains
CREATE TABLE sfDescription (
  name varchar(40) NOT NULL default '',		# ID of a Superfamily entry, 
						# same as Ensembl transcript name
  proteinID varchar(40) NOT NULL default '',	# ID of the corresponding Ensembl translation
  description varchar(255) NOT NULL default '', # Description of the domain
  KEY name (name),
  KEY protein (proteinID)
) TYPE=MyISAM;

