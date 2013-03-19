class AbTarget(models.Model):
""" Describes where an antibody is thought to bind. """
    term = models.CharField("term", primaryKey=True, max_length=255)
	# A relatively short label, no more than a few words
    deprecated = models.CharField("deprecated", max_length=255, blank=True)
	# If non-empty, the reason why this entry is obsolete.
    description = models.TextField("description")
	# Short description of antibody target
    externalId = models.CharField("external id", max_length=255)
	# Identifier for target, prefixed with source of ID, usually GeneCards
    targetUrl = models.CharField("target url", max_length=255, blank=True)
	# Web page associated with antibody target.

    def __unicode__(self):
        return self.term

class Ab(models.Model):
""" Information on an antibody including where to get it and what it targets."""
    term = models.CharField("term", max_length=255)
	# A relatively short label, no more than a few words
    tag = models.CharField("tag", max_length=255)
	# A short human and machine readable symbol with just alphanumeric characters.
    deprecated = models.CharField("deprecated", max_length=255, blank=True)
	# If non-empty, the reason why this entry is obsolete.
    description = models.TextField("description")
	# Short description of antibody itself.
    target = models.ForeignKey(AbTarget, "target", max_length=255)
	# Molecular target of antibody.
    vendorName = models.CharField("vendor name", max_length=255)
	# Name of vendor selling reagent.
    vendorId = models.CharField("vendor id", max_length=255)
	# Catalog number of other way of identifying reagent.
    orderUrl = models.CharField("order url", max_length=255, blank=True)
	# Web page to order regent.
    lab = models.CharField("lab", max_length=255)
	# Scientific lab producing data.
    validation = models.CharField("validation", max_length=255)
	# How antibody was validated to be specific for target.
    label = models.CharField("label", max_length=255, blank=True)
	# A relatively short label, no more than a few words
    lots = models.CharField("lots", max_length=255, blank=True)
	# The specific lots of reagent used.

    class Meta:
        db_table = 'cvDb_antibody'

    def __unicode__(self):
        return self.term

