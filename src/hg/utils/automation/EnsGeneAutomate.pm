#
# EnsGeneAutomate: mapping Ensembl species/gff file names to UCSC db
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/EnsGeneAutomate.pm instead.

# $Id: EnsGeneAutomate.pm,v 1.2 2008/02/07 22:48:23 hiram Exp $
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
    qw( ensGeneVersioning
      ),
);

#	Location of Ensembl FTP site for the releases
my $ensemblFTP = "ftp://ftp.ensembl.org/pub/";

#	This listings are created by going to the FTP site and running
#	an ls on the gtf directory.  Edit that listing into this hash.
# key is UCSC db name, result is FTP file name under the gtf directory
my %ensGeneFtpFileNames_47 = (
'aedAeg0' => 'Aedes_aegypti.AaegL1.47.gtf.gz',
'anoGam1' => 'Anopheles_gambiae.AgamP3.47.gtf.gz',
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
'myoLuc0' => 'Myotis_lucifugus.MICROBAT1.47.gtf.gz',
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

# key is UCSC db name, result is FTP file name under the gtf directory
my %ensGeneFtpFileNames_48 = (
'aedAeg0' => 'aedes_aegypti/Aedes_aegypti.AaegL1.48.gtf.gz',
'anoGam1' => 'anopheles_gambiae/Anopheles_gambiae.AgamP3.48.gtf.gz',
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
'anoGam1' => 'anopheles_gambiae/pep/Anopheles_gambiae.AgamP3.48.pep.all.fa.gz',
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

my @ensGtfReference;
$ensGtfReference[47] = \%ensGeneFtpFileNames_47;
$ensGtfReference[48] = \%ensGeneFtpFileNames_48;
my @ensPepReference;
$ensPepReference[48] = \%ensGeneFtpPeptideFileNames_48;

sub ensGeneVersioning($$) {
#  given a UCSC db name, and an Ensembl version number, return
#	FTP gtf file name, peptide file name, Vega file
  my ($ucscDb, $ensVersion) = @_;
  if (defined($ensGtfReference[$ensVersion]) &&
	defined($ensPepReference[$ensVersion])) {
    my $gtfReference = $ensGtfReference[$ensVersion];
    my $pepReference = $ensPepReference[$ensVersion];
    my $gtfDir = "release-$ensVersion/gtf/";
    my $pepDir = "release-$ensVersion/fasta/";
    if (exists($gtfReference->{$ucscDb}) &
	exists($pepReference->{$ucscDb}) ) {
	my $gtfName =  $ensemblFTP . $gtfDir . $gtfReference->{$ucscDb};
	my $pepName =  $ensemblFTP . $pepDir . $pepReference->{$ucscDb};
	return ($gtfName, $pepName);
    }
  }
  return (undef, undef);
}
