AnnoJ.config = {
	tracks : [
		//Models
		{
			id   : 'knownGene',
			name : 'UCSC Known Gene',
			type : 'ModelsTrack',
			path : 'Annotation models',
                        dataSource : '/data/WHATISTHISDATASOURCE',
                        requestMethod: 'GET',
                        requestParams: {format : 'json-annoj' },
                        //requestParams: {format : 'json-annoj', 'chrom': assembly, 'start': left, 'end': right },
			//data : 'fetchers/models/tair7.php',
                        data : '/data/track/range/hg18/knownGene/chrX',
			height : 80,
			showControls : true
		}
	],
	
	active : [
		'knownGene'
	],
	
	genome    : '/data/genome/hg18?format=json-annoj',
//	genome    : 'fetchers/arabidopsis_thaliana.php',
//	bookmarks : 'fetchers/arabidopsis_thaliana.php',
	bookmarks : null,
	stylesheets : [
		{
			id   : 'css1',
			name : 'Plugins CSS',
			href : 'css/plugins.css',
			active : true
		},{
			id   : 'css2',
			name : 'SALK CSS',
			href : 'css/salk.css',
			active : true
		}		
	],
	location : {
		assembly : 'chrX',
		position : 151073054,
		bases    : 20,
		pixels   : 1
	},
	admin : {
		name  : 'Julian Tonti-Filippini',
		email : 'tontij01@student.uwa.edu.au',
		notes : 'Perth, Western Australia (UTC +8)'
	},
//	citation : "<div style='text-align:center;font-size:12px;'><b>Highly Integrated Single-Base Resolution Maps of the Epigenome in Arabidopsis</b></div><div><img style='width:100%;' src='img/plant.jpg' /></div><div style='text-align:left;font-size:10px;'>We kindly request that use of this resource or data in any publication be cited as follows:<br />Lister, R., O'Malley, RC., Tonti-Filippini, J., Gregory, BD., Berry, CC., Millar, AH., and Ecker, JR. <b>Highly Integrated Single-Base Resolution Maps of the Epigenome in Arabidopsis</b>, Cell (2008), <a href='http://download.cell.com/pdfs/0092-8674/PIIS0092867408004480.pdf'>doi:10.1016/j.cell.2008.03.029</a>.</div><hr />"
	citation : "<div style='font-size:12px;'><b>Highly Integrated Single-Base Resolution Maps of the Epigenome in Arabidopsis</b></div><div><a target='new' href='http://neomorph.salk.edu/index.html'>Browser tutorial</a> | <a href='http://neomorph.salk.edu/epigenome/qanda.html' target='new'>Paper Q&amp;A</a></div><div><img style='width:150px;height:150px;' src='img/plant.jpg' /></div><div style='text-align:left;font-size:10px;'>We kindly request that use of this resource or data in any publication be cited as follows:<br />Lister, R., O'Malley, RC., Tonti-Filippini, J., Gregory, BD., Berry, CC., Millar, AH., and Ecker, JR. <b>Highly Integrated Single-Base Resolution Maps of the Epigenome in Arabidopsis</b>, Cell (2008), <a href='http://download.cell.com/pdfs/0092-8674/PIIS0092867408004480.pdf'>doi:10.1016/j.cell.2008.03.029</a>.</div><hr />"
};
