<?php

	/* http://hgwdev-kate.soe.ucsc.edu/template.html */

	/**************************************************************************
	* Set up landing page 
	***************************************************************************/
	if (!isset($_GET['page'])) 
		{
		$_GET['page'] = 'welcome'; 
		$section['welcome'] = 'Welcome'; 
		}

	/**************************************************************************
	* Set up colors 
	***************************************************************************/
	$theme['cescg'] 	= "FF8C00";
	$theme['team'] 		= $theme['cescg'];
	$theme['access'] 	= "4682B4";
	$theme['files'] 	= "C71585";
	$theme['projects'] 	= "1E90FF";
	$theme['atlas'] 	= "5cb85c"; // = "3CB371";
	$theme['footer'] 	= "aaaaaa";
	$theme['welcome'] 	= "aaaaaa";

	/**************************************************************************
	* Set up titles in the colored navigation bar
	***************************************************************************/
	$section['cescg'] 		= "About the Stem Cell Hub and CESCG (CIRM Genomics Initiative)";
	$section['team'] 		= "Get to know the people behind the CESCG and Stem Cell Hub";
	$section['access']		= "Obtain access to research data";
	$section['files'] 		= "Files";
	$section['projects'] 	= "View our CIRM CESCG Projects";
	$section['atlas'] 		= "Single Cell Brain Atlas and Differentiation Scorecard";
	
	/**************************************************************************
	* To parse the URL
	***************************************************************************/
	$url = "$_SERVER[HTTP_HOST]$_SERVER[REQUEST_URI]";

?>
<!DOCTYPE html>
<html lang="en">
	<head>
		<meta charset="utf-8">
		<meta http-equiv="X-UA-Compatible" content="IE=edge">
		<meta name="viewport" content="width=device-width, initial-scale=1">
		<!-- The above 3 meta tags *must* come first in the head; any other head content must come *after* these tags -->
		<title>Stem Cell Hub, California Institute for Regenerative Medicine CESCG at UC Santa Cruz</title>
		<link rel="shortcut icon" href="https://www.cirm.ca.gov/sites/default/files/favicon-cirm-2_0.png" type="image/png" />
		<script src="//ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js"></script>	

		<!-- Bootstrap -->
		<link href="https://maxcdn.bootstrapcdn.com/font-awesome/4.7.0/css/font-awesome.min.css" rel="stylesheet">

		<!-- HTML5 shim and Respond.js for IE8 support of HTML5 elements and media queries -->
		<!-- WARNING: Respond.js doesn't work if you view the page via file:// -->
		<!--[if lt IE 9]>
		<script src="https://oss.maxcdn.com/html5shiv/3.7.3/html5shiv.min.js"></script>
		<script src="https://oss.maxcdn.com/respond/1.4.2/respond.min.js"></script>
		<![endif]-->
		<!-- Latest compiled and minified CSS -->
		<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css" integrity="sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u" crossorigin="anonymous">

		<!-- Optional theme -->
		<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap-theme.min.css" integrity="sha384-rHyoN1iRsVXV4nD0JutlnGaslCJuC7uwjduW9SVrLvRYooPp2bWYgmgJQIXwl/Sp" crossorigin="anonymous">

		<!-- Latest compiled and minified JavaScript -->
		<script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js" integrity="sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa" crossorigin="anonymous"></script>
		<style>
			input.form-control { width: 300px; }
			textarea.form-control { width: 500px; height: 200px;}


			.label-cescg 						{ background-color: #<?php echo $theme['cescg']; ?>; }
			.label-cescg[href]:hover, .label-cescg[href]:focus 	{ background-color: #808080; }

			.label-access						{ background-color: #<?php echo $theme['access']; ?>; }
			.label-access[href]:hover, .label-access[href]:focus 	{ background-color: #808080; }

			.label-files 						{ background-color: #<?php echo $theme['files']; ?>; }
			.label-files[href]:hover, .label-files[href]:focus 	{ background-color: #808080; }

			.label-projects 					{ background-color: #<?php echo $theme['projects']; ?>; }
			.label-projects[href]:hover, .label-projects[href]:focus { background-color: #808080; }

			.label-atlas 						{ background-color: #<?php echo $theme['atlas']; ?>; }
			.label-atlas[href]:hover, .label-atlas[href]:focus 	{ background-color: #808080; }

			.a-unstyled { color: inherit; }
			.a-unstyled:link { color: inherit; }
			.a-unstyled:visited { color: inherit; }
			.a-unstyled:hover { color: inherit; text-decoration: none; }
			.a-unstyled:active { color: inherit; text-decoration: none; }

			.standsout { color: #ffffff; background-color: #1E90FF; padding-left: 3px; padding-right: 3px;}
			.standsout:link { color: #ffffff !important; background-color: #1E90FF; }
			.standsout:visited { color: #ffffff !important; background-color: #1E90FF;}
			.standsout:hover { color: #cccccc; text-decoration: none !important; background-color: #C71585;}
			.standsout:active { color: #ffffff; text-decoration: none !important; background-color: #1E90FF;}
	
			.nopadding { padding: 0 !important; margin: 0 !important; }
			.infoboxpadding { padding-right: 0 !important; margin-right: 0 !important; }

			.btnexpand:after { font-family: "Glyphicons Halflings"; content: "\e114"; float: right; margin-left: 15px; }
			.btnexpand.collapsed:after { content: "\e080"; } 
	/*		.verticaltext { width: 20px; transform: rotate(-90deg); transform-origin: right, top; -ms-transform: rotate(-90deg); -ms-transform-origin:right, top; -webkit-transform: rotate(-90deg); -webkit-transform-origin:right, top; color: #ed217c; position: relative; }

.verticaltext {
    -webkit-transform: rotate(-90deg);
    -moz-transform: rotate(-90deg);
    -ms-transform: rotate(-90deg);
    -o-transform: rotate(-90deg);
    filter: progid:DXImageTransform.Microsoft.BasicImage(rotation=3);
    left: -40px;
    top: 35px;
    position: absolute;
    color: #FFF;
    text-transform: uppercase;
    font-size:26px;
    font-weight:bold;
}
*/

.singlecell { color: #ed217c; }
.transcriptome { color: #<?php echo $theme['projects']; ?>; }
.genome  { color: #<?php echo $theme['files']; ?>; }
.epigenome { color: #<?php echo $theme['atlas']; ?>; }
.otherome { color: #<?php echo $theme['cescg']; ?>; }


.table-header-rotated th.row-header{
  width: auto;
}

.table-header-rotated td{
  width: 40px;
  border-top: 1px solid #dddddd;
  border-left: 1px solid #dddddd;
  border-right: 1px solid #dddddd;
  vertical-align: middle;
}

.table-header-rotated th.rotate-45{
  height: 80px;
  width: 40px;
  min-width: 40px;
  max-width: 40px;
  position: relative;
  vertical-align: bottom;
  padding: 0;
  font-size: 11px;
  line-height: 0.8;
}

.table-header-rotated th.rotate-45 > div{
  position: relative;
  top: 0px;
  left: 40px; /* 80 * tan(45) / 2 = 40 where 80 is the height on the cell and 45 is the transform angle*/
  height: 100%;
  -ms-transform:skew(-45deg,0deg);
  -moz-transform:skew(-45deg,0deg);
  -webkit-transform:skew(-45deg,0deg);
  -o-transform:skew(-45deg,0deg);
  transform:skew(-45deg,0deg);
  overflow: hidden;
  border-left: 1px solid #dddddd;
  border-right: 1px solid #dddddd;
  border-top: 1px solid #dddddd;
}

.table-header-rotated th.rotate-45 span {
  -ms-transform:skew(45deg,0deg) rotate(315deg);
  -moz-transform:skew(45deg,0deg) rotate(315deg);
  -webkit-transform:skew(45deg,0deg) rotate(315deg);
  -o-transform:skew(45deg,0deg) rotate(315deg);
  transform:skew(45deg,0deg) rotate(315deg);
  position: absolute;
  bottom: 30px; /* 40 cos(45) = 28 with an additional 2px margin*/
  left: -25px; /*Because it looked good, but there is probably a mathematical link here as well*/
  display: inline-block;
  // width: 100%;
  width: 85px; /* 80 / cos(45) - 40 cos (45) = 85 where 80 is the height of the cell, 40 the width of the cell and 45 the transform angle*/
  text-align: left;
  // white-space: nowrap; /*whether to display in one line or not*/
}


.hideOverflow
{
    overflow:hidden;
    white-space:nowrap;
    text-overflow:ellipsis;
    width:100%;
    display:block;
}

.popover {
        max-width:800px;
/*
		left: 10px !important;
		margin-right: 10px; */
    }

.popoveradjust {
	  padding: 5px 0;
  margin-top: -3px;
    }

		</style>
	</head>
	<body>

		<script>
			$(document).ready(function(){
    			$('[data-toggle="popover"]').popover(); 
			});

		</script>

		<div style="width: 100%; background-color: #4682B4; height: 20px;">
		</div>
		<div class="container container-fluid container-table" style="width: 90%">
			<div class="row">
						<div class="btn-group pull-right" style="margin-top: 10px;">
    							<button type="button" class="btn btn-success dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
									Single Cell Atlases <span class="caret"></span>
								</button>
								<ul class="dropdown-menu">
										<li><a href="?page=atlas&title=Brain of Cells">Brain of Cells V4</a></li>
										<li role="separator" class="divider"></li>
										<li><a href="?page=atlas">Heart of Cells</a></li>
										<li role="separator" class="divider"></li>
										<li><a href="?page=atlas">All Single Cell Datasets</a></li>
								</ul>
						</div>	
				<a href="?"><img src="logo5.png" class="img-responsive" style=" margin-top: 15px;" /></a>
				<h3>
					<div style="float: right; width: 200px; margin-top: 6px" class="input-group">
						<input type="text" class="form-control" placeholder="Search for...">
						<span class="input-group-btn">
							<button class="btn btn-default" type="button">search</button>
						</span>
					</div>
					<div style="line-height: 1.5em;">
						<a class="a-unstyled" href="?page=cescg"><span class="label label-cescg">CESCG Organization</span></a>
						<a class="a-unstyled" href="?page=access"><span class="label label-access">Research Access</span></a>
						<a class="a-unstyled" href="http://cirm.ucsc.edu/cgi-bin/cdwWebBrowse?cdwCommand=browseFiles"><span class="label label-files">Files</span></a>
						<a class="a-unstyled" href="?page=projects"><span class="label label-projects">Projects</span></a>

					</div>
				</h3>


				<div style="color: #ffffff; background: #<?php echo $theme[$_GET['page']] ?>; padding-left: 5px; margin-top: 10px; ">
					<h4 style=" padding-top: 5px; padding-bottom: 5px;">
						<?php 
							echo $section[$_GET['page']]; 
							if ($_GET['page'] == 'footer' && isset($_GET['title']) )
								{ 
								echo $_GET['title']; 
								}
							?>
					</h4>
				</div>
			</div>

			<br />

			<div class="row">
				<div class="col-sm-<?php if($_GET['page'] == 'projects') { echo "12"; } else { echo "9"; } ?> nopadding">

					<?php if ($_GET['page'] == 'welcome') { ?>

							<p>Welcome to the CIRM Genomics Hub, created from the Genomics Initiative from the California Institute for Regenerative Medicine's Center of Excellence for Stem Cell Genomics (CESCG).</p>
							<p>The CESCG is made up of approximately 20 separate projects, all involving stem or progenitor cells. The first release of information explores single cell expression datasets in the human brain, tools to deconvolute complex mixtures of brain cells into cell types and developmental states, and scorecards for in vitro differentiation into brain subclasses.</p>

					<?php } elseif ($_GET['page'] == 'cescg') { ?>

							<p class="lead"><span style="color: #<?php echo $theme['projects'] ; ?>">CESCG</span>: The CIRM Center of Excellence in Stem Cell Genomics</p>
							<p>CIRM's goal in establishing the <a href="https://www.cirm.ca.gov/researchers/genomics-initiative/about" target="_blank" class="standsout">CESCG <i class="fa fa-external-link" aria-hidden="true"></i></a> is to apply genomics and bioinformatics approaches to stem cell research to accelerate fundamental understanding of human biology and disease mechanisms, enhance cell and tissue production and advance personalized cellular therapeutics.</p>
							<p>The CESCG is composed of Operational Cores at <a href="http://med.stanford.edu/cescg.html" target="_blank" class="standsout">Stanford University <i class="fa fa-external-link" aria-hidden="true"></i></a> and at <a href="http://www.salk.edu/science/research-centers/center-of-excellence-in-stem-cell-genomics/" target="_blank" class="standsout">the Salk Institute <i class="fa fa-external-link" aria-hidden="true"></i></a> and a Data Coordination and Management Core at the University of California Santa Cruz, which currently support the following research programs:</p>
							<p>1) Center-initiated Projects: Two projects are applying advanced genomics approaches, one to explore cardiac disease and drug toxicity and the other to investigate cell fate and identity. A third project is developing innovative bioinformatics tools to establish molecular network models and to guide predictions of cell fate.</p>
							<p>2) Collaborative Research Program: Through the solicitation and participation in collaborative research projects, the CESCG provides stem cell scientists throughout the state of California with access to cutting-edge genomics and bioinformatics technologies, and expertise and assistance in experimental design and data analysis.</p>
							<p>Components of CIRM's Genomics Initiative:</p>
							<img src="https://www.cirm.ca.gov/sites/default/files/images/about_stemcells/genomics%20reduced.png" class="img-responsive">

					<?php } elseif ($_GET['page'] == 'team') { ?>
						<a name="CESCG"></a>
						<p class="lead">CIRM CESCG</p>
						<p>Lorem ipsum</p>
						

						<a name="DCM"></a>
						<p class="lead">Data Coordination and Management Group (DCM)</p>
						<p>Lorem ipsum</p>


						<a name="DCG"></a>
						<p class="lead">Data Curation Group (DCG)</p>
						<p>Lorem ipsum</p>


					<?php } elseif ($_GET['page'] == 'access') { ?>					
						

<?php
	if (!$ok && !$_POST["mail_submission"])
		{
				echo '<p class="lead">Research access to files requires additional authentication.</p>';
				echo '<p>Contact the data wranglers using the form below to request access to our secure site, or to ask a question. We look forward to working with you!</p>';
			

		}
	if ($_POST["mail_submission"])
		{
			$count = 0;
			$email = $_POST['email'];
			$domain = substr(strrchr($email, "@"), 1);

			/****************************************************
			| Magic regex to validate email. Not perfect.		|
			****************************************************/
			function validateEmail($email)
				{
					return filter_var($email, FILTER_VALIDATE_EMAIL);
				}

			/****************************************************
			| Check if domain has an MX record to see exists.	|
			****************************************************/
			function validateDomain($domain)
				{
					return checkdnsrr($domain, 'MX');
				}

			/****************************************************
			| Now use together to see if email is legit	|
			****************************************************/
			if (validateEmail($email) == TRUE)
				{
					echo "";
				} else {
					$errorList[$count] = "Missing or invalid: email address";
					$emailErrorDesc1 = "<p class=\"text-danger\">This email address does not appear valid.</p>";
					$count++;
					$emailError = "1";
				}
			if (validateDomain($domain) == TRUE)
				{
					echo "";
				} else {
					$errorList[$count] = "Missing or invalid: email domain name";
					$emailErrorDesc2 = "<p class=\"text-danger\">This email's domain name does not appear to be real.</p>";
					$count++;
					$emailError = "1";
				}

			if (!$_POST["name"])	{ $nameError = "1"; $errorList[$count] = "Missing or invalid: your name";  $count++; }
			if (!$_POST["subject"]) { $subjectError = "1"; $errorList[$count] = "Missing or invalid: subject";	$count++; }
			if (!$_POST["message"]) { $messageError = "1"; $errorList[$count] = "Missing or invalid: message";	$count++; }
			
			if (sizeof($errorList) == 0) 
				{

				/*-------------------------------------------------------------+
				| Make an email subject template.							  |
				+-------------------------------------------------------------*/
				$subjectPackage = $_POST["subject"]." | CIRM CESCG DCM contact form";

				/*---------------------------------------------------------+
				| Email template										   |
				+---------------------------------------------------------*/
				$emailPackage  =	'-------  -----------------------------------------------------------------------';
				$emailPackage .= "\n"  .'From     '.$_POST["name"].' <'.$_POST["email"].'> ';
				$emailPackage .= "\n"  .'-------  -----------------------------------------------------------------------';
				$emailPackage .= "\n"  .'Subject  '.$subjectPackage;
				$emailPackage .= "\n"  .'--------------------------------------------------------------------------------';
				$emailPackage .= "\n"  .'Message';
				$emailPackage .= "\n\n".$_POST["message"];
				$emailPackage .= "\n\n".'--------------------------------------------------------------------------------';


				/*-------------------------------------------------------------+
				| Setup Email Headers.										 |
				+-------------------------------------------------------------*/
				$headers = 'Reply-To:'.$_POST["email"]."\r\n";
				if($rdType == 1)
					{
						$headers .= "MIME-Version: 1.0\n" . "Content-type: text/html; charset=iso-8859-1";
						$headers .= "X-mailer: soe.ucsc.edu" . "\r\n"; // this will identify the real sender
						$headers.= "Precedence: bulk" . "\r\n"; // this will say it is bulk sender
						$emailPackage = stripslashes($emailPackage);
					} else {
						$headers .= "MIME-Version: 1.0\n" . "Content-type: text/plain; charset=iso-8859-1";
					}

				#$emailto = "clmfisch@ucsc.edu";
				$emailto = "cirm-wrangler-group@ucsc.edu";
				/*-------------------------------------------------------------+
				| Send the Mail.											   |
				+-------------------------------------------------------------*/
				$ok = @mail($emailto, $subjectPackage, $emailPackage, $headers);

				if ($ok) 
					{
						/*---------------------------------------------------------+
						| Print out a template to show submission.				 |
						+---------------------------------------------------------*/
						echo '<div class="alert alert-success">Thank you, your email was sent successfully.</div>';
						echo "<pre>".$emailPackage."</pre>";
					} else {
						/*---------------------------------------------------------+
						| In the future, automatically send an error to me.		|
						+---------------------------------------------------------*/
						echo "<p>It seems the mail could not be sent, please try again.</p>";
					}
			/*-----------------------------------------------------------------+
			| Errors were found
			+-----------------------------------------------------------------*/
				} else {
					echo '<div class="alert alert-danger">Sorry, we encountered ';
					if ($count == 1) { echo "an error"; } else { echo $count." errors";}
					echo ". Please fix the highlighted region below.</div>";
					/*
					for ($i = 0; $i < count($errorList); $i++)
					{
						echo $errorList[$i] . "<br>";
					}
					*/
				}
	}

	if (!$ok)
		{
?>
		<div style="background: #d7eaf2; padding: 10px; width: 520px; border: 1px solid #ccc;">

		<form method="POST">
			<div class="form-group">
				<label for="exampleInputEmail1"<?php if ($emailError) { echo ' class="label label-danger " style="font-size: 14px;"'; } ?>>Your email address</label>
				<input type="email" class="form-control" name="email" placeholder="Email" value="<?php echo $_POST["email"]; ?>">
				<?php echo $emailErrorDesc1.$emailErrorDesc2; ?>
			</div>
			<div class="form-group">
				<label for="exampleInputEmail1"<?php if ($nameError) { echo ' class="label label-danger " style="font-size: 14px;"'; } ?>>Name</label>
				<input type="text" class="form-control" name="name" placeholder="Name" value="<?php echo $_POST["name"]; ?>">
			</div>
			<div class="form-group">
				<label for="exampleInputEmail1" <?php if ($subjectError) { echo ' class="label label-danger " style="font-size: 14px;"'; } ?>>Subject</label>
				<input type="text" class="form-control" name="subject" placeholder="Subject" value="<?php echo $_POST["subject"]; ?>">
			</div>
			<div class="form-group">
				<label for="exampleInputEmail1"<?php if ($messageError) { echo ' class="label label-danger " style="font-size: 14px;"'; } ?>>Message</label>
				<textarea class="form-control" rows="3" name="message" placeholder="Your message or question"><?php echo $_POST["message"]; ?></textarea>
			</div>
		<br>
		<div class="pull-right">
		<button name="mail_submission" value="1" type="submit" class="btn btn-default">Submit</button>
		</div>
		<br><br>
		</form>
		</div>
	<?php } ?>


					<?php } elseif ($_GET['page'] == 'files') { ?>

						<p class="lead">Public files</p>
						<p>Here you can download all published, non-identifiable data.</p>
						<button type="button" class="btn btn-sm btn-default btnexpand collapsed" data-toggle="collapse" data-target="#demo">learn more</button>
						<div id="demo" class="collapse">Lorem ipsum</div>

					<?php } elseif ($_GET['page'] == 'projects') { ?>

						<p class="lead">Research themes</p>


						<div class="table-responsive scrollable-table" style="padding-right: 80px;">
								<table class="table table-striped table-header-rotated" style="">
										<thead>
												<tr>
														<th style="width: 1%;" class="text-nowrap"><br>Research Theme</th>
														<th style="width: 1%;" class="text-nowrap">Lab</th>
														<th style="width: 1%;" class="text-nowrap ">Data Set</th>
														<th style="width: 1%;" class="singlecell text-nowrap">Single cell <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="It is possible to assay individual cells from a population to learn more about the cells making up a community, also known as studying the <em>cellular heterogenetity</em>."><i class="fa fa-info-circle" aria-hidden="true"></i></a></th>
														<th style="width: 1%;" class="rotate-45 genome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> WGS</span</div></th>
														<th style="width: 1%;" class="text-nowrap rotate-45 genome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> Amplicon-seq</span</div></th>
														<th style="width: 1%;" class="rotate-45 transcriptome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Some RNA-seq is done on bulk tissue, though for this project much of it is done on single cells. The icons in the single cell column indicate how the cells were handled to generate the RNA-seq data."><i class="fa fa-info-circle" aria-hidden="true"></i></a> RNA-seq</span></div></th>
														<th style="width: 1%;" class="rotate-45 transcriptome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> Capture-seq</span></div></th>
														<th style="width: 1%;" class="rotate-45 transcriptome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> Frac-seq</span></div></th>
														<th style="width: 1%;" class="text-nowrap rotate-45 epigenome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> broad-ChIP-seq</span></div></th>
														<th style="width: 1%;" class="text-nowrap rotate-45 epigenome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> narrow-ChIP-seq</span></div></th>
														<th style="width: 1%;" class="rotate-45 epigenome"><div><span><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> TCL-seq</span></div></th>
														<th style="width: 1%;" class="rotate-45 epigenome"><div><span><a href="" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Information about "><i class="fa fa-info-circle" aria-hidden="true"></i></a> WGBS</span></div></th>
														<th style="width: 1%;" class="rotate-45 epigenome"><div><span><a href="https://www.ncbi.nlm.nih.gov/pubmed/16224102" target="_blank" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Reduced-representation Bisulifite Sequencing to determine methylation of promoter sites" data-content="Click the info icon to see the paper (2005)"><i class="fa fa-info-circle" aria-hidden="true"></i></a> RRBS</th>
														<th style="width: 1%;" class="rotate-45 epigenome"><div><span><a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4374986/" target="_blank" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Transposase-Accessible Chromatin with high throughput sequencing" data-content="Click the info icon to see the ATAC-seq publication."><i class="fa fa-info-circle" aria-hidden="true"></i></a> ATAC-seq</span></div></th>
														<th style="width: 1%;" class="rotate-45 epigenome"><div><span><a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3149993/" target="_blank" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Assay to determine spatial organization of chromatin" data-content="Click the info icon to see the Hi-C paper (2010)."><i class="fa fa-info-circle" aria-hidden="true"></i></a> Hi-C</span></div></th>
														<th style="width: 1%;" class="rotate-45 otherome"><div><span><a href="http://www.nature.com/nbt/journal/v33/n2/full/nbt.3117.html" target="_blank" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Genome-wide Unbiased Identifications of double strand breaks (DSBs) Evaluated by Sequencing" data-content="Click the info icon to see the paper (2014)."><i class="fa fa-info-circle" aria-hidden="true"></i></a> GUIDE-seq</span></div></th>

												</tr>
										</thead>
										<tbody>
												<tr style="font-size: 75%">
														<td colspan="3"></td>
														<td class="singlecell text-nowrap"></td>
														<td colspan="2" class="genome"></td>
														<td colspan="3" class="transcriptome"></td>
														<td colspan="3" class="epigenome text-center">Histone modification</td>
														<td colspan="2" class="epigenome text-center">Methylome</td>
														<td colspan="2" class="epigenome text-nowrap text-center">Chromatin modeling</td>
														<td colspan="1" class="otherome text-nowrap text-center">Gene editing profiling</td>
												</tr>
												<tr>
														<td colspan="3"></td>
														<td class="singlecell text-nowrap"></td>
														<td colspan="2" class="genome text-center">Genomics</td>
														<td colspan="3" class="transcriptome text-center">Transcriptomics</td>
														<td colspan="7" class="epigenome text-center">Epigenomics</td>
														<td colspan="1" class="otherome"></td>
												</tr>
												<tr>
														<td colspan="100%">Neurobiology</td>
												</tr>
												<!--
												<tr>
														<td></td>
														<td class="lab text-nowrap">Smith</td>
														<td class="datasetid">labAssaySample</td>
														<td class="singlecell text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="genome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="genome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center">X <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Capture-seq data" data-content="Information about this data set"><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="otherome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
												</tr>
												-->
												<tr>
														<td></td>
														<!-- <a href="#" title="Dismissible popover" data-toggle="popover" data-trigger="focus" data-content="C">Click me</a>  -->
														<td class="lab text-nowrap"><a href="https://quakelab.stanford.edu/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Stephen Quake, Stanford University" data-content="Stephen Quake (PI) is the Lee Otterson Professor of Bioengineering and Applied Physics at Stanford University, and an Investigator at the Howard Hughes Medical Institute. He is recognized as one of the fathers of microfluidics and a pioneer of genomics.<br /><br />He has received many prizes for his discoveries and inventions, including the Lemelson-MIT Prize for invention and innovation, the Nakasone Prize from the Human Frontiers of Science Foundation, the Sackler International Prize for Biophysics, and the Promega Biotechnology Award from the American Society for Microbiology.">Quake</a></td>
														<td class="datasetid">quakeBrainGeo1 
															<a href="https://www.ncbi.nlm.nih.gov/pubmed/26060301" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title=" Examination of cell types in healthy human brain samples" data-content="
															<b>Design:</b> Examination of cell types in healthy human brain samples.<br>
															<b>Dataset summary:</b> We used single cell RNA sequencing on 466 cells to capture the cellular complexity of the adult and fetal human brain at a whole transcriptome level. Healthy adult temporal lobe tissue was obtained from epileptic patients during temporal lobectomy for medically refractory seizures. We were able to classify individual cells into all of the major neuronal, glial, and vascular cell types in the brain.<br>
															<b>Species:</b> Homo sapiens<br>
															<b>Organ:</b> brain<br>
															<b>Body Part:</b> healthy temporal lobe cortex (394), healthy brain cortex (72)<br>
															<b>Lifestage:</b> postpartum (332), embryo (134)<br>
															<b>Sequencer:</b> Illumina NextSeq 500 (328), Illumina MiSeq (138)<br>
															<b>Cell Type:</b> neurons (131), nonreplicating fetal neuron (110), astrocytes (62), hybrid (46), oligodendrocytes (38), replicating fetal neuron (25), endothelial (20), OPC (18), microglia (16)<br>
															"><i class="fa fa-info-circle" aria-hidden="true"></i></a> 
															<a href="https://cirm.ucsc.edu/cgi-bin/cdwGetFile/quakeBrainGeo1/summary/index.html" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="" data-content="View analysis of this dataset"><i class="fa fa-bar-chart" aria-hidden="true"></i></a>
															<a href="https://www.ncbi.nlm.nih.gov/pubmed/26060301" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Quake, S., et al. (2015). A survey of human brain transcriptome diversity at the single cell level. PNAS." data-content="View publication from this dataset"><i class="fa fa-file-pdf-o" aria-hidden="true"></i></a>
															<a href="https://www.ncbi.nlm.nih.gov/geo/query/acc.cgi?acc=GSE67835" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="" data-content="<img src='https://www.ncbi.nlm.nih.gov/geo/img/geo_main.gif' style='height: 2em;'>View data set on GEO"><i class="fa fa-database" aria-hidden="true"></i></a>
														</td>
														<td class="singlecell text-center"><i class="fa fa-braille" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><a href="https://cirm.ucsc.edu/cgi-bin/cdwWebBrowse?cdwCommand=browseFiles&cdwBrowseFiles_f_data_set_id=quakeBrainGeo1" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Download from the Stem Cell Hub" data-content=" Takes you to an external website."><i class="fa fa-cloud-download transcriptome" aria-hidden="true"></i></a></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"></td>
														<td class="datasetid">quakeGlioblastomaGeo1</td>
														<td class="singlecell text-center"><i class="fa fa-braille fa-2x" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://profiles.ucsf.edu/arnold.kriegstein" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Arnold Kriegstein, UCSF" data-content="Cortical progenitor cells give rise to the diverse populations of cortical neurons. Many protocols have been developed to generate cortical neurons from pluripotent stem cells, including several recent protocols that use aggregate culture methods to differentiate human pluripotent stem cells into cerebral organoids. Cerebral organoids consist of heterogeneous populations of progenitors and neurons but the extent to which these in vitro-derived cell types resemble their endogenous counterparts remains unknown. To address this issue, the Kriegstein lab will use single cell RNA profiling at multiple stages of cerebral organoid differentiation and compare these in vitro gene expression profiles with data they are generating for primary human cortical tissues. These comparisons will provide objective measures of the efficiency and accuracy of various differentiation protocols. The ultimate goal of this analysis is to improve differentiation protocols, a necessary effort in order to realize the potential of stem cells for cell replacement therapy as well as for disease modeling.">Kriegstein</a></td>
														<td class="datasetid">kriegsteinBrainOrganoids1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Single-cell Brain Organoids" data-content="874 iPSC-derived brain organoid samples processed via single-cell RNA"><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"><i class="fa fa-braille fa-2x" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="" data-content=""></a></td>
														<td class="datasetid">kriegsteinRadialGliaStudy1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="single-cell brain development" data-content="376 primary samples processed via single-cell rna. published."><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"><i class="fa fa-braille fa-2x" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="https://med.stanford.edu/stemcell/about/Laboratories/clarke.html" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Michael F. Clarke, Stanford" data-content="Michael F. Clarke (co-PI) is the Karel and Avice Beekhuis Professor in Cancer Biology at Stanford University and the Associate Director of the Stanford Institute for Stem Cell Biology and Regenerative Medicine.">Clarke</a></td>
														<td class="datasetid"></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://yeolab.github.io/" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Gene Yeo, UCSD" data-content="In recent years, the importance of post-transcriptional gene regulation (PTGS) underlying neurodegenerative disorders such as amyotrophic lateral sclerosis (ALS), Alzheimer’s and Parkinson’s disease has increased tremendously with the growing number of RNA binding proteins (RBPs) found mutated in patients. Specifically, despite the hundreds of mutations found within these RBPs in patients with ALS, we still do not understand (1) how and why they cause motor neuron degeneration while leaving other cell-types in the central nervous system such as glial cells relatively unscathed, and (2) what aspects of PTGS these mutations affect in these cells. Thus this represents an unmet medical need. In order to study whether these disease-associated mutations result in cell-type specific PTGS, we propose to utilize induced pluripotent stem cells (iPSCs) from ALS patients harboring disease-causing mutations and generate mature neurons, a highly relevant cell-type in ALS. RBPs interact with specific sequences or structural features within transcribed RNAs to affect PTGS. In this proposal we will test specific hypotheses that these mutant RBPs affect cell-type specific alternative splicing and sub-cellular mis-localization of target mRNAs, both highly relevant to known normal functions of these RBPs. We will focus on abundantly expressed RBPs (TDP-43, FUS/TLS, hnRNP A2/B1 and Matrin3) that have been implicated in ALS. We already have generated patient-specific iPSC lines with TDP-43, FUS/TLS and hnRNP A2/B1 mutations. We will use highly optimized protocols to differentiate iPSCs to neurons. For the first time, we will identify mutant-dependent sub-cellular mis-localization of alternative isoform mRNAs in neurons as a novel hypothesis for disease pathology in ALS. To address whether these mutations cause cell-type specific aberrations in alternative splicing, we will utilize single-cell RNA-seq analysis to measure alternative splicing in the neuron culture system, which contains the requisite heterogeneous mixture of glial cells to maintain a healthy neuron differentiation and physiology. For the first time, we will identify mutant-dependent cell-type specific alternative splicing as a hypothesis for disease pathogenesis. These stem-cell based mRNA signatures are a critical resource that we will compare to post-mortem patient material to identify potential therapeutic targets.">Yeo</a></td>
														<td class="datasetid">yeoRnaLocalization1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="Investigating ALS with Frac-seq" data-content="Global subcellular mRNA localization in differentiation stress and neurodegeneration"><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="https://www.uclahealth.org/daniel-geschwind" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Daniel Geschwind, UCLA" data-content="Advances in genetics and genomics over the last decade have led to the identification of specific genetic variants that account for approximately 20% of the risk for autism spectrum disorder (ASD). Many of these variants represent rare, large effect size mutations.  The Geschwind lab has demonstrated a convergent pattern of disrupted gene expression in post mortem ASD brain, but it is not known how multiple distinct mutations lead to this convergent molecular pathology. To address this question, the lab will perform large-scale genomic and phenotypic characterization of induced pluripotent stem cell (iPSC)-based models of ASD, representing patients with distinct ASD risk mutations, patients with idiopathic ASD and healthy controls. Neurons will be generated from iPSC using an innovative 3D neural differentiation system that generates a laminated human cortex, including synaptically connected neurons and astrocytes. These in vitro models will be phenotyped for morphological and physiological abnormalities using automated assays and entire transcriptional networks will be analyzed during in vitro development. In addition, whole genome sequence will be obtained and together these data will be used to identify potential causal factors and key regulatory drivers of the disease. This will provide new mechanistic insight in ASD pathophysiology and provide an invaluable and unprecedented resource for the field. Furthermore, this project leverages iPSC lines generated as part of the CIRM hiPSC Initiative and capitalizes on tools being developed by the Ideker lab as part of the broader CESCG toolbox.">Geschwind</a></td>
														<td class="datasetid">geschwindNeurospheres1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="Neurosphere RNA-seq data from the Geschwind lab at UCLA. iPSC-based models of autism spectrum disorder. Unpublished and confidential."><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
<!--
												<tr>
														<td></td>
														<td class="lab text-nowrap">Brain of Cells</td>
														<td class="datasetid"><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""></a>Or should these be columns?</td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
-->
												<tr>
														<td colspan="100%">Cardiac Biology</td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://frazer.ucsd.edu/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Kelly Frazer, UCSD" data-content="Sudden cardiac arrest (SCA) is a leading cause of death among adults over the age of 40 in the United States. It is usually caused by ventricular arrhythmias (irregular heartbeats) due to abnormalities in the heart’s electrical system. In addition to naturally occurring SCA, drug therapies can cause a form of acquired arrhythmia that leads to SCA. Clinical risk factors for drug-induced arrhythmias include gender, age and existent cardiac and/or liver disease, and there is evidence for a genetic predisposition. The Frazer lab has generated human induced pluripotent stem cells (hiPSCs) from 225 individuals and has established protocols for large-scale derivation of human cardiomyocyte lines from these hiPSCs. They will use these lines for the study of ventricular arrhythmias, both in the naïve state or triggered by drug administration. The lab has NIH funding to identify genetic variants that are associated with electrophysiological phenotypes, while this work will identify genetic variants associated with genome-wide RNA expression levels and the epigenome. Together, these data will provide new knowledge of the biology of arrhythmias that may be exploited to improve treatment options for SCA.">Frazer</a></td>
														<td class="datasetid">frazerCardIps1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://www.salk.edu/scientist/katherine-jones/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Kathy Jones, The Salk Institute" data-content="The Wnt3a/β-catenin and Activin/SMAD2,3 signaling pathways synergize to induce hESC/iPSC differentiation to mesoderm, an intermediate step in the development of heart, intestine, liver, pancreatic and other cell lineages. Our lab recently investigated the transcriptional mechanism underlying the Wnt-Activin synergy in human embryonic stem cells using genomewide ChIP-seq, GRO-seq, and 3C studies. We found that Wnt3a signaling stimulates the assembly of β-catenin:LEF-1 enhancers that contain a unique variant form of RNAPII, which is phosphorylated at the Ser5, but not Ser7, positions in the CTD heptad repeats. Importantly, this variant RNAPII was also found at all hESC enhancers, whereas RNAPII at active promoters was highly phosphorylated at both Ser5 and Ser7. Phosphorylation of RNAPII-Ser7 in hESCs is mediated by the positive transcription elongation factor, P-TEFb (CycT1:CDK9), and we showed that ME gene activation depends on P-TEFb. Interestingly, Wnt3a signaling induces enhancer-promoter looping at mesendermal (ME) differentiation genes, which is facilitated by binding of β-catenin to cohesins. By contrast, Activin/SMAD2,3 did not affect enhancer-promoter interactions, but instead strongly increased P-TEFb occupancy and RNAPII CTD-Ser7P at ME gene promoters. Moreover, Activin is required for CTD-Ser2P and histone H3K36me3 at the 3' end of ME genes. Lastly, we found that many ME genes, including EOMES and MIXL1, were potently repressed by the Hippo regulator complex, Yap1:TEAD, which selectively inhibits P-TEFb elongation, without affecting SMAD2,3 chromatin binding. Thus Wnt3a/β-catenin-induced gene looping synergizes with Activin/Smad2,3-dependent elongation to strongly up-regulate ME genes. Our unpublished data show that CRISPR/Cas-mediated loss of Yap1 also promotes hESC differentiation to mesoderm, but not ectoderm, through increased Activin signaling. Here we will characterize these cells to define how Yap1 is recruited to ME genes and disrupts the P-TEFb CTD kinase to control the earliest steps of hESC differentiation. Further studies will analyze how Yap1 switches to cooperate with Wnt3a at later stages in cardiomyocyte development.">Jones</a></td>
														<td class="datasetid">jonesYap1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://bruneaulab.gladstone.org/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Benoit Bruneau, UCSF" data-content="The goal of this project is to develop an integrated understanding of epigenomic regulation of human cardiac differentiation, in order to apply this knowledge to congenital heart disease (CHD). The Bruneau lab will use patient-specific and engineered human induced pluripotent stem cell (hiPSC) lines to evaluate the impact of heterozygous missense mutations in CHD-associated transcriptional regulators on gene expression and a broad range of chromatin states in cardiac precursors and cardiomyocytes. In support of these goals, the lab will develop human cellular models that represent the two major cardiac cell types, atrial and ventricular cardiomyocytes, using hiPSC engineered to allow the isolation of these two cell types. And finally, they will define dynamic 3D maps of genomic interaction in human cardiac differentiation. This will yield a complete view of the effects of mutations in key chromatin modifying genes on gene regulatory causes of CHD, and an unprecedented view of human cardiac lineage commitment.">Bruneau</a></td>
														<td class="datasetid">bruneauCardioEpigenetics1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://cardiology.ucsd.edu/research/labs/chi-lab/Pages/default.aspx" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Neil Chi, UCSD" data-content="The overall goal of this proposal is to discover gene regulatory networks that will facilitate the differentiation of human pluripotent stem cells (hPSC) into ventricular and atrial cardiomyocyte (CM) lineages for regenerative therapies as well as for more precise modeling and treatment of human congenital or adult heart disease. Toward this goal, in Aim 1, we will perform RNA-seq, ChiP-seq for five histone modifications, and Hi-C analyses on extracts from human CMs purified from each of the four cardiac chambers of human hearts, [right ventricle (RV), left ventricle (LV), right atrium (RA), and left atrium (LA)] to define the transcriptional profiles and genomic regulatory networks for these in vivo specific CM lineages. In Aim 2, a comparable genomic dataset will be obtained for in vitro hPSC-derived ventricular and atrial CMs, at stages selected to correspond to those of characterized human heart samples. Stated objectives of this CESCG Call 2 are to combine genomic and bioinformatics approaches with stem cell research to accelerate fundamental understanding of human biology and disease mechanisms, enhance cell and tissue production, and advance personalized cellular therapeutics. Thus, these cardiac gene expression profiles and epigenetic landscapes to identify enhancers and their cognate promoters will provide critical insights into gene regulatory networks which can be used to identify individual myocardial states and further manipulated to generate specific and mature hPSC-CM lineages that may better recapitulate their in vivo CM counterparts for regenerative therapies and disease modeling. Additionally, these cardiac enhancer/promoter datasets may also provide insights into the significance of potential disease non-coding genetic variants discovered in whole genomic sequencing and genome-wide association heart studies.">Chi</a></td>
														<td class="datasetid">N/A <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://snyderlab.stanford.edu/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Mike Snyder, Stanford" data-content="Professor & Chair of Genetics at Stanford University and Director of the Stanford Center for Genomics & Personalized Medicine. He is an expert in the field of functional genomics and proteomics, and he has developed many genomic technologies and informatics pipelines.">Snyder</a></td>
														<td class="datasetid">snyderCardIps1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td colspan="100%">Blood stem cells and therapeutics</td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://baldwin.scripps.edu/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Kristin Baldwin, Scripps" data-content="Kristin K. Baldwin (co-PI) is an Associate Professor at the Scripps Research Institute. Her laboratory generated the first iPSC lines that could produce an entire organism.">Baldwin</a></td>
														<td class="datasetid text-nowrap">baldwin12PairedIpscCellLines1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://www.crooks-lab.com/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Gay Crooks, UCLA" data-content="Pluripotent stem cells (PSC) are an attractive source for deriving hematopoietic stem cells (HSC) for the treatment of blood disorders. However, yet poorly understood molecular blocks have prevented the generation of functional HSCs in culture. Preliminary studies imply that although cells that possess the immunophenotype of human HSC can be generated, the transcriptome of these cells is dysregulated at multiple levels, including protein coding genes, non-coding RNAs, and mRNA splicing. The Crooks lab will now create a comprehensive transcriptome and epigenome map of human HSC ontogeny by comparing human hematopoietic tissues and embryonic stem cell (ESC)-derived cells, providing a data set in which a limited number of differentially regulated RNAs likely to have functional impact can be identified. Importantly, this analysis will pinpoint key defects that underlie the poor function of ESC-derived hematopoietic cells, and offer new solutions for overcoming these molecular barriers. Moreover, as the hematopoietic hierarchy offers a powerful system for dissecting how transcriptome changes govern tissue development and homeostasis, these studies will provide broader insights to the regulation of stem cell fate decisions and how to induce proper developmental programs for generating tissue stem cells for regenerative medicine.">Crooks</a></td>
														<td class="datasetid">crooksHsc <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://www.scripps.edu/loring/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Jeanne Loring, Scripps" data-content="We are performing preclinical studies for an autologous cell therapy for Parkinson's disease. We have generated iPSCs from 10 Parkinson's patients for our initial cohort, based on criteria established from earlier studies using fetal tissue. Our clinical partner, Dr. Melissa Houser, is Director of the Movement Disorders Clinic at the Scripps Clinic in La Jolla. Our preliminary data include optimized, robust, and reproducible methods for differentiating different lines of patient-specific iPSCs into dopamine neuron precursors, which have shown efficacy in animal models. These are the cell preparations that we plan to transplant to patients to restore their motor control. We also cite our recently published data on genomic stability of human pluripotent stem cells, which pinpoint genomic aberrations that occur when these cells are cultured for very long (>2 years) periods. We include another of our studies, in press in Nature Communications, in which we performed comprehensive analysis of human iPSCs produced by three different reprogramming methods, using whole genome sequencing and de novo genome mapping methods to show that it is unlikely that reprogramming itself will introduce mutations that compromise the safety of iPSCs for therapy. For this grant application, we request funds to perform whole genome and RNA sequencing as quality control measures to assure that the cells that are used for transplantation have no deleterious mutations and are the correct cell type. The mRNAseq studies will expand on our earlier work, funded by CIRM, in which we developed a genomic diagnostic test to determine whether human cells are pluripotent. This gene expression-based diagnostic test, called PluriTest®, is now the most popular assay for pluripotency, recommended by the NIH and used more than 12,000 times since the website (www.pluritest.org) became active. The genome sequencing studies will improve upon our earlier assessments of genomic stability based on SNP genotyping. These studies will allow us to provide the most rigorous predictive assessments of cell therapy safety and efficacy for clinical stem cell applications, and help to set the standards for future clinical studies using human stem cell-derived products.">Loring</a></td>
														<td class="datasetid">loringParkinsons1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://stemcellphd.stanford.edu/faculty/irv-weissman.html" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Irv Weissman, Stanford" data-content="The goal of this program is to address a fundamentally unsolved issue in human development and stem-cell differentiation: how does human mesoderm become diversified into an array of therapeutically-relevant tissues, including bone, cartilage, skeletal muscle, heart, kidneys and blood? A comprehensive understanding of how these tissues all differentiate from a common embryonic mesodermal source might enable us to artificially generate these diverse lineages from human embryonic stem cells (hESCs) for regenerative medicine. Human mesoderm development remains a terra incognita, because it unfolds during human embryonic weeks 2-4, when it is ethically impossible to retrieve fetuses for developmental analyses. Because human mesoderm ontogeny has never been systematically described, the identity of key developmental intermediates and the order of lineage transitions remain unclear. A clear “lineage tree” of mesoderm development would expand developmental and cell biology and enhance the therapeutic potential of regenerative medicine. Hence our goal is to chart a comprehensive map of human mesoderm development, including the specifics of blood development (hematopoiesis), and to systematically reconstruct its component lineage intermediates and their progenitor-progeny relationships.">Weissman</a></td>
														<td class="datasetid"><a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="https://cornlab.com/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Jacob Corn, UC Berkeley" data-content="Sickle cell disease (SCD) is a devastating genetic disorder that affects ~100,000 primarily African American individuals in the USA, including 5,100 in California. In SCD, a Glu to Val point mutation in the ß-globin gene renders the resultant sickle hemoglobin prone to polymerize and damage the red blood cell. We have used CRISPR-Cas9 genome editing to develop methods to correct the sickle allele in hematopoietic stem cells (HSCs) and, together with SCD experts at Children’s Hospital Oakland, are in the process of developing proof-of-concept for a clinical trial to cure SCD via transplantation of gene-corrected autologous HSCs from patients. Our goal in this CESCG CRP is to establish the efficacy and safety of sickle correction in HSCs via targeted and unbiased sequencing. Together with the CESCG we will use next-generation sequencing to determine the extent of allele conversion in edited HSCs both in vitro and after in vivo engraftment in a mouse model. To establish the safety of editing, we will use three sequencing-based approaches. First, we will use custom amplicon-based resequencing to quantify undesired editing events at related globin genes and at sites computationally predicted as potential off-targets based on sequence similarity. Second, we will use established cancer resequencing panels to uncover low-frequency off-target events at genes known to be involved in tumorigenesis, with a focus on annotated tumor suppressors. Third, we will use unbiased capture and sequencing methods, such as the recently described GUIDE-Seq method, to uncover off-target events in the context of the entire human genome. This approach will be critical in providing key data to move editing SCD allele towards the clinic and will also provide an important precedent for establishing efficacy and safety metrics for therapeutic gene editing in HSCs.">Corn</a></td>
														<td class="datasetid">cornErythroidMaturation1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"></td>
														<td class="datasetid">cornOffTarget <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
												</tr>
												<tr>
														<td colspan="100%" class="">Molecular regulators of stem cells</td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://sanfordlab.mcdb.ucsc.edu/Sanford_Lab/Welcome.html" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Jeremy Sanford, UCSC" data-content="Alternative pre-mRNA splicing contributes to the regulation of gene expression and protein diversity. Many human diseases, including Amyotrophic Lateral Sclerosis, Frontotemporal Lobar Degeneration and cancer, are caused or exacerbated by aberrant RNA processing. The goals of this study are to investigate RNA processing during neuronal differentiation of human pluripotent stem cells, using innovative genomics approaches. The knowledge gained is not only critical for understanding how splicing is regulated to control differentiation but also for discovering how genetic variants such as inherited disease mutations disrupt gene expression and function.">Sanford</a></td>
														<td class="datasetid">sanfordRnaRegulation1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="CLIP-seq and RNA-seq on human stem cells and human neural progenitor cells."><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="https://labs.genetics.ucla.edu/fan/index.htm" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Guoping Fan, UCLA" data-content="Human overgrowth syndrome is a class of complex genetic disorders characterized by systemic or regional excess growth compared to peers of the same age. Overgrowth individuals with mutations in DNMT3A, a de novo DNA methyltransferase, are significantly taller, have distinctive cranio-facial features, and exhibit intellectual disability. In this study, the Fan lab will precisely map transcriptome and epigenome dynamics during development of disease-relevant tissues derived from embryonic stem cells that contain a spectrum of DNMT3A knock-in point mutations. The goal is to identify convergent target genes and signaling pathways perturbed in overgrowth syndrome, leading to a rich resource and novel approach to understanding the mechanisms of this disease.">Fan</a></td>
														<td class="datasetid">fanIcf1 <a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content="WGBS RRBS WGS ChIP-seq and RNA-seq on iPSCs generated from patients with human overgrowth syndrome. Unpublished and confidential."><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"><i class="fa fa-square" aria-hidden="true"></i></td>
														<td class="otherome text-center"></td>
												</tr>
												<tr>
														<td></td>
														<td class="lab text-nowrap"><a href="http://belmonte.salk.edu/" target="_blank" data-toggle="popover" data-trigger="hover" data-placement="top"  data-container="body" data-html="true" title="Juan Carlos Izpisua-Belmonte, The Salk Insitute" data-content="Somatic cells can be reprogrammed to an induced pluripotent stem cell (iPSC) state by transient forced expression of the 4 Yamanaka factors (4F) (Oct4, Sox2, Klf4, cMyc). This process has been widely explored in vitro to generate a variety of cell types or to rejuvenate them for their therapeutic application by autologous transplantation. However, transplantation is an invasive procedure and challenged by access to the niche, retention and integration of the graft in addition to the safety and functionality concerns of in vitro-derived cells. An alternative provocative approach is to reprogram cells in vivo. Nevertheless, the effect of 4F in vivo is largely unknown at the molecular level. The major goal of this proposal is to characterize the molecular dynamics of cells undergoing 4F-induced reprogramming in vivo in the mouse. Specifically, we are aiming to identify the changes in transcriptome, DNA methylome and histone methylation status. Toward this end, we will make use of a transgenic mouse that carries a Doxycycline-inducible cassette of 4F whose activity will be also conditional upon Cre expression to control the timing and location of the reprogramming in vivo. The dynamics of several cell types of different embryonic origin will be analyzed through time at the populational and single cell levels. We will also characterize the behavior of the cells undergoing a short-term induction followed by a recovery phase to evaluate if the partially reprogrammed cells will return to their original molecular phenotype or adopt an intermediate, de-differentiated phase. One potential therapeutic application of this approach is the rejuvenation of old cells. Thus, we will compare aged samples exposed to partial reprogramming with the young samples to evaluate if partial reprogramming confers a youthful molecular signature to old cells. Elucidation of the molecular dynamics of in vivo reprogramming in the mouse in collaboration with CESCG will enhance our understanding of the possibilities and risk factors of using this technology in regenerative medicine.">Belmonte</a></td>
														<td class="datasetid">belmonteMouseDnmt3a v<a href="#" data-toggle="popover" data-trigger="hover" class="popoveradjust" data-placement="top"  data-container="body" data-html="true" title="" data-content=""><i class="fa fa-info-circle" aria-hidden="true"></i></a></td>
														<td class="singlecell text-center"></td>
														<td class="genome text-center"></td>
														<td class="genome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="transcriptome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="epigenome text-center"></td>
														<td class="otherome text-center"></td>
												</tr>
										</tbody>
								</table>
							<div class="pull-right" style="background:#fff; border:1px solid #ed217c; padding: 10px; positive: relative; right: -30px;">
								<h4 class="singlecell">Single-cell Legend:</h4>
								<p class=""><i class="fa fa-braille fa-2x singlecell" aria-hidden="true"></i> C1-based (SpeedSeq, SMART-seq2, CEL-seq)</p>
								<p class=""><i class="fa fa-share-alt-square fa-2x singlecell" aria-hidden="true"></i> Drop-seq</p>
								<p class=""><i class="fa fa-flask fa-2x singlecell" aria-hidden="true"></i> 10X Genomics</p>
							</div>
						</div>


						<a name="tagStorm"></a>
						<h3>Tag Storm</h3>
						<p class="lead">Metadata made simple</p>
						<p>Tag Storms can utilize a <a href="https://github.com/ucscGenomeBrowser/kent/tree/master/src/tagStorm" target="_blank">suite of utilities</a>.</p>


						<a name="SCIMITAR"></a>
						<h3>SCIMITAR</h3>
						<p class="lead">Single Cell Inference of MorphIng Trajectories and their Associated Regulation</p>
						<p><i class="fa fa-file-pdf-o" aria-hidden="true"></i> <a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5203771/" target="_blank">https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5203771/</a></p>
						<p><a href="https://github.com/dimenwarper/scimitar" target="_blank">SCIMITAR</a> can help scientists infer cell trajectory.</p>


						<a name="RIGGLE"></a>
						<h3>RIGGLE</h3>
						<p class="lead">Regulator Inference by Graph-Guided LASSO Estimation</p>
						<p></p>

						<a name="MISCE"></a>
						<h3>MISCE</h3>
						<p class="lead">A Minimum Information About a Stem Cell Experiment</p>
						<p></p>



					<?php } elseif ($_GET['page'] == 'atlas') { ?>
						<p class="lead">Single-cell brain analysis using RNA-seq reveals cellular <abbr title="A state of being diverse in content">heterogeneity</abbr> - a community of cells that form the working brain</p>
						<div class="panel-group" id="accordion">
								<div class="panel panel-default">
										<div class="panel-heading">
												<h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#V4">Brain of Cells V4</a></h4>
										</div>
										<div id="V4" class="panel-collapse collapse in">
												<div class="panel-body">
													.
												</div>
										</div>
								</div>
								<div class="panel panel-default">
										<div class="panel-heading">
											<h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#V3">Brain of Cells V3</a></h4>
										</div>
										<div id="V3" class="panel-collapse collapse">
											<div class="panel-body">
												.
											</div>
										</div>
								</div>
								<div class="panel panel-default">
										<div class="panel-heading">
											<h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#V2">Brain of Cells V2</a></h4>
										</div>
										<div id="V2" class="panel-collapse collapse">
											<div class="panel-body">
												.
											</div>
										</div>
								</div>
								<div class="panel panel-default">
										<div class="panel-heading">
											<h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#V1">Brain of Cells V1</a></h4>
										</div>
										<div id="V1" class="panel-collapse collapse">
											<div class="panel-body">
												.
											</div>
										</div>
								</div>
						</div>
				<?php } elseif ($_GET['page'] == 'footer' && $_GET['title'] == 'Help') { ?>
					Help
				<?php } elseif ($_GET['page'] == 'footer' && $_GET['title'] == 'FAQ') { ?>
					FAQ
				<?php } else { ?>

				<?php } ?>

				</div>
				<div class="col-sm-3 infoboxpadding">

					<?php if ($_GET['page'] == 'welcome') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Other CESCG resources</h3>
							</div>
							<div class="panel-body">
								The <a href="https://www.cirm.ca.gov/researchers/genomics-initiative/about" target="_blank" class="standsout">CESCG <i class="fa fa-external-link" aria-hidden="true"></i></a> is comprised of Operational Cores at <a href="http://med.stanford.edu/cescg.html" target="_blank" class="standsout">Stanford University <i class="fa fa-external-link" aria-hidden="true"></i></a> and <a href="http://www.salk.edu/science/research-centers/center-of-excellence-in-stem-cell-genomics/" target="_blank" class="standsout">the Salk Institute <i class="fa fa-external-link" aria-hidden="true"></i></a> 
							</div>
						</div>
					<?php } elseif ($_GET['page'] == 'cescg') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Side panel</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>
					<?php } elseif ($_GET['page'] == 'access') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Why do I need to request access to certain files?</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>
					<?php } elseif ($_GET['page'] == 'files') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Why are some files not public?</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>
					<?php } elseif ($_GET['page'] == 'projectss') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Informatics and neurobiology</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Cardiac biology</h3>
							</div>
							<div class="panel-body">
								About 1% of all children are born with congenital heart defects
							</div>
						</div>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Blood stem cells and therapeutics</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Molecular regulators of stem cells</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>

					<?php } elseif ($_GET['page'] == 'team') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Role of the DCM</h3>
							</div>
							<div class="panel-body">
								
							</div>
						</div>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Role of the DCG</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>

					<?php } elseif ($_GET['page'] == 'atlas') { ?>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Why use RNA-seq?</h3>
							</div>
							<div class="panel-body">
								RNA-seq is an <abbr title="An assay is an investigative (analytic) procedure to qualitatively assess or quantitatively measure the presence, amount, or functional activity of a target entity (the analyte).">assay</abbr> scientists use to measure how often genes are being expressed, or turned off and on. With few exceptions, all cells in your body contain the same genome, yet are able to become the large variety of cells we see by changing how genes are expressed, leading to the community of cells we see.
							</div>
						</div>
						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Tools and scoreboard</h3>
							</div>
							<div class="panel-body">
								.
							</div>
						</div>
					<?php } else { ?>

					<?php } ?>

				</div>
			</div>
			<div class="row">
				<footer class="footer" style="background: #eeeeee; padding-top: 10px; margin-top: 20px;">

					<div class="container" style="width: 100%">
					<div class="col-md-2">
						<strong>Main</strong>
						<p class="text-muted">
							<a href="?">Home</a><br />
							<a href="?page=cescg">About</a><br />
							<a href="?page=access">Site Access</a><br />
							<a href="?page=files">Files</a><br />
							<a href="?page=projects">Projects</a><br />
						</p>
						<strong>Team</strong>
						<p class="text-muted">
							<a href="http://cirm.ucsc.edu/?page=team#CESCG">CESCG</a><br>
							<a href="http://cirm.ucsc.edu/?page=team#DCM">Data Management</a><br>
							<a href="http://cirm.ucsc.edu/?page=team#DCG">Data Curation Group</a>
						</p>
					</div>
					<div class="col-md-2">
						<strong>Tools</strong>
						<p class="text-muted">
							<!--
							<a href="?page=footer&title=Query">Query</a><br />
							<a href="?page=footer&title=Tags">Tags</a><br />
							<a href="?page=footer&title=Tracks">Tracks</a><br />
							<a href="?page=footer&title=Formats">Formats</a><br />
							<a href="?page=footer&title=Labs">Labs</a><br />
							<a href="?page=footer&title=Cell Browser">Cell Browser</a><br />
							-->
							<a href="https://github.com/ucscGenomeBrowser/kent/tree/master/src/tagStorm" target="_blank">Tag Storm Utilities <i class="fa fa-external-link" aria-hidden="true"></i></a><br />
							<a href="?page=projects#SCIMITAR">SCIMITAR</a><br>
							<a href="?page=projects#RIGGLE">RIGGLE</a><br>
							<a href="?page=projects#MISCE">MISCE</a><br>
						</p>
					</div>
					<div class="col-md-2">
						<strong>Documentation</strong>
						<p class="text-muted">
							<a href="?page=footer&title=FAQ">FAQ</a><br />
							<a href="http://cirm.ucsc.edu/cgi-bin/cdwWebBrowse?cdwCommand=help">Help</a><br />
							<!-- <a href="?page=footer&title=Help">Help</a><br /> -->
							<!-- 
							<a href="?page=footer&title=Sitemap">Sitemap</a><br />
							<a href="?page=footer&title=Privacy">Privacy</a><br /> 
							-->
						</p>
					</div>
					<div class="col-md-2" style="align: right">
						<strong>Contact</strong>
						<p class="text-muted">
							<a href="https://www.cirm.ca.gov/about-cirm/contact-us" target="_blank">CIRM Contact <i class="fa fa-external-link" aria-hidden="true"></i></a><br />
							<a href="https://www.cirm.ca.gov/researchers/genomics-initiative/about" target="_blank">CIRM Genomics <i class="fa fa-external-link" aria-hidden="true"></i></a><br />
							<!-- <a href="?page=footer&title=Social media">CIRM Social media</a><br /> -->
							<a href="?page=access">Contact CESCG DCM</a><br />
							</p>
					</div>
					<div class="col-md-4 text-right" >
						<a href="https://ucscgenomics.soe.ucsc.edu/" target="_blank"><img src="https://tumormap.ucsc.edu/ucscgi_clear.png" class="" style="width: 300px; height: 59px; margin-top: 5px"></a>
					</div>
					</div>
				</footer>
			</div>
			<div class="row" style="padding-right: 15px; padding-top: 25px; " class="text-right pull-right">
				<div class="row">
					<span class="pull-right">
						<a href="https://www.facebook.com/CaliforniaInstituteForRegenerativeMedicine" target="_blank"><i class="fa fa-facebook-square fa-2x"></i></a>
						&nbsp;&nbsp;<a href="https://www.instagram.com/cirm_stemcells/" target="_blank"><i class="fa fa-instagram fa-2x"></i></a>
						&nbsp;&nbsp;<a href="http://www.twitter.com/CIRMnews" target="_blank"><i class="fa fa-twitter-square fa-2x"></i></a>
						&nbsp;&nbsp;<a href="https://www.youtube.com/user/CIRMTV" target="_blank"><i class="fa fa-youtube fa-2x"></i></a>
						&nbsp;&nbsp;<a href="https://plus.google.com/u/0/112512721505484176043/posts" target="_blank"><i class="fa fa-google-plus-square fa-2x"></i></a>
						&nbsp;&nbsp;<a href="https://www.linkedin.com/groups?gid=4017737&trk=myg_ugrp_ovr" target="_blank"><i class="fa fa-linkedin-square fa-2x"></i></a>
						&nbsp;&nbsp;<a href="https://www.flickr.com/photos/cirm" target="_blank"><i class="fa fa-flickr fa-2x"></i></a>
						&nbsp;&nbsp;<a href="https://blog.cirm.ca.gov/" target="_blank"><i class="fa fa-wordpress fa-2x"></i></a>
						&nbsp;&nbsp;<a href="http://cirm.ca.gov" target="_blank"><img src="https://www.cirm.ca.gov/sites/default/files/images/about_cirm/CIRM_Logo_1300px.png" style="width: 59px; height: 25px; margin-top: -10px;" /></a>
					</span>
				</div>
				<div class="row">
					<span class="pull-right" style="padding-top: 20px; padding-bottom: 10px;">

					</span>
				</div>
			</div>
		</div>


	</body>

<!-- Google Analytics?  -->

</html>
