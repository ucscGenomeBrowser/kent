#
# EnsGeneAutomate: mapping Ensembl species/gff file names to UCSC db
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/EnsGeneAutomate.pm instead.

# $Id: EnsGeneAutomate.pm,v 1.11 2008/08/07 21:50:42 hiram Exp $
package EnsGeneAutomate;

use warnings;
use strict;
use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

# This is a listing of the public methods and variables (which should be
# treated as constants) exported by this module:
@EXPORT_OK = (
    # Support for common command line options:
    qw( ensVersionList ensGeneVersioning
      ),
);

#	Location of Ensembl FTP site for the releases
my $ensemblFTP = "ftp://ftp.ensembl.org/pub/";

#	version to date relationship for Ensembl archive reference
my @verToDate;
$verToDate[27] = "dec2004";
$verToDate[32] = "jul2005";
$verToDate[33] = "sep2005";
$verToDate[34] = "oct2005";
$verToDate[35] = "nov2005";
$verToDate[37] = "feb2006";
$verToDate[38] = "apr2006";
$verToDate[39] = "jun2006";
$verToDate[41] = "oct2006";
$verToDate[42] = "dec2006";
# before 43 there are no GTF files
$verToDate[43] = "feb2007";
$verToDate[44] = "apr2007";
$verToDate[45] = "jun2007";
$verToDate[46] = "aug2007";
$verToDate[47] = "oct2007";
$verToDate[48] = "dec2007";
$verToDate[49] = "mar2008";
$verToDate[50] = "jul2008";

#	older versions for archive purposes, there are different
#	directory structures for these, thus, the full path name
#	to append to the release-NN/ top level directory.
#  Fugu fr1 needs help here since there is no GTF file, fetch it from
#	EnsMart
my %ensGeneFtpFileNames_35 = (
'fr1' => 'fugu_rubripes_35_2g/data/fasta/dna/README',
);
my %ensGeneFtpPeptideFileNames_35 = (
'fr1' => 'fugu_rubripes_35_2g/data/fasta/pep/Fugu_rubripes.FUGU2.nov.pep.fa.gz',
);
my %ensGeneFtpMySqlFileNames_35 = (
'fr1' => 'fugu_rubripes_35_2g/data/mysql/fugu_rubripes_core_35_2g/assembly.txt.table.gz',
);

my %ensGeneFtpFileNames_46 = (
'mm8' => 'mus_musculus_46_36g/data/gtf/Mus_musculus.NCBIM36.46.gtf.gz',
);
my %ensGeneFtpPeptideFileNames_46 = (
'mm8' => 'mus_musculus_46_36g/data/fasta/pep/Mus_musculus.NCBIM36.46.pep.all.fa.gz',
);
my %ensGeneFtpMySqlFileNames_46 = (
'mm8' => 'mus_musculus_46_36g/data/mysql/mus_musculus_core_46_36g',
);

#	This listings are created by going to the FTP site and running
#	an ls on the gtf directory.  Edit that listing into this hash.
# key is UCSC db name, result is FTP file name under the gtf directory
my %ensGeneFtpFileNames_47 = (
'aedAeg0' => 'Aedes_aegypti.AaegL1.47.gtf.gz',
'anoGam2' => 'Anopheles_gambiae.AgamP3.47.gtf.gz',
'bosTau3' => 'Bos_taurus.Btau_3.1.47.gtf.gz',
'ce5' => 'Caenorhabditis_elegans.WS180.47.gtf.gz',
'canFam2' => 'Canis_familiaris.BROADD2.47.gtf.gz',
'cavPor2' => 'Cavia_porcellus.GUINEAPIG.47.gtf.gz',
'ci2' => 'Ciona_intestinalis.JGI2.47.gtf.gz',
'cioSav2' => 'Ciona_savignyi.CSAV2.0.47.gtf.gz',
'danRer5' => 'Danio_rerio.ZFISH7.47.gtf.gz',
'dasNov1' => 'Dasypus_novemcinctus.ARMA.47.gtf.gz',
'dm4' => 'Drosophila_melanogaster.BDGP4.3.47.gtf.gz',
'echTel1' => 'Echinops_telfairi.TENREC.47.gtf.gz',
'eriEur1' => 'Erinaceus_europaeus.HEDGEHOG.47.gtf.gz',
'felCat3' => 'Felis_catus.CAT.47.gtf.gz',
'galGal3' => 'Gallus_gallus.WASHUC2.47.gtf.gz',
'gasAcu1' => 'Gasterosteus_aculeatus.BROADS1.47.gtf.gz',
'hg18' => 'Homo_sapiens.NCBI36.47.gtf.gz',
'loxAfr1' => 'Loxodonta_africana.BROADE1.47.gtf.gz',
'rheMac2' => 'Macaca_mulatta.MMUL_1.47.gtf.gz',
'monDom5' => 'Monodelphis_domestica.BROADO5.47.gtf.gz',
'mm9' => 'Mus_musculus.NCBIM37.47.gtf.gz',
'myoLuc1' => 'Myotis_lucifugus.MICROBAT1.47.gtf.gz',
'ornAna1' => 'Ornithorhynchus_anatinus.OANA5.47.gtf.gz',
'oryCun1' => 'Oryctolagus_cuniculus.RABBIT.47.gtf.gz',
'oryLat1' => 'Oryzias_latipes.MEDAKA1.47.gtf.gz',
'otoGar1' => 'Otolemur_garnettii.BUSHBABY1.47.gtf.gz',
'panTro2' => 'Pan_troglodytes.CHIMP2.1.47.gtf.gz',
'rn4' => 'Rattus_norvegicus.RGSC3.4.47.gtf.gz',
'sacCer1' => 'Saccharomyces_cerevisiae.SGD1.01.47.gtf.gz',
'sorAra0' => 'Sorex_araneus.COMMON_SHREW1.47.gtf.gz',
'speTri0' => 'Spermophilus_tridecemlineatus.SQUIRREL.47.gtf.gz',
'fr2' => 'Takifugu_rubripes.FUGU4.47.gtf.gz',
'tetNig1' => 'Tetraodon_nigroviridis.TETRAODON7.47.gtf.gz',
'tupBel1' => 'Tupaia_belangeri.TREESHREW.47.gtf.gz',
'xenTro2' => 'Xenopus_tropicalis.JGI4.1.47.gtf.gz',
);

my %ensGeneFtpPeptideFileNames_47 = (
'aedAeg0' => 'aedes_aegypti_47_1a/pep/Aedes_aegypti.AaegL1.47.pep.all.fa.gz',
'anoGam2' => 'anopheles_gambiae_47_3i/pep/Anopheles_gambiae.AgamP3.47.pep.all.fa.gz',
'bosTau3' => 'bos_taurus_47_3d/pep/Bos_taurus.Btau_3.1.47.pep.all.fa.gz',
'ce5' => 'caenorhabditis_elegans_47_180/pep/Caenorhabditis_elegans.WS180.47.pep.all.fa.gz',
'canFam2' => 'canis_familiaris_47_2e/pep/Canis_familiaris.BROADD2.47.pep.all.fa.gz',
'cavPor2' => 'cavia_porcellus_47_1b/pep/Cavia_porcellus.GUINEAPIG.47.pep.all.fa.gz',
'ci2' => 'ciona_intestinalis_47_2g/pep/Ciona_intestinalis.JGI2.47.pep.all.fa.gz',
'cioSav2' => 'ciona_savignyi_47_2e/pep/Ciona_savignyi.CSAV2.0.47.pep.all.fa.gz',
'danRer5' => 'danio_rerio_47_7a/pep/Danio_rerio.ZFISH7.47.pep.all.fa.gz',
'dasNov1' => 'dasypus_novemcinctus_47_1d/pep/Dasypus_novemcinctus.ARMA.47.pep.all.fa.gz',
'dm4' => 'drosophila_melanogaster_47_43b/pep/Drosophila_melanogaster.BDGP4.3.47.pep.all.fa.gz',
'echTel1' => 'echinops_telfairi_47_1d/pep/Echinops_telfairi.TENREC.47.pep.all.fa.gz',
'eriEur1' => 'erinaceus_europaeus_47_1b/pep/Erinaceus_europaeus.HEDGEHOG.47.pep.all.fa.gz',
'felCat3' => 'felis_catus_47_1b/pep/Felis_catus.CAT.47.pep.all.fa.gz',
'galGal3' => 'gallus_gallus_47_2e/pep/Gallus_gallus.WASHUC2.47.pep.all.fa.gz',
'gasAcu1' => 'gasterosteus_aculeatus_47_1d/pep/Gasterosteus_aculeatus.BROADS1.47.pep.all.fa.gz',
'hg18' => 'homo_sapiens_47_36i/pep/Homo_sapiens.NCBI36.47.pep.all.fa.gz',
'loxAfr1' => 'loxodonta_africana_47_1c/pep/Loxodonta_africana.BROADE1.47.pep.all.fa.gz',
'rheMac2' => 'macaca_mulatta_47_10f/pep/Macaca_mulatta.MMUL_1.47.pep.all.fa.gz',
'monDom5' => 'monodelphis_domestica_47_5b/pep/Monodelphis_domestica.BROADO5.47.pep.all.fa.gz',
'mm9' => 'mus_musculus_47_37/pep/Mus_musculus.NCBIM37.47.pep.all.fa.gz',
'myoLuc0' => 'myotis_lucifugus_47_1c/pep/Myotis_lucifugus.MICROBAT1.47.pep.all.fa.gz',
'ornAna1' => 'ornithorhynchus_anatinus_47_1d/pep/Ornithorhynchus_anatinus.OANA5.47.pep.all.fa.gz',
'oryCun1' => 'oryctolagus_cuniculus_47_1d/pep/Oryctolagus_cuniculus.RABBIT.47.pep.all.fa.gz',
'oryLat1' => 'oryzias_latipes_47_1c/pep/Oryzias_latipes.MEDAKA1.47.pep.all.fa.gz',
'otoGar1' => 'otolemur_garnettii_47_1a/pep/Otolemur_garnettii.BUSHBABY1.47.pep.all.fa',
'panTro2' => 'pan_troglodytes_47_21f/pep/Pan_troglodytes.CHIMP2.1.47.pep.all.fa.gz ',
'rn4' => 'rattus_norvegicus_47_34q/pep/Rattus_norvegicus.RGSC3.4.47.pep.all.fa.gz',
'sacCer1' => 'saccharomyces_cerevisiae_47_1g/pep/Saccharomyces_cerevisiae.SGD1.01.47.pep.all.fa.gz',
'sorAra0' => 'sorex_araneus_47_1a/pep/Sorex_araneus.COMMON_SHREW1.47.pep.all.fa.gz',
'speTri0' => 'spermophilus_tridecemlineatus_47_1c/pep/Spermophilus_tridecemlineatus.SQUIRREL.47.pep.all.fa.gz',
'fr2' => 'takifugu_rubripes_47_4g/pep/Takifugu_rubripes.FUGU4.47.pep.all.fa.gz',
'tetNig1' => 'tetraodon_nigroviridis_47_1i/pep/Tetraodon_nigroviridis.TETRAODON7.47.pep.all.fa.gz',
'tupBel1' => 'tupaia_belangeri_47_1b/pep/Tupaia_belangeri.TREESHREW.47.pep.all.fa.gz',
'xenTro2' => 'xenopus_tropicalis_47_41g/pep/Xenopus_tropicalis.JGI4.1.47.pep.all.fa.gz',
);

#	directory name under release-47/mysql/ to find 'seq_region' and
#	'assembly' table copies for GeneScaffold coordinate conversions
my %ensGeneFtpMySqlFileNames_47 = (
'aedAeg0' => 'aedes_aegypti_core_47_1a',
'anoGam2' => 'anopheles_gambiae_core_47_3i',
'bosTau3' => 'bos_taurus_core_47_3d',
'ce5' => 'caenorhabditis_elegans_core_47_180',
'canFam2' => 'canis_familiaris_core_47_2e',
'cavPor2' => 'cavia_porcellus_core_47_1b',
'ci2' => 'ciona_intestinalis_core_47_2g',
'cioSav2' => 'ciona_savignyi_core_47_2e',
'danRer5' => 'danio_rerio_core_47_7a',
'dasNov1' => 'dasypus_novemcinctus_core_47_1d',
'dm4' => 'drosophila_melanogaster_core_47_43b',
'echTel1' => 'echinops_telfairi_core_47_1d',
'eriEur1' => 'erinaceus_europaeus_core_47_1b',
'felCat3' => 'felis_catus_core_47_1b',
'galGal3' => 'gallus_gallus_core_47_2e',
'gasAcu1' => 'gasterosteus_aculeatus_core_47_1d',
'hg18' => 'homo_sapiens_core_47_36i',
'loxAfr1' => 'loxodonta_africana_core_47_1c',
'rheMac2' => 'macaca_mulatta_core_47_10f',
'monDom5' => 'monodelphis_domestica_core_47_5b',
'mm9' => 'mus_musculus_core_47_37',
'myoLuc0' => 'myotis_lucifugus_core_47_1c',
'ornAna1' => 'ornithorhynchus_anatinus_core_47_1d',
'oryCun1' => 'oryctolagus_cuniculus_core_47_1d',
'oryLat1' => 'oryzias_latipes_core_47_1c',
'otoGar1' => 'otolemur_garnettii_core_47_1a',
'panTro2' => 'pan_troglodytes_core_47_21f',
'rn4' => 'rattus_norvegicus_core_47_34q',
'sacCer1' => 'saccharomyces_cerevisiae_core_47_1g',
'sorAra0' => 'sorex_araneus_core_47_1a',
'speTri0' => 'spermophilus_tridecemlineatus_core_47_1c',
'fr2' => 'takifugu_rubripes_core_47_4g',
'tetNig1' => 'tetraodon_nigroviridis_core_47_1i',
'tupBel1' => 'tupaia_belangeri_core_47_1b',
'xenTro2' => 'xenopus_tropicalis_core_47_41g',
);

# key is UCSC db name, result is FTP file name under the gtf directory
my %ensGeneFtpFileNames_48 = (
'aedAeg0' => 'aedes_aegypti/Aedes_aegypti.AaegL1.48.gtf.gz',
'anoGam2' => 'anopheles_gambiae/Anopheles_gambiae.AgamP3.48.gtf.gz',
'bosTau3' => 'bos_taurus/Bos_taurus.Btau_3.1.48.gtf.gz',
'ce5' => 'caenorhabditis_elegans/Caenorhabditis_elegans.WS180.48.gtf.gz',
'canFam2' => 'canis_familiaris/Canis_familiaris.BROADD2.48.gtf.gz',
'cavPor2' => 'cavia_porcellus/Cavia_porcellus.GUINEAPIG.48.gtf.gz',
'ci2' => 'ciona_intestinalis/Ciona_intestinalis.JGI2.48.gtf.gz',
'cioSav2' => 'ciona_savignyi/Ciona_savignyi.CSAV2.0.48.gtf.gz',
'danRer5' => 'danio_rerio/Danio_rerio.ZFISH7.48.gtf.gz',
'dasNov1' => 'dasypus_novemcinctus/Dasypus_novemcinctus.ARMA.48.gtf.gz',
'dm4' => 'drosophila_melanogaster/Drosophila_melanogaster.BDGP4.3.48.gtf.gz',
'echTel1' => 'echinops_telfairi/Echinops_telfairi.TENREC.48.gtf.gz',
'eriEur1' => 'erinaceus_europaeus/Erinaceus_europaeus.HEDGEHOG.48.gtf.gz',
'felCat3' => 'felis_catus/Felis_catus.CAT.48.gtf.gz',
'galGal3' => 'gallus_gallus/Gallus_gallus.WASHUC2.48.gtf.gz',
'gasAcu1' => 'gasterosteus_aculeatus/Gasterosteus_aculeatus.BROADS1.48.gtf.gz',
'hg18' => 'homo_sapiens/Homo_sapiens.NCBI36.48.gtf.gz',
'loxAfr1' => 'loxodonta_africana/Loxodonta_africana.BROADE1.48.gtf.gz',
'rheMac2' => 'macaca_mulatta/Macaca_mulatta.MMUL_1.48.gtf.gz',
'micMur0' => 'microcebus_murinus/Microcebus_murinus.micMur1.48.gtf.gz',
'monDom5' => 'monodelphis_domestica/Monodelphis_domestica.BROADO5.48.gtf.gz',
'mm9' => 'mus_musculus/Mus_musculus.NCBIM37.48.gtf.gz',
'myoLuc0' => 'myotis_lucifugus/Myotis_lucifugus.MICROBAT1.48.gtf.gz',
'ochPri0' => 'ochotona_princeps/Ochotona_princeps.pika.48.gtf.gz',
'ornAna1' => 'ornithorhynchus_anatinus/Ornithorhynchus_anatinus.OANA5.48.gtf.gz',
'oryCun1' => 'oryctolagus_cuniculus/Oryctolagus_cuniculus.RABBIT.48.gtf.gz',
'oryLat1' => 'oryzias_latipes/Oryzias_latipes.MEDAKA1.48.gtf.gz',
'otoGar1' => 'otolemur_garnettii/Otolemur_garnettii.BUSHBABY1.48.gtf.gz',
'panTro2' => 'pan_troglodytes/Pan_troglodytes.CHIMP2.1.48.gtf.gz',
'rn4' => 'rattus_norvegicus/Rattus_norvegicus.RGSC3.4.48.gtf.gz',
'sacCer1' => 'saccharomyces_cerevisiae/Saccharomyces_cerevisiae.SGD1.48.gtf.gz',
'sorAra0' => 'sorex_araneus/Sorex_araneus.COMMON_SHREW1.48.gtf.gz',
'speTri0' => 'spermophilus_tridecemlineatus/Spermophilus_tridecemlineatus.SQUIRREL.48.gtf.gz',
'fr2' => 'takifugu_rubripes/Takifugu_rubripes.FUGU4.48.gtf.gz',
'tetNig1' => 'tetraodon_nigroviridis/Tetraodon_nigroviridis.TETRAODON7.48.gtf.gz',
'tupBel1' => 'tupaia_belangeri/Tupaia_belangeri.TREESHREW.48.gtf.gz',
'xenTro2' => 'xenopus_tropicalis/Xenopus_tropicalis.JGI4.1.48.gtf.gz',
);

# key is UCSC db name, result is FTP file name under the fasta directory
my %ensGeneFtpPeptideFileNames_48 = (
'aedAeg0' => 'aedes_aegypti/pep/Aedes_aegypti.AaegL1.48.pep.all.fa.gz',
'anoGam2' => 'anopheles_gambiae/pep/Anopheles_gambiae.AgamP3.48.pep.all.fa.gz',
'bosTau3' => 'bos_taurus/pep/Bos_taurus.Btau_3.1.48.pep.all.fa.gz',
'ce5' => 'caenorhabditis_elegans/pep/Caenorhabditis_elegans.WS180.48.pep.all.fa.gz',
'canFam2' => 'canis_familiaris/pep/Canis_familiaris.BROADD2.48.pep.all.fa.gz',
'cavPor2' => 'cavia_porcellus/pep/Cavia_porcellus.GUINEAPIG.48.pep.all.fa.gz',
'ci2' => 'ciona_intestinalis/pep/Ciona_intestinalis.JGI2.48.pep.all.fa.gz',
'cioSav2' => 'ciona_savignyi/pep/Ciona_savignyi.CSAV2.0.48.pep.all.fa.gz',
'danRer5' => 'danio_rerio/pep/Danio_rerio.ZFISH7.48.pep.all.fa.gz',
'dasNov1' => 'dasypus_novemcinctus/pep/Dasypus_novemcinctus.ARMA.48.pep.all.fa.gz',
'dm4' => 'drosophila_melanogaster/pep/Drosophila_melanogaster.BDGP4.3.48.pep.all.fa.gz',
'echTel1' => 'echinops_telfairi/pep/Echinops_telfairi.TENREC.48.pep.all.fa.gz',
'eriEur1' => 'erinaceus_europaeus/pep/Erinaceus_europaeus.HEDGEHOG.48.pep.all.fa.gz',
'felCat3' => 'felis_catus/pep/Felis_catus.CAT.48.pep.all.fa.gz',
'galGal3' => 'gallus_gallus/pep/Gallus_gallus.WASHUC2.48.pep.all.fa.gz',
'gasAcu1' => 'gasterosteus_aculeatus/pep/Gasterosteus_aculeatus.BROADS1.48.pep.all.fa.gz',
'hg18' => 'homo_sapiens/pep/Homo_sapiens.NCBI36.48.pep.all.fa.gz',
'loxAfr1' => 'loxodonta_africana/pep/Loxodonta_africana.BROADE1.48.pep.all.fa.gz',
'rheMac2' => 'macaca_mulatta/pep/Macaca_mulatta.MMUL_1.48.pep.all.fa.gz',
'micMur0' => 'microcebus_murinus/pep/Microcebus_murinus.micMur1.48.pep.all.fa.gz',
'monDom5' => 'monodelphis_domestica/pep/Monodelphis_domestica.BROADO5.48.pep.all.fa.gz',
'mm9' => 'mus_musculus/pep/Mus_musculus.NCBIM37.48.pep.all.fa.gz',
'myoLuc0' => 'myotis_lucifugus/pep/Myotis_lucifugus.MICROBAT1.48.pep.all.fa.gz',
'ochPri0' => 'ochotona_princeps/pep/Ochotona_princeps.pika.48.pep.all.fa.gz',
'ornAna1' => 'ornithorhynchus_anatinus/pep/Ornithorhynchus_anatinus.OANA5.48.pep.all.fa.gz',
'oryCun1' => 'oryctolagus_cuniculus/pep/Oryctolagus_cuniculus.RABBIT.48.pep.all.fa.gz',
'oryLat1' => 'oryzias_latipes/pep/Oryzias_latipes.MEDAKA1.48.pep.all.fa.gz',
'otoGar1' => 'otolemur_garnettii/pep/Otolemur_garnettii.BUSHBABY1.48.pep.all.fa.gz',
'panTro2' => 'pan_troglodytes/pep/Pan_troglodytes.CHIMP2.1.48.pep.all.fa.gz',
'rn4' => 'rattus_norvegicus/pep/Rattus_norvegicus.RGSC3.4.48.pep.all.fa.gz',
'sacCer1' => 'saccharomyces_cerevisiae/pep/Saccharomyces_cerevisiae.SGD1.48.pep.all.fa.gz',
'sorAra0' => 'sorex_araneus/pep/Sorex_araneus.COMMON_SHREW1.48.pep.all.fa.gz',
'speTri0' => 'spermophilus_tridecemlineatus/pep/Spermophilus_tridecemlineatus.SQUIRREL.48.pep.all.fa.gz',
'fr2' => 'takifugu_rubripes/pep/Takifugu_rubripes.FUGU4.48.pep.all.fa.gz',
'tetNig1' => 'tetraodon_nigroviridis/pep/Tetraodon_nigroviridis.TETRAODON7.48.pep.all.fa.gz',
'tupBel1' => 'tupaia_belangeri/pep/Tupaia_belangeri.TREESHREW.48.pep.all.fa.gz',
'xenTro2' => 'xenopus_tropicalis/pep/Xenopus_tropicalis.JGI4.1.48.pep.all.fa.gz',
);

#	directory name under release-48/mysql/ to find 'seq_region' and
#	'assembly' table copies for GeneScaffold coordinate conversions
my %ensGeneFtpMySqlFileNames_48 = (
'aedAeg0' => 'aedes_aegypti_core_48_1b',
'anoGam2' => 'anopheles_gambiae_core_48_3j',
'bosTau3' => 'bos_taurus_core_48_3e',
'ce5' => 'caenorhabditis_elegans_core_48_180a',
'canFam2' => 'canis_familiaris_core_48_2f',
'cavPor2' => 'cavia_porcellus_core_48_1c',
'ci2' => 'ciona_intestinalis_core_48_2h',
'cioSav2' => 'ciona_savignyi_core_48_2f',
'danRer5' => 'danio_rerio_core_48_7b',
'dasNov1' => 'dasypus_novemcinctus_core_48_1e',
'dm4' => 'drosophila_melanogaster_core_48_43b',
'echTel1' => 'echinops_telfairi_core_48_1e',
'eriEur1' => 'erinaceus_europaeus_core_48_1c',
'felCat3' => 'felis_catus_core_48_1c',
'galGal3' => 'gallus_gallus_core_48_2f',
'gasAcu1' => 'gasterosteus_aculeatus_core_48_1e',
'hg18' => 'homo_sapiens_core_48_36j',
'loxAfr1' => 'loxodonta_africana_core_48_1d',
'rheMac2' => 'macaca_mulatta_core_48_10g',
'micMur0' => 'microcebus_murinus_core_48_1',
'monDom5' => 'monodelphis_domestica_core_48_5c',
'mm9' => 'mus_musculus_core_48_37a',
'myoLuc0' => 'myotis_lucifugus_core_48_1d',
'ochPri0' => 'ochotona_princeps_core_48_1',
'ornAna1' => 'ornithorhynchus_anatinus_core_48_1e',
'oryCun1' => 'oryctolagus_cuniculus_core_48_1e',
'oryLat1' => 'oryzias_latipes_core_48_1d',
'otoGar1' => 'otolemur_garnettii_core_48_1b',
'panTro2' => 'pan_troglodytes_core_48_21g',
'rn4' => 'rattus_norvegicus_core_48_34r',
'sacCer1' => 'saccharomyces_cerevisiae_core_48_1h',
'sorAra0' => 'sorex_araneus_core_48_1b',
'speTri0' => 'spermophilus_tridecemlineatus_core_48_1d',
'fr2' => 'takifugu_rubripes_core_48_4h',
'tetNig1' => 'tetraodon_nigroviridis_core_48_1j',
'tupBel1' => 'tupaia_belangeri_core_48_1c',
'xenTro2' => 'xenopus_tropicalis_core_48_41h',
);

# key is UCSC db name, result is FTP file name under the gtf directory
my %ensGeneFtpFileNames_49 = (
'aedAeg0' => 'aedes_aegypti/Aedes_aegypti.AaegL1.49.gtf.gz',
'anoGam2' => 'anopheles_gambiae/Anopheles_gambiae.AgamP3.49.gtf.gz',
'bosTau3' => 'bos_taurus/Bos_taurus.Btau_3.1.49.gtf.gz',
'ce5' => 'caenorhabditis_elegans/Caenorhabditis_elegans.WS180.49.gtf.gz',
'canFam2' => 'canis_familiaris/Canis_familiaris.BROADD2.49.gtf.gz',
'cavPor2' => 'cavia_porcellus/Cavia_porcellus.GUINEAPIG.49.gtf.gz',
'ci2' => 'ciona_intestinalis/Ciona_intestinalis.JGI2.49.gtf.gz',
'cioSav2' => 'ciona_savignyi/Ciona_savignyi.CSAV2.0.49.gtf.gz',
'danRer5' => 'danio_rerio/Danio_rerio.ZFISH7.49.gtf.gz',
'dasNov1' => 'dasypus_novemcinctus/Dasypus_novemcinctus.ARMA.49.gtf.gz',
'dm5' => 'drosophila_melanogaster/Drosophila_melanogaster.BDGP5.4.49.gtf.gz',
'echTel1' => 'echinops_telfairi/Echinops_telfairi.TENREC.49.gtf.gz',
'equCab2' => 'equus_caballus/Equus_caballus.EquCab2.49.gtf.gz',
'eriEur1' => 'erinaceus_europaeus/Erinaceus_europaeus.HEDGEHOG.49.gtf.gz',
'felCat3' => 'felis_catus/Felis_catus.CAT.49.gtf.gz',
'galGal3' => 'gallus_gallus/Gallus_gallus.WASHUC2.49.gtf.gz',
'gasAcu1' => 'gasterosteus_aculeatus/Gasterosteus_aculeatus.BROADS1.49.gtf.gz',
'hg18' => 'homo_sapiens/Homo_sapiens.NCBI36.49.gtf.gz',
'loxAfr1' => 'loxodonta_africana/Loxodonta_africana.BROADE1.49.gtf.gz',
'rheMac2' => 'macaca_mulatta/Macaca_mulatta.MMUL_1.49.gtf.gz',
'micMur0' => 'microcebus_murinus/Microcebus_murinus.micMur1.49.gtf.gz',
'monDom5' => 'monodelphis_domestica/Monodelphis_domestica.BROADO5.49.gtf.gz',
'mm9' => 'mus_musculus/Mus_musculus.NCBIM37.49.gtf.gz',
'myoLuc1' => 'myotis_lucifugus/Myotis_lucifugus.MICROBAT1.49.gtf.gz',
'ochPri0' => 'ochotona_princeps/Ochotona_princeps.pika.49.gtf.gz',
'ornAna1' => 'ornithorhynchus_anatinus/Ornithorhynchus_anatinus.OANA5.49.gtf.gz',
'oryCun1' => 'oryctolagus_cuniculus/Oryctolagus_cuniculus.RABBIT.49.gtf.gz',
'oryLat1' => 'oryzias_latipes/Oryzias_latipes.MEDAKA1.49.gtf.gz',
'otoGar1' => 'otolemur_garnettii/Otolemur_garnettii.BUSHBABY1.49.gtf.gz',
'panTro2' => 'pan_troglodytes/Pan_troglodytes.CHIMP2.1.49.gtf.gz',
'ponAbe2' => 'pongo_pygmaeus/Pongo_pygmaeus.PPYG2.49.gtf.gz',
'rn4' => 'rattus_norvegicus/Rattus_norvegicus.RGSC3.4.49.gtf.gz',
'sacCer1' => 'saccharomyces_cerevisiae/Saccharomyces_cerevisiae.SGD1.01.49.gtf.gz',
'sorAra0' => 'sorex_araneus/Sorex_araneus.COMMON_SHREW1.49.gtf',
'speTri0' => 'spermophilus_tridecemlineatus/Spermophilus_tridecemlineatus.SQUIRREL.49.gtf.gz',
'fr2' => 'takifugu_rubripes/Takifugu_rubripes.FUGU4.49.gtf.gz',
'tetNig1' => 'tetraodon_nigroviridis/Tetraodon_nigroviridis.TETRAODON7.49.gtf.gz',
'tupBel1' => 'tupaia_belangeri/Tupaia_belangeri.TREESHREW.49.gtf.gz',
'xenTro2' => 'xenopus_tropicalis/Xenopus_tropicalis.JGI4.1.49.gtf.gz',
);

# key is UCSC db name, result is FTP file name under the fasta directory
my %ensGeneFtpPeptideFileNames_49 = (
'aedAeg0' => 'aedes_aegypti/pep/Aedes_aegypti.AaegL1.49.pep.all.fa.gz',
'anoGam2' => 'anopheles_gambiae/pep/Anopheles_gambiae.AgamP3.49.pep.all.fa.gz',
'bosTau3' => 'bos_taurus/pep/Bos_taurus.Btau_3.1.49.pep.all.fa.gz',
'ce5' => 'caenorhabditis_elegans/pep/Caenorhabditis_elegans.WS180.49.pep.all.fa.gz',
'canFam2' => 'canis_familiaris/pep/Canis_familiaris.BROADD2.49.pep.all.fa.gz',
'cavPor2' => 'cavia_porcellus/pep/Cavia_porcellus.GUINEAPIG.49.pep.all.fa.gz',
'ci2' => 'ciona_intestinalis/pep/Ciona_intestinalis.JGI2.49.pep.all.fa.gz',
'cioSav2' => 'ciona_savignyi/pep/Ciona_savignyi.CSAV2.0.49.pep.all.fa.gz',
'danRer5' => 'danio_rerio/pep/Danio_rerio.ZFISH7.49.pep.all.fa.gz',
'dasNov1' => 'dasypus_novemcinctus/pep/Dasypus_novemcinctus.ARMA.49.pep.all.fa.gz',
'dm5' => 'drosophila_melanogaster/pep/Drosophila_melanogaster.BDGP5.4.49.pep.all.fa.gz',
'echTel1' => 'echinops_telfairi/pep/Echinops_telfairi.TENREC.49.pep.all.fa.gz',
'equCab2' => 'equus_caballus/pep/Equus_caballus.EquCab2.49.pep.all.fa.gz',
'eriEur1' => 'erinaceus_europaeus/pep/Erinaceus_europaeus.HEDGEHOG.49.pep.all.fa.gz',
'felCat3' => 'felis_catus/pep/Felis_catus.CAT.49.pep.all.fa.gz',
'galGal3' => 'gallus_gallus/pep/Gallus_gallus.WASHUC2.49.pep.all.fa.gz',
'gasAcu1' => 'gasterosteus_aculeatus/pep/Gasterosteus_aculeatus.BROADS1.49.pep.all.fa.gz',
'hg18' => 'homo_sapiens/pep/Homo_sapiens.NCBI36.49.pep.all.fa.gz',
'loxAfr1' => 'loxodonta_africana/pep/Loxodonta_africana.BROADE1.49.pep.all.fa.gz',
'rheMac2' => 'macaca_mulatta/pep/Macaca_mulatta.MMUL_1.49.pep.all.fa.gz',
'micMur0' => 'microcebus_murinus/pep/Microcebus_murinus.micMur1.49.pep.all.fa.gz',
'monDom5' => 'monodelphis_domestica/pep/Monodelphis_domestica.BROADO5.49.pep.all.fa.gz',
'mm9' => 'mus_musculus/pep/Mus_musculus.NCBIM37.49.pep.all.fa.gz',
'myoLuc0' => 'myotis_lucifugus/pep/Myotis_lucifugus.MICROBAT1.49.pep.all.fa.gz',
'ochPri0' => 'ochotona_princeps/pep/Ochotona_princeps.pika.49.pep.all.fa.gz',
'ornAna1' => 'ornithorhynchus_anatinus/pep/Ornithorhynchus_anatinus.OANA5.49.pep.all.fa.gz',
'oryCun1' => 'oryctolagus_cuniculus/pep/Oryctolagus_cuniculus.RABBIT.49.pep.all.fa.gz',
'oryLat1' => 'oryzias_latipes/pep/Oryzias_latipes.MEDAKA1.49.pep.all.fa.gz',
'otoGar1' => 'otolemur_garnettii/pep/Otolemur_garnettii.BUSHBABY1.49.pep.all.fa.gz',
'panTro2' => 'pan_troglodytes/pep/Pan_troglodytes.CHIMP2.1.49.pep.all.fa.gz',
'ponAbe2' => 'pongo_pygmaeus/pep/Pongo_pygmaeus.PPYG2.49.pep.all.fa.gz',
'rn4' => 'rattus_norvegicus/pep/Rattus_norvegicus.RGSC3.4.49.pep.all.fa.gz',
'sacCer1' => 'saccharomyces_cerevisiae/pep/Saccharomyces_cerevisiae.SGD1.01.49.pep.all.fa.gz',
'sorAra0' => 'sorex_araneus/pep/Sorex_araneus.COMMON_SHREW1.49.pep.all.fa.gz',
'speTri0' => 'spermophilus_tridecemlineatus/pep/Spermophilus_tridecemlineatus.SQUIRREL.49.pep.all.fa.gz',
'fr2' => 'takifugu_rubripes/pep/Takifugu_rubripes.FUGU4.49.pep.all.fa.gz',
'tetNig1' => 'tetraodon_nigroviridis/pep/Tetraodon_nigroviridis.TETRAODON7.49.pep.all.fa.gz',
'tupBel1' => 'tupaia_belangeri/pep/Tupaia_belangeri.TREESHREW.49.pep.all.fa.gz',
'xenTro2' => 'xenopus_tropicalis/pep/Xenopus_tropicalis.JGI4.1.49.pep.all.fa.gz',
);

#	directory name under release-49/mysql/ to find 'seq_region' and
#	'assembly' table copies for GeneScaffold coordinate conversions
my %ensGeneFtpMySqlFileNames_49 = (
'aedAeg0' => 'aedes_aegypti_core_49_1b',
'anoGam2' => 'anopheles_gambiae_core_49_3j',
'bosTau3' => 'bos_taurus_core_49_3f',
'ce5' => 'caenorhabditis_elegans_core_49_180a',
'canFam2' => 'canis_familiaris_core_49_2g',
'cavPor2' => 'cavia_porcellus_core_49_1c',
'ci2' => 'ciona_intestinalis_core_49_2i',
'cioSav2' => 'ciona_savignyi_core_49_2f',
'danRer5' => 'danio_rerio_core_49_7c',
'dasNov1' => 'dasypus_novemcinctus_core_49_1f',
'dm5' => 'drosophila_melanogaster_core_49_54',
'echTel1' => 'echinops_telfairi_core_49_1e',
'equCab2' => 'equus_caballus_core_49_2',
'eriEur1' => 'erinaceus_europaeus_core_49_1c',
'felCat3' => 'felis_catus_core_49_1c',
'galGal3' => 'gallus_gallus_core_49_2g',
'gasAcu1' => 'gasterosteus_aculeatus_core_49_1f',
'hg18' => 'homo_sapiens_core_49_36k',
'loxAfr1' => 'loxodonta_africana_core_49_1d',
'rheMac2' => 'macaca_mulatta_core_49_10h',
'micMur0' => 'microcebus_murinus_core_49_1',
'monDom5' => 'monodelphis_domestica_core_49_5d',
'mm9' => 'mus_musculus_core_49_37b',
'myoLuc0' => 'myotis_lucifugus_core_49_1e',
'ochPri0' => 'ochotona_princeps_core_49_1',
'ornAna1' => 'ornithorhynchus_anatinus_core_49_1f',
'oryCun1' => 'oryctolagus_cuniculus_core_49_1f',
'oryLat1' => 'oryzias_latipes_core_49_1e',
'otoGar1' => 'otolemur_garnettii_core_49_1c',
'panTro2' => 'pan_troglodytes_core_49_21h',
'ponAbe2' => 'pongo_pygmaeus_core_49_1',
'rn4' => 'rattus_norvegicus_core_49_34s',
'sacCer1' => 'saccharomyces_cerevisiae_core_49_1h',
'sorAra0' => 'sorex_araneus_core_49_1b',
'speTri0' => 'spermophilus_tridecemlineatus_core_49_1e',
'fr2' => 'takifugu_rubripes_core_49_4i',
'tetNig1' => 'tetraodon_nigroviridis_core_49_1k',
'tupBel1' => 'tupaia_belangeri_core_49_1d',
'xenTro2' => 'xenopus_tropicalis_core_49_41i',
);

# key is UCSC db name, result is FTP file name under the gtf directory
my %ensGeneFtpFileNames_50 = (
'aedAeg0' => 'aedes_aegypti/Aedes_aegypti.AaegL1.50.gtf.gz',
'anoGam2' => 'anopheles_gambiae/Anopheles_gambiae.AgamP3.50.gtf.gz',
'bosTau4' => 'bos_taurus/Bos_taurus.Btau_4.0.50.gtf.gz',
'ce6' => 'caenorhabditis_elegans/Caenorhabditis_elegans.WS190.50.gtf.gz',
'canFam2' => 'canis_familiaris/Canis_familiaris.BROADD2.50.gtf.gz',
'cavPor2' => 'cavia_porcellus/Cavia_porcellus.GUINEAPIG.50.gtf.gz',
'ci2' => 'ciona_intestinalis/Ciona_intestinalis.JGI2.50.gtf.gz',
'cioSav2' => 'ciona_savignyi/Ciona_savignyi.CSAV2.0.50.gtf.gz',
'danRer5' => 'danio_rerio/Danio_rerio.ZFISH7.50.gtf.gz',
'dasNov1' => 'dasypus_novemcinctus/Dasypus_novemcinctus.ARMA.50.gtf.gz',
'dm5' => 'drosophila_melanogaster/Drosophila_melanogaster.BDGP5.4.50.gtf.gz',
'echTel1' => 'echinops_telfairi/Echinops_telfairi.TENREC.50.gtf.gz',
'equCab2' => 'equus_caballus/Equus_caballus.EquCab2.50.gtf.gz',
'eriEur1' => 'erinaceus_europaeus/Erinaceus_europaeus.HEDGEHOG.50.gtf.gz',
'felCat3' => 'felis_catus/Felis_catus.CAT.50.gtf.gz',
'galGal3' => 'gallus_gallus/Gallus_gallus.WASHUC2.50.gtf.gz',
'gasAcu1' => 'gasterosteus_aculeatus/Gasterosteus_aculeatus.BROADS1.50.gtf.gz',
'hg18' => 'homo_sapiens/Homo_sapiens.NCBI36.50.gtf.gz',
'loxAfr1' => 'loxodonta_africana/Loxodonta_africana.BROADE1.50.gtf.gz',
'rheMac2' => 'macaca_mulatta/Macaca_mulatta.MMUL_1.50.gtf.gz',
'micMur1' => 'microcebus_murinus/Microcebus_murinus.micMur1.50.gtf.gz',
'monDom5' => 'monodelphis_domestica/Monodelphis_domestica.BROADO5.50.gtf.gz',
'mm9' => 'mus_musculus/Mus_musculus.NCBIM37.50.gtf.gz',
'myoLuc1' => 'myotis_lucifugus/Myotis_lucifugus.MICROBAT1.50.gtf.gz',
'ochPri1' => 'ochotona_princeps/Ochotona_princeps.pika.50.gtf.gz',
'ornAna1' => 'ornithorhynchus_anatinus/Ornithorhynchus_anatinus.OANA5.50.gtf.gz',
'oryCun1' => 'oryctolagus_cuniculus/Oryctolagus_cuniculus.RABBIT.50.gtf.gz',
'oryLat1' => 'oryzias_latipes/Oryzias_latipes.MEDAKA1.50.gtf.gz',
'otoGar1' => 'otolemur_garnettii/Otolemur_garnettii.BUSHBABY1.50.gtf.gz',
'panTro2' => 'pan_troglodytes/Pan_troglodytes.CHIMP2.1.50.gtf.gz',
'ponAbe2' => 'pongo_pygmaeus/Pongo_pygmaeus.PPYG2.50.gtf.gz',
'rn4' => 'rattus_norvegicus/Rattus_norvegicus.RGSC3.4.50.gtf.gz',
'sacCer1' => 'saccharomyces_cerevisiae/Saccharomyces_cerevisiae.SGD1.01.50.gtf.gz',
'sorAra0' => 'sorex_araneus/Sorex_araneus.COMMON_SHREW1.50.gtf.gz',
'speTri1' => 'spermophilus_tridecemlineatus/Spermophilus_tridecemlineatus.SQUIRREL.50.gtf.gz',
'fr2' => 'takifugu_rubripes/Takifugu_rubripes.FUGU4.50.gtf.gz',
'tetNig2' => 'tetraodon_nigroviridis/Tetraodon_nigroviridis.TETRAODON8.50.gtf.gz',
'tupBel1' => 'tupaia_belangeri/Tupaia_belangeri.TREESHREW.50.gtf.gz',
'xenTro2' => 'xenopus_tropicalis/Xenopus_tropicalis.JGI4.1.50.gtf.gz',
);

# key is UCSC db name, result is FTP file name under the fasta directory
my %ensGeneFtpPeptideFileNames_50 = (
'aedAeg0' => 'aedes_aegypti/pep/Aedes_aegypti.AaegL1.50.pep.all.fa.gz',
'anoGam2' => 'anopheles_gambiae/pep/Anopheles_gambiae.AgamP3.50.pep.all.fa.gz',
'bosTau4' => 'bos_taurus/pep/Bos_taurus.Btau_4.0.50.pep.all.fa.gz',
'ce6' => 'caenorhabditis_elegans/pep/Caenorhabditis_elegans.WS190.50.pep.all.fa.gz',
'canFam2' => 'canis_familiaris/pep/Canis_familiaris.BROADD2.50.pep.all.fa.gz',
'cavPor2' => 'cavia_porcellus/pep/Cavia_porcellus.GUINEAPIG.50.pep.all.fa.gz',
'ci2' => 'ciona_intestinalis/pep/Ciona_intestinalis.JGI2.50.pep.all.fa.gz',
'cioSav2' => 'ciona_savignyi/pep/Ciona_savignyi.CSAV2.0.50.pep.all.fa.gz',
'danRer5' => 'danio_rerio/pep/Danio_rerio.ZFISH7.50.pep.all.fa.gz',
'dasNov1' => 'dasypus_novemcinctus/pep/Dasypus_novemcinctus.ARMA.50.pep.all.fa.gz',
'dm5' => 'drosophila_melanogaster/pep/Drosophila_melanogaster.BDGP5.4.50.pep.all.fa.gz',
'echTel1' => 'echinops_telfairi/pep/Echinops_telfairi.TENREC.50.pep.all.fa.gz',
'equCab2' => 'equus_caballus/pep/Equus_caballus.EquCab2.50.pep.all.fa.gz',
'eriEur1' => 'erinaceus_europaeus/pep/Erinaceus_europaeus.HEDGEHOG.50.pep.all.fa.gz',
'felCat3' => 'felis_catus/pep/Felis_catus.CAT.50.pep.all.fa.gz',
'galGal3' => 'gallus_gallus/pep/Gallus_gallus.WASHUC2.50.pep.all.fa.gz',
'gasAcu1' => 'gasterosteus_aculeatus/pep/Gasterosteus_aculeatus.BROADS1.50.pep.all.fa.gz',
'hg18' => 'homo_sapiens/pep/Homo_sapiens.NCBI36.50.pep.all.fa.gz',
'loxAfr1' => 'loxodonta_africana/pep/Loxodonta_africana.BROADE1.50.pep.all.fa.gz',
'rheMac2' => 'macaca_mulatta/pep/Macaca_mulatta.MMUL_1.50.pep.all.fa.gz',
'micMur1' => 'microcebus_murinus/pep/Microcebus_murinus.micMur1.50.pep.all.fa.gz',
'monDom5' => 'monodelphis_domestica/pep/Monodelphis_domestica.BROADO5.50.pep.all.fa.gz',
'mm9' => 'mus_musculus/pep/Mus_musculus.NCBIM37.50.pep.all.fa.gz',
'myoLuc1' => 'myotis_lucifugus/pep/Myotis_lucifugus.MICROBAT1.50.pep.all.fa.gz',
'ochPri1' => 'ochotona_princeps/pep/Ochotona_princeps.pika.50.pep.all.fa.gz',
'ornAna1' => 'ornithorhynchus_anatinus/pep/Ornithorhynchus_anatinus.OANA5.50.pep.all.fa.gz',
'oryCun1' => 'oryctolagus_cuniculus/pep/Oryctolagus_cuniculus.RABBIT.50.pep.all.fa.gz',
'oryLat1' => 'oryzias_latipes/pep/Oryzias_latipes.MEDAKA1.50.pep.all.fa.gz',
'otoGar1' => 'otolemur_garnettii/pep/Otolemur_garnettii.BUSHBABY1.50.pep.all.fa.gz',
'panTro2' => 'pan_troglodytes/pep/Pan_troglodytes.CHIMP2.1.50.pep.all.fa.gz',
'ponAbe2' => 'pongo_pygmaeus/pep/Pongo_pygmaeus.PPYG2.50.pep.all.fa.gz',
'rn4' => 'rattus_norvegicus/pep/Rattus_norvegicus.RGSC3.4.50.pep.all.fa.gz',
'sacCer1' => 'saccharomyces_cerevisiae/pep/Saccharomyces_cerevisiae.SGD1.01.50.pep.all.fa.gz',
'sorAra0' => 'sorex_araneus/pep/Sorex_araneus.COMMON_SHREW1.50.pep.all.fa.gz',
'speTri1' => 'spermophilus_tridecemlineatus/pep/Spermophilus_tridecemlineatus.SQUIRREL.50.pep.all.fa.gz',
'fr2' => 'takifugu_rubripes/pep/Takifugu_rubripes.FUGU4.50.pep.all.fa.gz',
'tetNig2' => 'tetraodon_nigroviridis/pep/Tetraodon_nigroviridis.TETRAODON8.50.pep.all.fa.gz',
'tupBel1' => 'tupaia_belangeri/pep/Tupaia_belangeri.TREESHREW.50.pep.all.fa.gz',
'xenTro2' => 'xenopus_tropicalis/pep/Xenopus_tropicalis.JGI4.1.50.pep.all.fa.gz',
);

#	directory name under release-50/mysql/ to find 'seq_region' and
#	'assembly' table copies for GeneScaffold coordinate conversions
my %ensGeneFtpMySqlFileNames_50 = (
'aedAeg0' => 'aedes_aegypti_core_50_1c',
'anoGam2' => 'anopheles_gambiae_core_50_3k',
'bosTau4' => 'bos_taurus_core_50_4',
'ce6' => 'caenorhabditis_elegans_core_50_190',
'canFam2' => 'canis_familiaris_core_50_2h',
'cavPor2' => 'cavia_porcellus_core_50_1d',
'ci2' => 'ciona_intestinalis_core_50_2j',
'cioSav2' => 'ciona_savignyi_core_50_2g',
'danRer5' => 'danio_rerio_core_50_7d',
'dasNov1' => 'dasypus_novemcinctus_core_50_1g',
'dm5' => 'drosophila_melanogaster_core_50_54a',
'echTel1' => 'echinops_telfairi_core_50_1f',
'equCab2' => 'equus_caballus_core_50_2a',
'eriEur1' => 'erinaceus_europaeus_core_50_1d',
'felCat3' => 'felis_catus_core_50_1d',
'galGal3' => 'gallus_gallus_core_50_2h',
'gasAcu1' => 'gasterosteus_aculeatus_core_50_1g',
'hg18' => 'homo_sapiens_core_50_36l',
'loxAfr1' => 'loxodonta_africana_core_50_1e',
'rheMac2' => 'macaca_mulatta_core_50_10i',
'micMur1' => 'microcebus_murinus_core_50_1a',
'monDom5' => 'monodelphis_domestica_core_50_5e',
'mm9' => 'mus_musculus_core_50_37c',
'myoLuc1' => 'myotis_lucifugus_core_50_1f',
'ochPri1' => 'ochotona_princeps_core_50_1a',
'ornAna1' => 'ornithorhynchus_anatinus_core_50_1g',
'oryCun1' => 'oryctolagus_cuniculus_core_50_1g',
'oryLat1' => 'oryzias_latipes_core_50_1f',
'otoGar1' => 'otolemur_garnettii_core_50_1d',
'panTro2' => 'pan_troglodytes_core_50_21i',
'ponAbe2' => 'pongo_pygmaeus_core_50_1a',
'rn4' => 'rattus_norvegicus_core_50_34t',
'sacCer1' => 'saccharomyces_cerevisiae_core_50_1i',
'sorAra0' => 'sorex_araneus_core_50_1d',
'speTri1' => 'spermophilus_tridecemlineatus_core_50_1f',
'fr2' => 'takifugu_rubripes_core_50_4j',
'tetNig2' => 'tetraodon_nigroviridis_core_50_8',
'tupBel1' => 'tupaia_belangeri_core_50_1e',
'xenTro2' => 'xenopus_tropicalis_core_50_41j',
);

my @versionList = qw( 50 49 48 47 46 35 );

my @ensGtfReference;
$ensGtfReference[50] = \%ensGeneFtpFileNames_50;
$ensGtfReference[49] = \%ensGeneFtpFileNames_49;
$ensGtfReference[48] = \%ensGeneFtpFileNames_48;
$ensGtfReference[47] = \%ensGeneFtpFileNames_47;
$ensGtfReference[46] = \%ensGeneFtpFileNames_46;
$ensGtfReference[35] = \%ensGeneFtpFileNames_35;
my @ensPepReference;
$ensPepReference[50] = \%ensGeneFtpPeptideFileNames_50;
$ensPepReference[49] = \%ensGeneFtpPeptideFileNames_49;
$ensPepReference[48] = \%ensGeneFtpPeptideFileNames_48;
$ensPepReference[47] = \%ensGeneFtpPeptideFileNames_47;
$ensPepReference[46] = \%ensGeneFtpPeptideFileNames_46;
$ensPepReference[35] = \%ensGeneFtpPeptideFileNames_35;
my @ensMySqlReference;
$ensMySqlReference[50] = \%ensGeneFtpMySqlFileNames_50;
$ensMySqlReference[49] = \%ensGeneFtpMySqlFileNames_49;
$ensMySqlReference[48] = \%ensGeneFtpMySqlFileNames_48;
$ensMySqlReference[47] = \%ensGeneFtpMySqlFileNames_47;
$ensMySqlReference[46] = \%ensGeneFtpMySqlFileNames_46;
$ensMySqlReference[35] = \%ensGeneFtpMySqlFileNames_35;

sub ensVersionList() {
   return @versionList;
}

sub ensGeneVersioning($$) {
#  given a UCSC db name, and an Ensembl version number, return
#	FTP gtf file name, peptide file name, MySql core directory
#	and archive version string
  my ($ucscDb, $ensVersion) = @_;
  if (defined($ensGtfReference[$ensVersion]) &&
	defined($ensPepReference[$ensVersion])) {
    my $gtfReference = $ensGtfReference[$ensVersion];
    my $pepReference = $ensPepReference[$ensVersion];
    my $mySqlReference = $ensMySqlReference[$ensVersion];
    my $gtfDir = "release-$ensVersion/gtf/";
    my $pepDir = "release-$ensVersion/fasta/";
    my $mySqlDir = "release-$ensVersion/mysql/";
    if ($ensVersion < 47) {
	$gtfDir = "release-$ensVersion/";
	$pepDir = "release-$ensVersion/";
	$mySqlDir = "release-$ensVersion/";
    }
    if (exists($gtfReference->{$ucscDb}) &&
	exists($pepReference->{$ucscDb}) &&
	exists($mySqlReference->{$ucscDb}) ) {
	my $gtfName =  $ensemblFTP . $gtfDir . $gtfReference->{$ucscDb};
	my $pepName =  $ensemblFTP . $pepDir . $pepReference->{$ucscDb};
	my $mySqlName =  $ensemblFTP . $mySqlDir . $mySqlReference->{$ucscDb};
	return ($gtfName, $pepName, $mySqlName, $verToDate[$ensVersion]);
    }
  }
  return (undef, undef, undef);
}
