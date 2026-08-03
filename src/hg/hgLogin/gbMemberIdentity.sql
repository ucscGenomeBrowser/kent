# gbMemberIdentity - links a gbMembers account to one or more external OAuth/OpenID
# identities (Google, ORCID).  A single gbMembers account may have several rows here
# (e.g. one Google and one ORCID).  Created automatically at runtime by hgLogin; this
# file is kept for reference and manual setup.

CREATE TABLE gbMemberIdentity (
    idx int unsigned NOT NULL,			# gbMembers.idx of the linked account
    provider varchar(16) NOT NULL,		# Identity provider: 'google' or 'orcid'
    subject varchar(255) NOT NULL,		# Stable unique subject id from the provider
    email varchar(255) NOT NULL default '',	# Email reported by provider at last login
    created DATETIME NOT NULL,			# Date this identity link was created
    lastUse DATETIME NOT NULL,			# Date this identity was last used to log in
		#Indices
    UNIQUE KEY provSub (provider, subject),
    INDEX(idx)
);
