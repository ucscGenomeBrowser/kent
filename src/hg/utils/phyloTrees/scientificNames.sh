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
s/allSin1/Alligator_sinensis/;
s/amaVit1/Amazona_vittata/;
s/araMac1/Ara_macao/;
s/apiMel4/Apis_mellifera/;
s/macFas5/Macaca_fascicularis/;
s/camFer1/Camelus_ferus/;
s/canLup1/Canus_lupus/;
s/chlSab1/Chlorocebus_sabaeus/;
s/chlSab2/Chlorocebus_sabaeus/;
s/panHod1/Pantholops_hodgsonii/;
s/capHir1/Capra_hircus/;
s/equPrz1/Equus_przewalskii/;
s/chrAsi1/Chrysochloris_asiatica/;
s/colLiv1/Columba_livia/;
s/apaVit1/Apaloderma_vittatum/;
s/phoRub1/Phoenicopterus_ruber_ruber/;
s/opiHoa1/Opisthocomus_hoazin/;
s/tauEry1/Tauraco_erythrolophus/;
s/fulGla1/Fulmarus_glacialis/;
s/chaVoc1/Charadrius_vociferus/;
s/calAnn1/Calypte_anna/;
s/cucCan1/Cuculus_canorus/;
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
s/aquChr1/Aquila_chrysaetos_canadensis/;
s/halAlb1/Haliaeetus_albicilla/;
s/manVit1/Manacus_vitellinus/;
s/corBra1/Corvus_brachyrhynchos/;
s/acaChl1/Acanthisitta_chloris/;
s/cotJap1/Coturnix_japonica/;
s/lepDis1/Leptosomus_discolor/;
s/merNub1/Merops_nubicus/;
s/pelCri1/Pelecanus_crispus/;
s/phaCar1/Phalacrocorax_carbo/;
s/phaLep1/Phaethon_lepturus/;
s/pteGut1/Pterocles_gutturalis/;
s/nipNip1/Nipponia_nippon/;
s/egrGar1/Egretta_garzetta/;
s/pygAde1/Pygoscelis_adeliae/;
s/aptFor1/Aptenodytes_forsteri/;
s/carCri1/Cariama_cristata/;
s/mesUni1/Mesitornis_unicolor/;
s/eurHel1/Eurypyga_helias/;
s/balPav1/Balearica_pavonina_gibbericeps/;
s/chlUnd1/Chlamydotis_undulata/;
s/ficAlb1/Ficedula_albicollis/;
s/ficAlb2/Ficedula_albicollis/;
s/hapBur1/Haplochromis_burtoni/;
s/jacJac1/Jaculus_jaculus/;
s/nanGal1/Nannospalax_galili/;
s/lepOcu1/Lepisosteus_oculatus/;
s/stePar1/Stegastes_partitus/;
s/dicLab1/Dicentrarchus_labrax/;
s/picPub1/Picoides_pubescens/;
s/strCam1/Struthio_camelus/;
s/tinGut1/Tinamus_guttatus/;
s/colStr1/Colius_striatus/;
s/mayZeb1/Maylandia_zebra/;
s/micOch1/Microtus_ochrogaster/;
s/myoDav1/Myotis_davidii/;
s/rhiFer1/Rhinolophus_ferrumequinum/;
s/myoBra1/Myotis_brandtii/;
s/neoBri1/Neolamprologus_brichardi/;
s/ochPri3/Ochotona_princeps/;
s/octDeg1/Octodon_degus/;
s/odoRosDiv1/Odobenus_rosmarus_divergens/;
s/lepWed1/Leptonychotes_weddellii/;
s/orcOrc1/Orcinus_orca/;
s/phyCat1/Physeter_catodon/;
s/balAcu1/Balaenoptera acutorostrata scammoni/;
s/oryAfe1/Orycteropus_afer/;
s/oviAri3/Ovis_aries/;
s/panPan1/Pan_paniscus/;
s/pytBiv1/Python_bivittatus/;
s/cheMyd1/Chelonia_mydas/;
s/apaSpi1/Apalone_spinifera/;
s/anaPla1/Anas_platyrhynchos/;
s/tytAlb1/Tyto_alba/;
s/pelSin1/Pelodiscus_sinensis/;
s/pseHum1/Pseudopodoces_humilis/;
s/gavSte1/Gavia_stellata/;
s/capCar1/Caprimulgus_carolinensis/;
s/zonAlb1/Zonotrichia_albicollis/;
s/gavGan1/Gavialis_gangeticus/;
s/eidHel1/Eidolon_helvum/;
s/pteAle1/Pteropus_alecto/;
s/punNye1/Pundamilia_nyererei/;
s/takFla1/Takifugu_flavidus/;
s/sorAra2/Sorex_araneus/;
s/taeGut2/Taeniopygia_guttata/;
s/tupChi1/Tupaia_chinensis/;
s/chrPic2/Chrysemys_picta_bellii/;
s/xenTro7/Xenopus_tropicalis/;
s/astMex1/Astyanax_mexicanus/;
s/angJap1/Anguilla_japonica/;
s/calMil1/Callorhinchus_milii/;
s/perManBai1/Peromyscus_maniculatus_bairdii/;
s/panTig1/Panthera_tigris_altaica/;
s/bubBub1/Bubalus_bubalis/;
s/bosMut1/Bos_mutus/;
s/lipVex1/Lipotes_vexillifer/;
s/bosTau7/Bos_taurus/;
s/serCan1/Serinus_canaria/;
s/cynSem1/Cynoglossus_semilaevis/;
s/sebNig1/Sebastes_nigrocinctus/;
s/sebRub1/Sebastes_rubrivinctus/;
s/poeFor1/Poecilia_formosa/;
s/poeReg1/Poecilia_reticulata/;
s/xipMac1/Xiphophorus_maculatus/;'
}

export F=$1

sed 's/[a-z][a-z]*_//g; s/:[0-9\.][0-9\.]*//g; s/;//; /^ *$/d; s/(//g; s/)//g; s/,//g' ${F} \
    | xargs echo | tr '[ ]' '[\n]' | sort | while read DB
do
    sciName=`hgsql -N -e "select scientificName from dbDb where name=\"${DB}\";" hgcentraltest 2> /dev/null | sed -e 's/ /_/g;'`
    echo "$DB -> $sciName from hgcentraltest" 1>&2
    if [ "X${sciName}Y" = "XY" ]; then
       sciName=`notYetInDbDb $DB`
       echo "$DB -> $sciName from sed statement" 1>&2
    fi
    treeDocString="${treeDocString} $DB -> $sciName ;"
#    echo "$treeDocString" 1>&2
    echo "$treeDocString"
done | tail -1 | while read treeDocString
do
    /cluster/bin/phast/tree_doctor -r "${treeDocString}" ${F}
done | sed -e "s/00*)/)/g; s/00*,/,/g"
