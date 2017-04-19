<?php
	if (!isset($_GET['page'])) { $_GET['page'] = 'footer'; $section['footer'] = 'Landing page'; }

	$theme['cescg'] 	= "FF8C00";
	$theme['access'] 	= "4682B4";
	$theme['files'] 	= "C71585";
	$theme['projects'] 	= "1E90FF";
	$theme['atlas'] 	= "5cb85c"; // = "3CB371";
	$theme['footer'] 	= "aaaaaa";

	$section['cescg'] 	= "About the Stem Cell Hub and CESCG (CIRM Genomics Initiative)";
	$section['access']	= "Obtain access to research data";
	$section['files'] 	= "Files";
	$section['projects'] 	= "View our CIRM CESCG Projects";
	$section['atlas'] 	= "Single Cell Brain Atlas and Differentiation Scorecard";
	//$section['footer'] 	= "Other...";

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
		<link href="https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css" rel="stylesheet">

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

			.label-cescg 						{ background-color: #FF8C00; }
			.label-cescg[href]:hover, .label-cescg[href]:focus 	{ background-color: #808080; }

			.label-access						{ background-color: #4682B4; }
			.label-access[href]:hover, .label-access[href]:focus 	{ background-color: #808080; }

			.label-files 						{ background-color: #C71585; }
			.label-files[href]:hover, .label-files[href]:focus 	{ background-color: #808080; }

			.label-projects 					{ background-color: #1E90FF; }
			.label-projects[href]:hover, .label-projects[href]:focus { background-color: #808080; }

			.label-atlas 						{ background-color: #5cb85c; }
			.label-atlas[href]:hover, .label-atlas[href]:focus 	{ background-color: #808080; }

			.a-unstyled { color: inherit; }
			.a-unstyled:link { color: inherit; }
			.a-unstyled:visited { color: inherit; }
			.a-unstyled:hover { color: inherit; text-decoration: none; }
			.a-unstyled:active { color: inherit; text-decoration: none; }

			.login { color: #ffffff; }
			.login:link { color: #ffffff !important; }
			.login:visited { color: #ffffff !important; }
			.login:hover { color: #cccccc; text-decoration: underline !important; }
			.login:active { color: #ffffff; text-decoration: underline !important; }
	
			.nopadding { padding: 0 !important; margin: 0 !important; }

		</style>

	</head>
	<body>
		<div style="width: 100%; background-color: #4682B4; height: 20px;">
			<!--
			<div class="container container-table" style="width: 90%; font-color: #ffffff;">
				<div style="float: right" class="login">
				<a href="">login</a> | <a href="">sign up</a>
				</div>
			</div>-->
		</div>
		<div class="container container-table" style="width: 90%">
			<div class="row">
				<!-- -->
				<div style="float: right; padding: 5px; background: #eeeeee; margin-top: 10px;">
					For researchers: <a href="?page=footer&title=Log in">login</a> | <a href="?page=footer&title=Sign up">sign up</a>
				</div>
				<!-- -->
				<a href="?"><img src="logo5.png" class="img-responsive" style=" margin-top: 15px;" /></a>
				<!--<a href="?"><img src="https://www.cirm.ca.gov/sites/default/files/images/about_cirm/CIRM_Logo_1300px.png" style="width: 200px; height: 84px; margin-top: 15px;" /></a>-->

				<h3>
					<div style="float: right; width: 200px; margin-top: 5px" class="input-group">
						<input type="text" class="form-control" placeholder="Search for...">
						<span class="input-group-btn">
						<button class="btn btn-default" type="button">search</button>
						</span>
					</div>
					<div style="float: right; width: 190px; margin-top: 5px" class="input-group">
						<!-- Split button -->
						<div class="btn-group">
							<button type="button" class="btn btn-success">Single Cell Atlases</button>
							<button type="button" class="btn btn-success dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
								<span class="caret"></span>
								<span class="sr-only">Toggle Dropdown</span>
							</button>
							<ul class="dropdown-menu">
								<li><a href="?page=atlas">Brain of Cells V1</a></li>
								<li><a href="?page=atlas">Brain of Cells V2</a></li>
								<li><a href="?page=atlas">Brain of Cells V3</a></li>
								<li><a href="?page=atlas">Brain of Cells V4</a></li>
								<li role="separator" class="divider"></li>
								<li><a href="?page=atlas">Heart of Cells</a></li>
								<li role="separator" class="divider"></li>
								<li><a href="?page=atlas">Immune System of Cells</a></li>
								<li role="separator" class="divider"></li>
								<li><a href="?page=atlas">All Single Cell Datasets</a></li>
							</ul>
						</div>
					</div>
					<div style="line-height: 1.5em;">
						<a class="a-unstyled" href="?page=cescg"><span class="label label-cescg">CESCG Organization</span></a>
						<a class="a-unstyled" href="?page=access"><span class="label label-access">Research Access</span></a>
						<a class="a-unstyled" href="?page=files"><span class="label label-files">Files</span></a>
						<a class="a-unstyled" href="?page=projects"><span class="label label-projects">Projects</span></a>
						<!--<a class="a-unstyled" href="?page=atlas"><span class="label label-atlas">Single Cell Brain Atlas</span></a> -->

					</div>
				</h3>


				<div style="color: #ffffff; background: #<?php echo $theme[$_GET['page']] ?>; padding-left: 5px; margin-top: 10px; ">
					<h4 style=" padding-top: 5px; padding-bottom: 5px;">
						<?php echo $section[$_GET['page']]; if ($_GET['page'] == 'footer') { echo $_GET['title']; }?>
					</h4>
				</div>
			</div>

			<br />

			<div class="row">
				<div class="col-sm-6 nopadding">
					<p>Welcome to the CIRM Genomics Hub, created from the Genomics Initiative from the California Institute for Regenerative Medicine's Center of Excellence for Stem Cell Genomics (CESCG).</p>
					<p>The CESCG is made up of approximately 20 separate projects, all involving stem or progenitor cells. The first release of information explores single cell expression datasets in the human brain, tools to deconvolute complex mixtures of brain cells into cell types and developmental states, and scorecards for in vitro differentiation into brain subclasses.</p>
					<p class="h3" style="margin-top: -5px">Donec auctor</p>
					<p class="lead">Nunc imperdiet rhoncus finibus. Praesent vel metus porttitor, posuere ligula id, ullamcorper justo. Nullam eget lacus semper, consequat massa quis, faucibus tellus. Vivamus laoreet tortor sit amet nisi pellentesque vestibulum. Donec auctor.</p>
					<p>lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</p>
					<blockquote>
						<p>Ut enim ad minima veniam, quis nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur? Quis autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla pariatur?</p>
						<small>Vivamus laoreet, <cite title="Source Title">Dor sit amet</cite></small>
					</blockquote>
					<p>At vero eos et accusamus et iusto odio dignissimos ducimus qui blanditiis praesentium voluptatum deleniti atque corrupti quos dolores et quas molestias excepturi sint occaecati cupiditate non provident, similique sunt in culpa qui officia deserunt mollitia animi, id est laborum et dolorum fuga. Et harum quidem rerum facilis est et expedita distinctio. Nam libero tempore, cum soluta nobis est eligendi optio cumque nihil impedit quo minus id quod maxime placeat facere possimus, omnis voluptas assumenda est, omnis dolor repellendus. Temporibus autem quibusdam et aut officiis debitis aut rerum necessitatibus saepe eveniet ut et voluptates repudiandae sint et molestiae non recusandae. Itaque earum rerum hic tenetur a sapiente delectus, ut aut reiciendis voluptatibus maiores alias consequatur aut perferendis doloribus <abbr title="Example description, we can explain things to users">asperiores</abbr> repellat.</p>
				</div>
				<div class="col-sm-3">

					<div class="panel panel-default">
						<div class="panel-heading">
							<h3 class="panel-title">CESCG Brain Atlas Labs</h3>
						</div>
						<div class="panel-body">
							Panel content
						</div>
					</div>
					<div class="panel panel-default">
						<div class="panel-heading">
							<h3 class="panel-title">CESCG</h3>
						</div>
						<div class="panel-body">
							Panel content
						</div>
					</div>

				</div>
				<div class="col-sm-3 nopadding">

						<div class="panel panel-default">
							<div class="panel-heading">
								<h3 class="panel-title">Brain atlas files</h3>
							</div>
							<div class="panel-body" style="text-align: left">
								<img src="https://ucscgenomics.soe.ucsc.edu/wp-content/uploads/2016/09/mousebrain.png" class="img-rounded img-responsive" style="width: 50%; float: right; padding-left: 10px;" />
								Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
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
					</div>
					<div class="col-md-2">
						<strong>Tools</strong>
						<p class="text-muted">
							<a href="?page=footer&title=Query">Query</a><br />
							<a href="?page=footer&title=Tags">Tags</a><br />
							<a href="?page=footer&title=Tracks">Tracks</a><br />
							<a href="?page=footer&title=Formats">Formats</a><br />
							<a href="?page=footer&title=Labs">Labs</a><br />
							<a href="?page=footer&title=Cell Browser">Cell Browser</a><br />
							<a href="https://github.com/ucscGenomeBrowser/kent/tree/master/src/tagStorm" target="_blank">Tag Storm Utilities</a><br />
						</p>
					</div>
					<div class="col-md-2">
						<strong>Documentation</strong>
						<p class="text-muted">
							<a href="?page=footer&title=FAQ">FAQ</a><br />
							<a href="?page=footer&title=Help">Help</a><br />
							<a href="?page=footer&title=Sitemap">Sitemap</a><br />
							<a href="?page=footer&title=Privacy">Privacy</a><br />
						</p>
					</div>
					<div class="col-md-2" style="align: right">
						<strong>Contact</strong>
						<p class="text-muted">
							<a href="https://www.cirm.ca.gov/about-cirm/contact-us" target="_blank">CIRM Contact</a><br />
							<a href="https://www.cirm.ca.gov/researchers/genomics-initiative/about" target="_blank">CIRM Genomics</a><br />
							<a href="?page=footer&title=Social media">CIRM Social media</a><br />
							<a href="?page=footer&title=Contact us">Contact CESCG DCM</a><br />
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
