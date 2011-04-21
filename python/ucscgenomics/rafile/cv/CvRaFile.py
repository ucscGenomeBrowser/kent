import re
from RaFile import *

class CvRaFile(RaFile):

    def __init__(self, filePath=''):
        RaFile.__init__(self)
        if filePath != '':
            self.read(filePath)


    def readStanza(self, stanza):
        print 'it works!'
        return 'foo', 'bar', 'baz'


class CellStanza(RaEntry):
    pass


class GeneStanza(RaEntry):
    pass


class AntibodyStanza(RaEntry):
    pass


class ReadTypeStanza(RaEntry):
    pass


class InsertLengthStanza(RaEntry):
    pass


class LocalizationStanza(RaEntry):
    pass


class RnaExtractStanza(RaEntry):
    pass


class PromoterStanza(RaEntry):
    pass


class FreezeDateStanza(RaEntry):
    pass


class SpeciesStanza(RaEntry):
    pass


class ControlStanza(RaEntry):
    pass


class TreatmentStanza(RaEntry):
    pass


class ProtocolStanza(RaEntry):
    pass


class PhaseStanza(RaEntry):
    pass


class RegionStanza(RaEntry):
    pass


class RestrictionEnzymeStanza(RaEntry):
    pass


class DataTypeStanza(RaEntry):
    pass


class VersionStanza(RaEntry):
    pass


class StrainStanza(RaEntry):
    pass



