<?php
/**
 * @file
 * @ingroup SpecialPage
 */

/**
 * constructor
 */
function wfSpecialUserlogoutUCSC() {
	global $wgUser, $wgOut, $wgRequest;

	$oldName = $wgUser->getName();
	$wgUser->logout();
	$wgOut->setRobotpolicy( 'noindex,nofollow' );

	// Hook.
	$injected_html = '';
	wfRunHooks( 'UserLogoutComplete', array(&$wgUser, &$injected_html, $oldName) );

	$wgOut->addHTML( wfMsgExt( 'logouttext', array( 'parse' ) ) . $injected_html );
	returnToExternal( true, $wgRequest->getVal( 'returnto' ) );
}

/**
 * @access private
 */
function returnToExternal( $auto = true, $returnto = NULL ) {
	global $wgUser, $wgOut, $wgRequest;

	if ( $returnto == NULL ) {
		$returnto = $wgRequest->getText( 'returnto' );
	}
		$sk = $wgUser->getSkin();
	if ( '' == $returnto ) {
		$returnto = wfMsgForContent( 'mainpage' );
	}
	if ( $auto ) {
		$wgOut->addMeta( 'http:Refresh', '1;url=' . $returnto );
	}
	$wgOut->addHTML( "\n<p>" );
	$sk = $wgUser->getSkin();
	$wgOut->addHTML($sk->makeExternalLink($returnto, "Return to $returnto", false));
	$wgOut->addHTML( "</p>\n" );
}

