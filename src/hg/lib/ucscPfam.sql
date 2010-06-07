CREATE TABLE ucscPfam (
ucscId 	 varchar(255) NOT NULL,
start 	 int(11) NOT NULL,
end 	 int(11) NOT NULL,
domainName varchar(255) NOT NULL,
KEY 	 ucscId (ucscId(16)),
KEY 	 domainName (domainName(16))
) TYPE=MyISAM;
