import com.meterware.httpunit.*;

/**
 *  This container holds the data for a robot
 */
public class Robot {

  // data
  String initialURL;
  WebConversation wc; 
  WebResponse currentPage;

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

}
