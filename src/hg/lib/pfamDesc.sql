CREATE TABLE pfamDesc (
pfamAC   	    varchar(40) NOT NULL,
pfamID  	    varchar(40) NOT NULL,
description varchar(255) NOT NULL,
KEY 	    pfamAC  (pfamAC(16)),
KEY 	    pfamID (pfamID(16))
);
