# table for Non-standard Join Certificates
CREATE TABLE certificate (
  accession1 varchar(40) NOT NULL,	# First accession number
  accession2 varchar(40) NOT NULL,	# 2nd   accession number
  spanner varchar(40),			# Spanner
  evaluation varchar(40),		# Evaluation
  variation varchar(40),		# Variation
  varEvidence varchar(255),		# Variation Evidence
  contact varchar(255),			# contact info
  remark text,				# Remark
  comment text,				# Comment
  KEY  (accession1),
  KEY  (accession2),
  KEY  (spanner)
) TYPE=MyISAM;
