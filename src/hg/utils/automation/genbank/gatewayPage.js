function setWgetSrc() {
  var srcHost = "//hgdownload.soe.ucsc.edu";
  var hostName = location.hostname;
  if( /^hgwdev.*|^genome-test.*/i.test(hostName) ) {
    srcHost = "//genome-test.soe.ucsc.edu";
  } else if ( /^genome-preview.*|^hgwalpha.*/i.test(hostName) ) {
    srcHost = "//genome-preview.soe.ucsc.edu";
  } else if ( /^genome.ucsc.*/i.test(hostName) ) {
    srcHost = "//hgdownload.soe.ucsc.edu";
  }

  var wgetSrc = document.getElementById('wgetSrc');
  if (wgetSrc.innerText) {
    wgetSrc.innerText = srcHost;
  }
  else
  if (wgetSrc.textContent) {
    wgetSrc.textContent = srcHost;   
  }

  if (typeof asmId === 'undefined' || asmId === null) { asmId = "notSet"; }

  var u = "ht"+"tp://geno"+"mewiki.ucsc.e"+"du/cg"+"i-bin/useC"+"ount?version=hub."+asmId;
  $.get(u);
}
window.onload = setWgetSrc();
