
#A database of browser databases.
CREATE TABLE dbDb (
   name varchar(32) not null,	# Name of database
   description varchar(256) not null, # Short description (freeze date, etc.)
   nibPath varchar(256) not null, # Path to packed DNA files
     #Indices
   UNIQUE(name)
);

INSERT dbDb VALUES
   ("hg6", "Dec. 12, 2000", "/projects/hg2/gs.6/oo.27/nib"),
   ("hg7", "April 1 2001", "/projects/hg2/gs.7/oo.29/nib"),
   ("hg8", "Aug. 6, 2001", "/projects/hg2/gs.8/oo.33/nib");

CREATE TABLE blatServers (
   db varchar(32) not null,     # Name of database
   host varchar(128) not null,	# Host this is on
   port int not null,		# Port this is on
   isTrans tinyint not null,	# Set to 1 if translated
     #Indices
   UNIQUE(name)
);

INSERT blatServers VALUES
   ("hg6", "blat2.cse.ucsc.edu", 17778, 1),
   ("hg6", "blat2.cse.ucsc.edu", 17779, 0),
   ("hg7", "blat2.cse.ucsc.edu", 17778, 1),
   ("hg7", "blat2.cse.ucsc.edu", 17779, 0),
   ("hg8", "blat2.cse.ucsc.edu", 17778, 1),
   ("hg8", "blat2.cse.ucsc.edu", 17779, 0);
