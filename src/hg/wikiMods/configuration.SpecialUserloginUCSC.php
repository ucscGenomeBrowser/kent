<?php
$wgExtensionFunctions[] = "wfExtensionSpecialUserloginUCSC";
function wfExtensionSpecialUserloginUCSC() {
    global $wgMessageCache;
    $wgMessageCache->addMessages(array('userloginucsc' => 'UCSC Genome Browser / Genomewiki Login'));
    SpecialPage::addPage( new SpecialPage( 'UserloginUCSC' ) );
}
//extension specific configuration options (like new user groups and perms) here
?>
