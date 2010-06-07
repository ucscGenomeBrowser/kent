CREATE TABLE domainDesc (
acc   	    varchar(255) NOT NULL,
name  	    varchar(255) NOT NULL,
description varchar(255) NOT NULL,
KEY 	 acc  (acc(16)),
KEY 	 name (name(16))
) TYPE=MyISAM;
