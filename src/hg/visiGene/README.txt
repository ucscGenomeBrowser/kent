NOTES:
Removing all the unused stuff from hgVisiGene for a slimmed down cancer version of visiGene. 
This version will use the vgPrepImage functions, but will have to have its own vgLoadImage
Also, has it's own table schema.

I removed the visiGeneSearch dir, which allows you to test search terms on the loaded images. 
Might want to reinstate that in the future.

Ditto "hgRemoveSubmission". This will be necessary to pull certain sets out of the db, 
as they're all in the same tables (unlike CGB). But right now we want things in, not out,
so this is a later project.
