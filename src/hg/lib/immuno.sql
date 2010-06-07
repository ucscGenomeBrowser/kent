CREATE TABLE immuno (
subjId          char(40),	# GSID ID 
InjPriInf       int(10), 	# Injections Prior to Infection 
SDayLastPTest   int(10), 	# Study Day of Last Peak Test 
LastPVisit      float,		# Last Peak Visit (Month) 
LastPCD4Blk     float,	 	# Last Peak CD4 Blocking 
LastPMNNeutral  int(10),	# Last Peak MN Neutralization 
LastPAntiGP120  float,		# Last Peak anti-gp120 
SDayLastTrTest  int(10),	# Study Day of Last Trough Test 
LastTrVisit     int(10),	# Last Trough Visit (Month) 
LastTrCD4Blk    float,		# Last Trough CD4 Blocking 
LastTrMnNeutral int(10), 	# Last Trough MN Neutralization 
LastTrAntiGP120 float,		# Last Trough anti-gp120 
KEY subjId(subjId)
) TYPE=MyISAM;
