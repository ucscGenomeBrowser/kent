    $(document).ready(function(){

            $("a.change").each(function(){ 
                    this.href = this.href.replace(/^http:\/\/genome\.ucsc\.edu/, "http://" +window.location.host);
                });
            
            $("a.insideLink").each(function(){ 
                    this.href = this.href.replace(/=http:\/\/genome\.ucsc\.edu/, "=http://" +window.location.host);
                });

     });
     
