function zobrazCas() {
	var cas = new Date();
	var h = cas.getHours();
	var m = cas.getMinutes();
	var s = cas.getSeconds();
	var day = cas.getDate();
	var month = cas.getMonth()+1;
	var year = cas.getFullYear();
	m = zkontrolujCas(m);
	s = zkontrolujCas(s);
	month = zkontrolujCas(month);
	document.getElementById('cas').innerHTML = h + ":" + m + ":" + s;
	document.getElementById('rok').innerHTML = day + "." + month + "." + year;
	setTimeout(zobrazCas,500);
}

function zkontrolujCas(i) {
	if (i < 10) { i = "0" + i };
	return i;
}

function zobrazStranku() {
	var a = document.getElementById('rezim').value;
	var b = document.getElementById('udajePripojeni');
	var c = document.getElementById('udajeAP');
	var d = document.getElementById('vyberRezimuZasilani');
	var e = document.getElementById('rezimZasilani').value;
	var f = document.getElementById('proThingspeak');
	var g = document.getElementById('proVlastniServer');
	var i = document.getElementById('proVlastniServer_zobrazeniNaStrance');

	b.style.display = "none";
	c.style.display = "none";
	d.style.display = "none";
	f.style.display = "none";
	g.style.display = "none";
	i.style.display = "none";

	document.getElementById('apikey').required = false;
	document.getElementById('apikey2').required = false;
	document.getElementById('channelID').required = false;
	document.getElementById('channelID2').required = false;
	document.getElementById('url_ip').required = false;
	document.getElementById('teplota_p1').required = false;
	document.getElementById('teplota_p2').required = false;
	document.getElementById('teplota_p3').required = false;
	document.getElementById('vlhkost_p1').required = false;
	document.getElementById('vlhkost_p2').required = false;
	document.getElementById('vlhkost_p3').required = false;


	if(a == 1) {
		b.style.display = "block";
		d.style.display = "block";
		
		if(e == 1) {
			f.style.display = "block";
			document.getElementById('apikey').required = true;
			document.getElementById('apikey2').required = true;
			document.getElementById('channelID').required = true;
			document.getElementById('channelID2').required = true;
		} else if(e == 2) {
			g.style.display = "block";
			i.style.display = "block";
			document.getElementById('url_ip').required = true;
			document.getElementById('teplota_p1').required = true;
			document.getElementById('teplota_p2').required = true;
			document.getElementById('teplota_p3').required = true;
			document.getElementById('vlhkost_p1').required = true;
			document.getElementById('vlhkost_p2').required = true;
			document.getElementById('vlhkost_p3').required = true;	
		} 
		
	} else {
		c.style.display = "block";
		i.style.display = "block";
		document.getElementById('teplota_p1').required = true;
		document.getElementById('teplota_p2').required = true;
		document.getElementById('teplota_p3').required = true;
		document.getElementById('vlhkost_p1').required = true;
		document.getElementById('vlhkost_p2').required = true;
		document.getElementById('vlhkost_p3').required = true;
	}
}



