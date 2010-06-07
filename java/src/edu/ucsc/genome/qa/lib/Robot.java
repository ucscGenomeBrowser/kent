package edu.ucsc.genome.qa.lib;
import com.meterware.httpunit.*;

/**
 *  This container holds the data for a robot
 */
public class Robot {

  // data
  public String initialURL;
  public WebConversation wc; 
  public WebResponse currentPage;

  // constructors
 /**
  *  @param myURL  The starting point
  */
  public Robot(String myURL) throws Exception {

    // save this in case we need to reinitialize
    initialURL = myURL;
    wc = new WebConversation();
    WebRequest req = new GetMethodWebRequest(myURL);
    currentPage = wc.getResponse(req);

  }

 /**
  * Clicks link and gets response code from server
  *
  * @param wc    The open web connection
  * @param link  The link
  * @return      The response code    
  */

  // link is known to be URL
  public static int getResponseCode(WebConversation wc, WebLink link) {
    try {
        link.click();

        // disable Javscript spew
        HttpUnitOptions.setExceptionsThrownOnScriptError(false);

	WebResponse resp = wc.getCurrentPage();
	return resp.getResponseCode();
    } catch (Exception e) {
        System.err.println(e.getMessage());
        return 0;
    }
  }
}
