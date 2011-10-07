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

	/**
	 * Some satellite ISPs use broken precaching schemes that log people out straight after
	 * they're logged in (bug 17790). Luckily, there's a way to detect such requests.
	 */
	if ( isset( $_SERVER['REQUEST_URI'] ) && strpos( $_SERVER['REQUEST_URI'], '&amp;' ) !== false ) {
		wfDebug( "Special:Userlogout request {$_SERVER['REQUEST_URI']} looks suspicious, denying.\n" );
		wfHttpError( 400, wfMsg( 'loginerror' ), wfMsg( 'suspicious-userlogout' ) );
		return;
	}
	
	$oldName = $wgUser->getName();
	$wgUser->logout();
	$wgOut->setRobotPolicy( 'noindex,nofollow' );

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

