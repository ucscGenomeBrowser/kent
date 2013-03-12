#!/bin/sh

if [ $# -ne 1 ]; then
    echo "usage: ./commonNames.sh <Nway.nh>"
    echo "Renames the names from UCSC database names to common names."
    echo "This depends upon the exact UCSC database names present in the"
    echo ".nh file.  Last updated to work with the 57way.nh file."
    exit 255
fi

export F=$1

/cluster/bin/phast/tree_doctor \
-r \
"hg19 -> Human ;
panTro3 -> Chimp ;
panTro4 -> Chimp ;
gorGor3 -> Gorilla ;
ponAbe2 -> Orangutan ;
rheMac2 -> Rhesus ;
rheMac3 -> Rhesus ;
nomLeu1 -> Gibbon ;
nomLeu3 -> Gibbon ;
papHam1 -> Baboon ;
saiBol1 -> Squirrel_monkey ;
calJac3 -> Marmoset ;
tarSyr1 -> Tarsier ;
micMur1 -> Mouse_lemur ;
otoGar1 -> Bushbaby ;
otoGar3 -> Bushbaby ;
tupBel1 -> TreeShrew ;
criGri1 -> Chinese_hamster ;
mm9 -> Mouse ;
mm10 -> Mouse ;
rn4 -> Rat ;
rn5 -> Rat ;
dipOrd1 -> Kangaroo_rat ;
hetGla1 -> Mole_rat ;
hetGla2 -> Mole_rat ;
cavPor3 -> Guinea_Pig ;
speTri1 -> Squirrel ;
speTri2 -> Squirrel ;
musFur1 -> Ferret ;
chiLan1 -> Chinchilla ;
oryCun2 -> Rabbit ;
ochPri2 -> Pika ;
susScr2 -> Pig ;
susScr3 -> Pig ;
vicPac1 -> Alpaca ;
turTru1 -> Dolphin ;
turTru2 -> Dolphin ;
oviAri1 -> Sheep ;
capHir1 -> Goat ;
camFer1 -> Bactrian_camel ;
bosTau6 -> Cow ;
bosTau7 -> Cow ;
equCab2 -> Horse ;
cerSim1 -> White_rhinoceros ;
felCat4 -> Cat ;
felCat5 -> Cat ;
canFam2 -> Dog ;
canFam3 -> Dog ;
ailMel1 -> Panda ;
myoLuc2 -> Microbat ;
pteVam1 -> Megabat ;
eptFus1 -> Big_brown_bat ;
eriEur1 -> Hedgehog ;
eriEur2 -> Hedgehog ;
conCri1 -> Star_nosed_mole ;
sorAra1 -> Shrew ;
loxAfr3 -> Elephant ;
eleEdw1 -> Cape_elephant_shrew ;
chrAsi1 -> Cape_golden_mole ;
orcOrc1 -> Killer_whale ;
proCap1 -> Rock_hyrax ;
echTel1 -> Tenrec ;
echTel2 -> Tenrec ;
dasNov2 -> Armadillo ;
dasNov3 -> Armadillo ;
triMan1 -> Manatee ;
choHof1 -> Sloth ;
macEug2 -> Wallaby ;
sarHar1 -> Tasmanian_devil ;
monDom5 -> Opossum ;
ornAna1 -> Platypus ;
colLiv1 -> Rock_pigeon ;
geoFor1 -> Medium_ground_finch ;
ficAlb1 -> Collared_flycatcher ;
amaVit1 -> Puerto_Rican_parrot ;
falChe1 -> Saker_falcon ;
falPer1 -> Peregrine_falcon ;
melUnd1 -> Budgerigar ;
melGal1 -> Turkey ;
galGal3 -> Chicken ;
galGal4 -> Chicken ;
taeGut1 -> Zebra_finch ;
allMis1 -> Alligator ;
croPor1 -> Crocodile ;
latCha1 -> Coelacanth ;
anoCar2 -> Lizard ;
chrPic1 -> Painted_turtle ;
xenTro3 -> X_tropicalis ;
tetNig2 -> Tetraodon ;
fr3 -> Fugu ;
oreNil1 -> Tilapia ;
oreNil2 -> Tilapia ;
gasAcu1 -> Stickleback ;
oryLat2 -> Medaka ;
xipMac1 -> Southern_platyfish ;
danRer7 -> Zebrafish ;
gadMor1 -> Atlantic_cod ;
petMar1 -> Lamprey ;
petMar2 -> Lamprey ;" \
	${F} | sed -e "s/X_trop/X._trop/"
