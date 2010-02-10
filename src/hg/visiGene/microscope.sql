#This was created by copying the pertinent parts from visigene.sql file in ~/kent/src/hg/visiGene/

#Location of image, typically a file directory
CREATE TABLE fileLocation (
    id int auto_increment not null,     # ID of location
    name longblob not null,     # Directory path usually
    
    PRIMARY KEY(id)
);

# Source of data - an external database, a contributor, etc.
CREATE TABLE submissionSource (
    id int auto_increment not null,
    name varchar(255) not null, 
              #Indices
    PRIMARY KEY(id),
    UNIQUE(name(32))
);


#Info on a batch of images submitted at once
CREATE TABLE submissionSet (
    id int auto_increment not null,     # ID of submission set
    name varchar(255) not null,  # Name of submission set
    contributors longblob not null,     # Comma separated list of contributors in format Kent W.J., Wu F.Y.
    submissionSource int not null, # Source of this submission
    privateUser int not null,   # ID of user allowed to view. If 0 all can see.
    copyright int not null,     # Copyright notice
              #Indices
    PRIMARY KEY(id),
    UNIQUE(name(32))
);

#Copyright information
CREATE TABLE copyright (
    id int auto_increment not null,     # ID of copyright
    notice longblob not null,   # Text of copyright notice
              #Indices
    PRIMARY KEY(id),
    INDEX(notice(26))
);

#Association between contributors and submissionSets
CREATE TABLE submissionContributor (
    submissionSet int not null, # ID in submissionSet table
    contributor int not null,   # ID in contributor table
              #Indices
    INDEX(submissionSet),
    INDEX(contributor)
);

#Info on contributor
CREATE TABLE contributor (
    id int auto_increment not null,     # ID of contributor
    name varchar(255) not null, # Name in format like Kent W.J.
              #Indices
    PRIMARY KEY(id),
    INDEX(name(8))
);

#A biological image file
CREATE TABLE imageFile (
    id int auto_increment not null,     # ID of imageFile
    fileName varchar(255) not null,     # Image file name not including directory
    priority float not null,    # Lower priorities are displayed first
    imageWidth int not null,    # width of image in pixels
    imageHeight int not null,   # height of image in pixels
    fullLocation int not null,  # Location of full-size image
    thumbLocation int not null, # Location of thumbnail-sized image
    submissionSet int not null, # Submission set this is part of
    submitId varchar(255) not null,     # ID within submission set
    PRIMARY KEY(id),
    INDEX(fullLocation,fileName(12)),
    INDEX(submissionSet),
    INDEX(submitId(12))
);

#An image.  There may be multiple images within an imageFile
CREATE TABLE image (
    id int auto_increment not null,     # ID of image
    submissionSet int not null, # Submission set this is part of
    imageFile int not null,     # ID of image file
    imagePos int not null,      # Position in image file, starting with 0
    PRIMARY KEY(id),
    INDEX(imageFile),
    INDEX(submissionSet)
);

