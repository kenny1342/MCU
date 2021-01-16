
function formatSecs(input=0) {
    
  let days = Math.floor(input / 86400);
  let hours = Math.floor((input % 86400 ) / 3600) ;
  let minutes = Math.floor(((input % 86400 ) % 3600 ) / 60 );
  let seconds = ((input % 86400 ) % 3600 ) % 60  ;
  let timeString = "";

  if(days > 0) {
    timeString = days.toString() + ' days, ';
  }
    
  timeString += hours.toString().padStart(2, '0') 
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
            json = JSON.parse('{"cmd":16,"devid":16,"firmware":"2.21","uptimesecs":18,"freemem":3211,"alarms":[],"lastAlarm":"-"}');
            console.log(json);
            localStorage.setItem('json0x10', JSON.stringify(json));
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
            json = JSON.parse('{"cmd":17,"devid":16,"emon_freq":50.2008,"emon_vrms_L_N":239.2254,"emon_vrms_L_PE":135.3039,"emon_vrms_N_PE":123.7783,"circuits":{"1":{"I":19.02676,"P_a":4551.683,"PF":0.18295},"2":{"I":15.572593,"P_a":376.0423,"PF":0},"3":{"I":0.753701,"P_a":180.2182,"PF":0},"13":{"I":6.06794,"P_a":1451.714,"PF":0.072817}}}');
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
            json = JSON.parse('{"cmd":18,"devid":16,"temp_c":22.9,"temp_motor_c":30,"hum_room_pct":31.9,"pressure_bar":3.722656,"WP":{"status":"RUN","t_state":145,"susp_r":0,"cnt_starts":1,"cnt_susp":0,"t_susp":0,"t_susp_tot":0,"t_totruntime":145,"t_press_st":3,"press_st":0}}');
            //console.log(json);
            localStorage.setItem('json0x12', JSON.stringify(json));
            //localStorage.setItem('json0x12', JSON.stringify("{}"));
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

    // Get Frontend data from JSON
    setInterval(function ( ) {

      fetch('/json/frontend')
      .then(response => response.json())
      .then(data => {
          console.log(data);
          localStorage.setItem('jsonsystem', JSON.stringify(data));
          } // data =>
      )
      .catch(error => {
          //console.error('Error:', error);
          json = JSON.parse('{"ESP":{"hostname":"wifi-ctrl-02","heap":204820,"freq":240,"chipid":18500,"uptimesecs":21424}}');
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
        $("#adc_freemem").empty().append( json.freemem );
        $("#lastAlarm").empty().append(json.lastAlarm);
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
        let susp_reasons = { 10:"Low memory", 11:"Sensor O/R", 12:"Sensor ADC zero", 13:"Temp too low", 14:"Runtime exceeded" };
        let susp_r = json.WP.susp_r;
        if(parseInt(susp_r, 10) > 0) {
          if(susp_reasons[susp_r]) {
            $("#wp_susp_r").empty().append("(" + susp_reasons[susp_r]+")");
          } else {
            $("#wp_susp_r").empty().append(susp_r);
          }  
        } else {
          $("#wp_susp_r").empty();
        }
        
        $("#temperature").empty().append(parseFloat(json.temp_c).toFixed(1));
        $("#temperature_motor").empty().append(parseFloat(json.temp_motor_c).toFixed(1));
        $("#hum_room_pct").empty().append(parseFloat(json.hum_room_pct).toFixed(1));
        $("#waterpressure").empty().append(parseFloat(json.pressure_bar).toFixed(2));
        $("#wp_status").empty().append( json.WP.status);
        $("#wp_t_state").empty().append( formatSecs(json.WP.t_state) );
        $("#wp_cnt_starts").empty().append( json.WP.cnt_starts );
        $("#wp_cnt_susp").empty().append( json.WP.cnt_susp );
        $("#wp_t_totruntime").empty().append( formatSecs(json.WP.t_totruntime) );
        $("#wp_t_susp_tot").empty().append( formatSecs(json.WP.t_susp_tot) );

        let wp_status = json.WP.status;
        
        if(wp_status == "SUSPENDED") {
          $("#wp_status").removeClass().addClass("SUSPENDED");
        } else if(wp_status == "RUN") {
          $("#wp_status").removeClass().addClass("RUN");
        } else {
          $("#wp_status").removeClass().addClass("STOP");
        }

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
        let circuit_names = { 1:"K1 MAIN", 2:"K2 Living room", 3:"Kitchen", 13:"Heatpump" };
        let circuit_fuses = { 1:63, 2:16, 3:16, 13:16 };
        

        if(Object.keys(json.circuits).length > 0) {
          $("#table_emon_circuits tbody").empty();
          for(var id in json.circuits) {
            let name = circuit_names[id] ? circuit_names[id] : id + ' (UNKNOWN)';
            let Ifuse = circuit_fuses[id] ? circuit_fuses[id] : 0;
            let I = json.circuits[id].I;

            markup ='<tr id="tr_circuit_' + id + '">'+
              '<td id="title_circuit_' + id + '" class="td_title">K' + id + '/' + Ifuse + 'A - ' +name+'</td>' +
              '<td id="I_circuit_' + id + '" class="td_value"><small>I(rms)</small> ' + parseFloat( I ).toFixed(1) + '<sup class="units_xs">A</sup></td>' +
              '<td id="P_a_circuit_' + id + '" class="td_value">' + parseInt(json.circuits[id].P_a,10) + '<sup class="units_xs">W</sup></td>' +
              '<td id="PF_circuit_' + id + '" class="td_value"><small>PF:</small> ' + parseFloat(json.circuits[id].PF).toFixed(2) + '<sup class="units_xs">%</sup></td>' +
            '</tr>"';
            $("#table_emon_circuits tbody").append(markup);
            if( (Ifuse - I) <= 2) {
              $("#title_circuit_" + id).removeClass().addClass("ERR");
            } else {
              $("#title_circuit_" + id).removeClass();
            }
            
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
