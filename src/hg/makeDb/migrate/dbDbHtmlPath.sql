# Add htmlPath column to dbDb table
# This will have the format:  "/gbdb/<assembly>/html/<file>.html"
#	e.g.: "/gbdb/hg16/html/description.html"
#

ALTER TABLE dbDb ADD COLUMN htmlPath varchar(255) NOT NULL;

#	Discovered that other columns should also be NOT NULL


ALTER TABLE dbDb MODIFY scientificName varchar(255) NOT NULL;
ALTER TABLE dbDb MODIFY organism varchar(255) NOT NULL;
ALTER TABLE dbDb MODIFY active int(1) NOT NULL DEFAULT 0;
ALTER TABLE dbDb MODIFY orderKey int(11) NOT NULL DEFAULT 1000000;

#	The following current exist on the RR.  The Zoos could all be unique

UPDATE dbDb set htmlPath = "/gbdb/hg16/html/description.html" where name="hg16";
UPDATE dbDb set htmlPath = "/gbdb/hg15/html/description.html" where name="hg15";
UPDATE dbDb set htmlPath = "/gbdb/hg13/html/description.html" where name="hg13";
UPDATE dbDb set htmlPath = "/gbdb/hg12/html/description.html" where name="hg12";
UPDATE dbDb set htmlPath = "/gbdb/cb1/html/description.html" where name="cb1";
UPDATE dbDb set htmlPath = "/gbdb/ce1/html/description.html" where name="ce1";
UPDATE dbDb set htmlPath = "/gbdb/mm2/html/description.html" where name="mm2";
UPDATE dbDb set htmlPath = "/gbdb/mm3/html/description.html" where name="mm3";
UPDATE dbDb set htmlPath = "/gbdb/rn3/html/description.html" where name="rn3";
UPDATE dbDb set htmlPath = "/gbdb/rn2/html/description.html" where name="rn2";
UPDATE dbDb set htmlPath = "/gbdb/rn1/html/description.html" where name="rn1";
UPDATE dbDb set htmlPath = "/gbdb/sc1/html/description.html" where name="sc1";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooHuman3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooCat3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooChicken3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooBaboon3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooChimp3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooCow3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooDog3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooFugu3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooMouse3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooPig3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooRat3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooTetra3";
UPDATE dbDb set htmlPath = "/gbdb/zoo3/html/description.html" where name="zooZebrafish3";
