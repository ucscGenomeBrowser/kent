<?php
$wgExtensionFunctions[] = "wfExtensionSpecialUserlogoutUCSC";
function wfExtensionSpecialUserlogoutUCSC() {
    global $wgMessageCache;
    $wgMessageCache->addMessages(array('userlogoutucsc' => 'UCSC Genome Browser / Genomewiki Login'));
    SpecialPage::addPage( new SpecialPage( 'UserlogoutUCSC' ) );
}
//extension specific configuration options (like new user groups and perms) here
?>
