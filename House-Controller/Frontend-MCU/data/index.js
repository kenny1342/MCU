function formatSecs(input=0) {
    
  let days = Math.floor(input / 86400);
  let hours = Math.floor((input % 86400 ) / 3600) ;
  let minutes = Math.floor(((input % 86400 ) % 3600 ) / 60 );
  let seconds = ((input % 86400 ) % 3600 ) % 60  ;
  
  timeString = days.toString() + ' days, ' +  
  hours.toString().padStart(2, '0') 
      + ':' + minutes.toString().padStart(2, '0') 
      + ':' + seconds.toString().padStart(2, '0'); 
    
  return timeString;
  }
    
  
  // Make sure JQuery script is loaded. Because of size, we try to include it from public/CDN first. If that fails, we load it locally (slower)
  
  if (typeof jQuery === "undefined") {
    console.log("public cdn jquery not loaded, loading local version")
    document.write('<script src="jquery.js"><\/script>')
  } else {
  
  }

  
jQuery(document).ready(function () {

    // Get data from JSON
    setInterval(function ( ) {

      fetch('/json/0x10') //ADCSYSDATA
      .then(response => response.json())
      .then(data => {
          console.log(data);
          localStorage.setItem('json0x10', JSON.stringify(data));
          } // data =>
      )
      .catch(error => {
          //console.error('Error:', error);
          if($("#chktestdata").is(":checked")){
            localStorage.setItem('json0x10', JSON.stringify("{}"));
          } else {
            //console.log("NOT using test data");
            localStorage.setItem('json0x10', JSON.stringify("{}"));
          }
        }
      ); // fetch


    }, 2500 ) ;


    setInterval(function ( ) {

      fetch('/json/0x11') //ADCEMONDATA
      .then(response => response.json())
      .then(data => {
          console.log(data);
          localStorage.setItem('json0x11', JSON.stringify(data));
          } // data =>
      )
      .catch(error => {
          //console.error('Error:', error);
          if($("#chktestdata").is(":checked")){
            console.log("using test data");
            json = JSON.parse('{"firmware":"2.11","pressure_bar":0.000001,"temp_c":0.00001,"hum_room_pct":0.0001,temp_motor_c:0.0001,"alarms":[],"emon_freq":0.0000,"emon_vrms_L_N":0.0001,"emon_vrms_L_PE":0.0001,"emon_vrms_N_PE":0.001,"WP":{"t_state":0,"is_running":0,"is_suspended":false,"cnt_starts":0,"cnt_susp":0,"t_susp":0,"t_susp_tot":0,"t_totruntime":0},"circuits":{"K2":{"I":0.000001,"P_a":0,"PF":0},"K3":{"I":0.00001,"P_a":0,"PF":0}}}');
            console.log(json);
            localStorage.setItem('json0x11', JSON.stringify(json));
          } else {
            //console.log("NOT using test data");
            localStorage.setItem('json0x11', JSON.stringify("{}"));
          }
        }
      ); // fetch


    }, 2000 ) ;


    setInterval(function ( ) {

      fetch('/json/0x12') //ADCWATERPUMPDATA
      .then(response => response.json())
      .then(data => {
          console.log(data);
          localStorage.setItem('json0x12', JSON.stringify(data));
          } // data =>
      )
      .catch(error => {
          //console.error('Error:', error);
          if($("#chktestdata").is(":checked")){
            //console.log("using test data");
            json = JSON.parse('{"firmware":"2.11","pressure_bar":0.000001,"temp_c":0.00001,"hum_room_pct":0.0001,temp_motor_c:0.0001,"alarms":[],"emon_freq":0.0000,"emon_vrms_L_N":0.0001,"emon_vrms_L_PE":0.0001,"emon_vrms_N_PE":0.001,"WP":{"t_state":0,"is_running":0,"is_suspended":false,"cnt_starts":0,"cnt_susp":0,"t_susp":0,"t_susp_tot":0,"t_totruntime":0},"circuits":{"K2":{"I":0.000001,"P_a":0,"PF":0},"K3":{"I":0.00001,"P_a":0,"PF":0}}}');
            //console.log(json);
            //localStorage.setItem('json0x12', JSON.stringify(json));
            localStorage.setItem('json0x12', JSON.stringify("{}"));
          } else {
            //console.log("NOT using test data");
            localStorage.setItem('json0x12', JSON.stringify("{}"));
          }
        }
      ); // fetch


    }, 2000 ) ;


    // Get remote sensor data from JSON
    setInterval(function ( ) {

      fetch('/json/0x45') //REMOTE_SENSOR_DATA
      .then(response => response.json())
      .then(data => {
          console.log(data);
          localStorage.setItem('json0x45', JSON.stringify(data));
          } // data =>
      )
      .catch(error => {
          //console.error('Error:', error);
          if($("#chktestdata").is(":checked")){
            console.log("using test data");
            json = JSON.parse('{"cmd":69,"id":1,"firmware":"0.00","IP":"192.168.255.255","port":2323,"uptime_sec":18124,"data":[0.0001,0.0001,0.0001],"units":["DEGREES_C","DEGREES_C","PERCENT"],"text":[]}');
            console.log(json);
            localStorage.setItem('json0x45', JSON.stringify(json));
          } else {
            //console.log("NOT using test data");
            localStorage.setItem('json0x45', JSON.stringify("{}"));
          }
        }
      ); // fetch


    }, 3000 ) ;

    // Get system data from JSON
    setInterval(function ( ) {

      fetch('/json/system')
      .then(response => response.json())
      .then(data => {
          console.log(data);
          localStorage.setItem('jsonsystem', JSON.stringify(data));
          } // data =>
      )
      .catch(error => {
          //console.error('Error:', error);
          json = JSON.parse('{"ESP":{"heap":0,"freq":0,"chipid":123456,"uptimesecs":0}}');
          localStorage.setItem('jsonsystem', JSON.stringify(json));
        }
      ); // fetch


    }, 900 ) ;

    // Update the page
    setInterval(function ( ) {

      let json;

      // -------------- ESP/SYSTEM ---------------------------------
      try {
        json = JSON.parse(localStorage.getItem('jsonsystem'));
        if(Object.keys(json).length < 1) {
          console.log("invalid data, len=" + Object.keys(json).length);
          return;
        }
          $("#uptime_sys").empty().append( formatSecs(json.ESP.uptimesecs) );
          $("#heap").empty().append( json.ESP.heap );
          $("#freq").empty().append( json.ESP.freq  + ' Mhz');
          $("#chipid").empty().append( json.ESP.chipid );
      } catch(e) {
          let str="Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
          console.log(str);
          return;
      }


      // -------------- ADCSYS ---------------------------------
      try {
        json = JSON.parse(localStorage.getItem('json0x10')); 
        if(Object.keys(json).length < 1) {
          console.log("invalid data, len=" + Object.keys(json).length);
          return;
        }
        $("#uptime_adc").empty().append( formatSecs(json.uptimesecs) );
        $("#alarms").empty();
        if(Object.keys(json.alarms).length > 0) {
          $("#alarms").append("ALARMS: ");
          for(var key in json.alarms) {
            $("#alarms").append(json.alarms[key] + "&nbsp;");
          }
        }

      } catch(e) {
        let str="Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
        console.log(str);
        return;
      }

      // -------------- WATERPUMP ---------------------------------
      try {
        json = JSON.parse(localStorage.getItem('json0x12')); 
        if(Object.keys(json).length < 1) {
          console.log("invalid data, len=" + Object.keys(json).length);
          return;
        }

        $("#temperature").empty().append(parseFloat(json.temp_c).toFixed(1));
        $("#temperature_motor").empty().append(parseFloat(json.temp_motor_c).toFixed(1));
        $("#hum_room_pct").empty().append(parseFloat(json.hum_room_pct).toFixed(1));
        $("#waterpressure").empty().append(parseFloat(json.pressure_bar).toFixed(2));
        $("#wp_is_running").empty().append( json.WP.is_running ? "ON" : "OFF");
        $("#wp_is_suspended").empty().append( json.WP.is_suspended ? "YES" : "NO");
        $("#wp_t_state").empty().append( formatSecs(json.WP.t_state) );
        $("#wp_cnt_starts").empty().append( json.WP.cnt_starts );
        $("#wp_cnt_susp").empty().append( json.WP.cnt_susp );
        $("#wp_t_totruntime").empty().append( formatSecs(json.WP.t_totruntime) );
        $("#wp_t_susp_tot").empty().append( formatSecs(json.WP.t_susp_tot) );
      } catch(e) {
        let str="Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
        console.log(str);
        return;
      }


      // -------------- EMON ---------------------------------
      try {
        json = JSON.parse(localStorage.getItem('json0x11')); 
        if(Object.keys(json).length < 1) {
          console.log("invalid data, len=" + Object.keys(json).length);
          return;
        }

        $("#emon_freq").empty().append( parseFloat(json.emon_freq).toFixed(1) );
        $("#emon_vrms_L_N").empty().append( parseFloat(json.emon_vrms_L_N).toFixed(1) );
        $("#emon_vrms_L_PE").empty().append( parseFloat(json.emon_vrms_L_PE).toFixed(1) );
        $("#emon_vrms_N_PE").empty().append( parseFloat(json.emon_vrms_N_PE).toFixed(1) );

        let emon_vrms_L_N = json.emon_vrms_L_N;
        // Forsyningskrav i Norge er nominell spenning 230V +/- 10%
        if(emon_vrms_L_N > 253.0 || emon_vrms_L_N < 207.0) {
          $("#emon_vrms_L_N").removeClass().addClass("ERR");
        } else {
          $("#emon_vrms_L_N").removeClass().addClass("OK");
        }

        let emon_freq = json.emon_freq;
        if(Math.abs(50.0 - emon_freq) > 1.1) {
          $("#emon_freq").removeClass().addClass("ERR");
        } else {
          $("#emon_freq").removeClass().addClass("OK");
        }

        let emon_vrms_L_PE = json.emon_vrms_L_PE;
        let emon_vrms_N_PE = json.emon_vrms_N_PE;

        if(Math.abs(emon_vrms_N_PE - emon_vrms_L_PE) > 15.0) {
          $("#emon_vrms_L_PE").removeClass().addClass("ERR");
          $("#emon_vrms_N_PE").removeClass().addClass("ERR");
        } else {
          $("#emon_vrms_L_PE").removeClass().addClass("OK");
          $("#emon_vrms_N_PE").removeClass().addClass("OK");
        }
        
        // dynamically add table rows from json.circuits array
        if(Object.keys(json.circuits).length > 0) {
          $("#table_emon_circuits tbody").empty();
          for(var key in json.circuits) {
            markup ='<tr>'+
              '<td class="td_title">'+key+'</td>' +
              '<td class="td_value"><small>I(rms)</small> ' + parseFloat( json.circuits[key].I ).toFixed(1) + '<sup class="units_xs">A</sup></td>' +
              '<td class="td_value">' + json.circuits[key].P_a + '<sup class="units_xs">W</sup></td>' +
              '<td class="td_value"><small>PF:</small> ' + json.circuits[key].PF + '<sup class="units_xs">%</sup></td>' +
            '</tr>"';
            $("#table_emon_circuits tbody").append(markup);

          }
        }

      } catch(e) {
        let str="Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
        console.log(str);
        return;
      }

      // -------------- REMOTE SENSORS ---------------------------------
      try {
        json = JSON.parse(localStorage.getItem('json0x45'));
        if(Object.keys(json).length < 1) {
          console.log("invalid data, len=" + Object.keys(json).length);
          return;
        }
          $("#fridge_temp").empty().append(parseFloat(json["50406"]["1"].toFixed(1))); // fridge wifi sensor - devid=50406, sid=1
      } catch(e) {
        let str="Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
        console.log(str);
        return;
      }
      
      
    }, 500 ) ;

  }); // page loaded
