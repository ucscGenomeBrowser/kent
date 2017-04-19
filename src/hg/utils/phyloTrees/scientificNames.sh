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
s/acaChl1/Acanthisitta_chloris/;
s/aciJub1/Acinonyx_jubatus/;
s/allSin1/Alligator_sinensis/;
s/amaVit1/Amazona_vittata/;
s/anaPla1/Anas_platyrhynchos/;
s/angJap1/Anguilla_japonica/;
s/aotNan1/Aotus_nancymaae/;
s/apaSpi1/Apalone_spinifera/;
s/apaVit1/Apaloderma_vittatum/;
s/apiMel4/Apis_mellifera/;
s/aptFor1/Aptenodytes_forsteri/;
s/aquChr1/Aquila_chrysaetos_canadensis/;
s/aquChr2/Aquila_chrysaetos_canadensis/;
s/araMac1/Ara_macao/;
s/astMex1/Astyanax_mexicanus/;
s/balAcu1/Balaenoptera_acutorostrata_scammoni/;
s/balPav1/Balearica_pavonina_gibbericeps/;
s/bisBis1/Bison_bison_bison/;
s/bosInd1/Bos_indicus/;
s/bosMut1/Bos_mutus/;
s/bosTau7/Bos_taurus/;
s/bosTau8/Bos_taurus/;
s/bubBub1/Bubalus_bubalis/;
s/bucRhi1/Buceros_rhinoceros_silvestris/;
s/calAnn1/Calypte_anna/;
s/calMil1/Callorhinchus_milii/;
s/camBac1/Camelus_bactrianus/;
s/camDro1/Camelus_dromedarius/;
s/camFer1/Camelus_ferus/;
s/canLup1/Canus_lupus/;
s/capCar1/Caprimulgus_carolinensis/;
s/capHir1/Capra_hircus/;
s/carCri1/Cariama_cristata/;
s/catAur1/Cathartes_aura/;
s/cavApe1/Cavia_aperea/;
s/cebCap1/Cebus_capucinus_imitator/;
s/cerAty1/Cercocebus_atys/;
s/chaPel1/Chaetura_pelagica/;
s/chaVoc1/Charadrius_vociferus/;
s/chaVoc2/Charadrius_vociferus/;
s/cheMyd1/Chelonia_mydas/;
s/chlSab1/Chlorocebus_sabaeus/;
s/chlSab2/Chlorocebus_sabaeus/;
s/chlUnd1/Chlamydotis_undulata/;
s/chrAsi1/Chrysochloris_asiatica/;
s/chrPic2/Chrysemys_picta_bellii/;
s/colLiv1/Columba_livia/;
s/colStr1/Colius_striatus/;
s/conCri1/Condylura_cristata/;
s/corBra1/Corvus_brachyrhynchos/;
s/corCor1/Corvus_cornix_cornix/;
s/cotJap1/Coturnix_japonica/;
s/criGri1/Cricetulus_griseus/;
s/croPor0/Crocodylus_porosus/;
s/croPor1/Crocodylus_porosus/;
s/croPor2/Crocodylus_porosus/;
s/cucCan1/Cuculus_canorus/;
s/cynSem1/Cynoglossus_semilaevis/;
s/cypVar1/Cyprinodon_variegatus/;
s/dicLab1/Dicentrarchus_labrax/;
s/dipOrd2/Dipodomys_ordii/;
s/dm4/Drosophila_melanogaster/;
s/echTel2/Echinops_telfairi/;
s/egrGar1/Egretta_garzetta/;
s/eidHel1/Eidolon_helvum/;
s/eleEdw1/Elephantulus_edwardii/;
s/eptFus1/Eptesicus_fuscus/;
s/equAsi1/Equus_asinus/;
s/equPrz1/Equus_przewalskii/;
s/eriEur2/Erinaceus_europaeus/;
s/esoLuc1/Esox_lucius/;
s/eurHel1/Eurypyga_helias/;
s/falChe1/Falco_cherrug/;
s/falPer1/Falco_peregrinus/;
s/ficAlb1/Ficedula_albicollis/;
s/ficAlb2/Ficedula_albicollis/;
s/fukDam1/Fukomys_damarensis/;
s/fulGla1/Fulmarus_glacialis/;
s/gavGan0/Gavialis_gangeticus/;
s/gavGan1/Gavialis_gangeticus/;
s/gavSte1/Gavia_stellata/;
s/halAlb1/Haliaeetus_albicilla/;
s/halLeu1/Haliaeetus_leucocephalus/;
s/hapBur1/Haplochromis_burtoni/;
s/hipArm1/Hipposideros_armiger/;
s/jacJac1/Jaculus_jaculus/;
s/lepDis1/Leptosomus_discolor/;
s/lepOcu1/Lepisosteus_oculatus/;
s/lepWed1/Leptonychotes_weddellii/;
s/lipVex1/Lipotes_vexillifer/;
s/macFas5/Macaca_fascicularis/;
s/manJav1/Manis_javanica/;
s/manLeu1/Mandrillus_leucophaeus/;
s/manVit1/Manacus_vitellinus/;
s/manVit2/Manacus_vitellinus/;
s/marMar1/Marmota_marmota_marmota/;
s/mayZeb1/Maylandia_zebra/;
s/melGal5/Meleagris_gallopavo/;
s/merNub1/Merops_nubicus/;
s/mesAur1/Mesocricetus_auratus/;
s/mesUni1/Mesitornis_unicolor/;
s/micOch1/Microtus_ochrogaster/;
s/minNat1/Miniopterus_natalensis/;
s/musPut1/Mustela_putorius_furo/;
s/myoBra1/Myotis_brandtii/;
s/myoDav1/Myotis_davidii/;
s/nanGal1/Nannospalax_galili/;
s/nanPar1/Nanorana_parkeri/;
s/nasLar1/Nasalis_larvatus/;
s/neoBri1/Neolamprologus_brichardi/;
s/nesNot1/Nestor_notabilis/;
s/nipNip1/Nipponia_nippon/;
s/ochPri3/Ochotona_princeps/;
s/octDeg1/Octodon_degus/;
s/odoRosDiv1/Odobenus_rosmarus_divergens/;
s/opiHoa1/Opisthocomus_hoazin/;
s/orcOrc1/Orcinus_orca/;
s/ornAna5/Ornithorhynchus_anatinus/;
s/oryAfe1/Orycteropus_afer/;
s/oviAri3/Ovis_aries/;
s/panHod1/Pantholops_hodgsonii/;
s/panPan1/Pan_paniscus/;
s/panPar1/Panthera_pardus/;
s/panTig1/Panthera_tigris_altaica/;
s/pelCri1/Pelecanus_crispus/;
s/pelSin1/Pelodiscus_sinensis/;
s/perManBai1/Peromyscus_maniculatus_bairdii/;
s/phaCar1/Phalacrocorax_carbo/;
s/phaLep1/Phaethon_lepturus/;
s/phoRub1/Phoenicopterus_ruber_ruber/;
s/phyCat1/Physeter_catodon/;
s/picPub1/Picoides_pubescens/;
s/podCri1/Podiceps_cristatus/;
s/poeFor1/Poecilia_formosa/;
s/poeReg1/Poecilia_reticulata/;
s/ponAbe3/Pongo_abelii/;
s/proCap2/Procavia_capensis/;
s/proCoq1/Propithecus_coquereli/;
s/pseHum1/Pseudopodoces_humilis/;
s/pteAle1/Pteropus_alecto/;
s/pteGut1/Pterocles_gutturalis/;
s/pteVam2/Pteropus_vampyrus/;
s/punNye1/Pundamilia_nyererei/;
s/pygAde1/Pygoscelis_adeliae/;
s/pytBiv1/Python_bivittatus/;
s/rhiFer1/Rhinolophus_ferrumequinum/;
s/rhiRox1/Rhinopithecus_roxellana/;
s/rhiSin1/Rhinolophus_sinicus/;
s/sebNig1/Sebastes_nigrocinctus/;
s/sebRub1/Sebastes_rubrivinctus/;
s/serCan1/Serinus_canaria/;
s/sorAra2/Sorex_araneus/;
s/stePar1/Stegastes_partitus/;
s/strCam1/Struthio_camelus/;
s/taeGut2/Taeniopygia_guttata/;
s/takFla1/Takifugu_flavidus/;
s/tauEry1/Tauraco_erythrolophus/;
s/tinGut1/Tinamus_guttatus/;
s/tinGut2/Tinamus_guttatus/;
s/tupChi1/Tupaia_chinensis/;
s/tytAlb1/Tyto_alba/;
s/vipBer1/Vipera_berus_berus/;
s/xenTro7/Xenopus_tropicalis/;
s/xipMac1/Xiphophorus_maculatus/;
s/zonAlb1/Zonotrichia_albicollis/;'
}

export F=$1

sed 's/[a-z][a-z]*_//g; s/:[0-9\.][0-9\.]*//g; s/;//; /^ *$/d; s/(//g; s/)//g; s/,//g' ${F} \
    | xargs echo | tr '[ ]' '[\n]' | sort | while read DB
do
    sciName=`hgsql -N -e "select scientificName from dbDb where name=\"${DB}\";" hgcentraltest 2> /dev/null | sed -e 's/ /_/g;'`
    if [ "X${sciName}Y" = "XY" ]; then
       sciName=`notYetInDbDb $DB`
       if [ "${DB}" = "${sciName}" ]; then
         sciName="${DB}_missing"
         echo "$DB -> ${sciName}" 1>&2
       else 
         echo "$DB -> $sciName from sed statement" 1>&2
       fi
    else
       echo "$DB -> $sciName from hgcentraltest" 1>&2
    fi
    treeDocString="${treeDocString} $DB -> $sciName ;"
#    echo "$treeDocString" 1>&2
    echo "$treeDocString"
done | tail -1 | while read treeDocString
do
    /cluster/bin/phast/tree_doctor -r "${treeDocString}" ${F}
done | sed -e "s/00*)/)/g; s/00*,/,/g"
