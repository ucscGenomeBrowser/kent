#!/bin/sh

if [ $# -ne 1 ]; then
    echo "usage: ./scientificNames.sh <Nway.nh>"
    echo "Renames the names from UCSC database names to the dbDb scientificNames names."
    echo "This depends upon the exact UCSC database names present in the"
    echo ".nh file.  Last updated to work with the 96way.nh file."
    exit 255
fi

notYetInDbDb() {
echo $1 | sed -e '
s/amaVit1/Amazona_vittata/;
s/araMac1/Ara_macao/;
s/apiMel4/Apis_mellifera/;
s/macFas5/Macaca_fascicularis/;
s/camFer1/Camelus_ferus/;
s/chlSab1/Chlorocebus_sabaeus/;
s/panHod1/Pantholops_hodgsonii/;
s/capHir1/Capra_hircus/;
s/chrAsi1/Chrysochloris_asiatica/;
s/colLiv1/Columba_livia/;
s/conCri1/Condylura_cristata/;
s/criGri1/Cricetulus_griseus/;
s/mesAur1/Mesocricetus_auratus/;
s/croPor1/Crocodylus_porosus/;
s/dm4/Drosophila_melanogaster/;
s/echTel2/Echinops_telfairi/;
s/eleEdw1/Elephantulus_edwardii/;
s/eptFus1/Eptesicus_fuscus/;
s/eriEur2/Erinaceus_europaeus/;
s/falChe1/Falco_cherrug/;
s/falPer1/Falco_peregrinus/;
s/ficAlb1/Ficedula_albicollis/;
s/ficAlb2/Ficedula_albicollis/;
s/hapBur1/Haplochromis_burtoni/;
s/jacJac1/Jaculus_jaculus/;
s/lepOcu1/Lepisosteus_oculatus/;
s/mayZeb1/Maylandia_zebra/;
s/micOch1/Microtus_ochrogaster/;
s/myoDav1/Myotis_davidii/;
s/myoBra1/Myotis_brandtii/;
s/neoBri1/Neolamprologus_brichardi/;
s/ochPri3/Ochotona_princeps/;
s/octDeg1/Octodon_degus/;
s/odoRosDiv1/Odobenus_rosmarus_divergens/;
s/lepWed1/Leptonychotes_weddellii/;
s/orcOrc1/Orcinus_orca/;
s/oryAfe1/Orycteropus_afer/;
s/oviAri3/Ovis_aries/;
s/panPan1/Pan_paniscus/;
s/cheMyd1/Chelonia_mydas/;
s/apaSpi1/Apalone_spinifera/;
s/anaPla1/Anas_platyrhynchos/;
s/pelSin1/Pelodiscus_sinensis/;
s/pseHum1/Pseudopodoces_humilis/;
s/zonAlb1/Zonotrichia_albicollis/;
s/gavGan1/Gavialis_gangeticus/;
s/pteAle1/Pteropus_alecto/;
s/punNye1/Pundamilia_nyererei/;
s/takFla1/Takifugu_flavidus/;
s/sorAra2/Sorex_araneus/;
s/taeGut2/Taeniopygia_guttata/;
s/tupChi1/Tupaia_chinensis/;
s/xenTro7/Xenopus_tropicalis/;
s/astMex1/Astyanax_mexicanus/;
s/xipMac1/Xiphophorus_maculatus/;'
}

export F=$1

sed 's/[a-z][a-z]*_//g; s/:[0-9\.][0-9\.]*//g; s/;//; /^ *$/d; s/(//g; s/)//g; s/,//g' ${F} \
    | xargs echo | tr '[ ]' '[\n]' | sort | while read DB
do
    sciName=`hgsql -N -e "select scientificName from dbDb where name=\"${DB}\";" hgcentraltest 2> /dev/null | sed -e 's/ /_/g;'`
    if [ "X${sciName}Y" = "XY" ]; then
       sciName=`notYetInDbDb $DB`
    fi
    treeDocString="${treeDocString} $DB -> $sciName ;"
    echo "$treeDocString"
done | tail -1 | while read treeDocString
do
    /cluster/bin/phast/tree_doctor -r "${treeDocString}" ${F}
done | sed -e "s/00*)/)/g; s/00*,/,/g"

# petMar2 -> Lamprey ;" \
#	${F} | sed -e "s/X_trop/X._trop/; s/Burton_s/Burton's/; s/David_s/David's/;"
