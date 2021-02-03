
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


  console.log("Loading config.json");
  fetch('/config.json') // Load configuration
  .then(response => response.json())
  .then(data => {
      //console.log(data);
      localStorage.setItem('config', JSON.stringify(data));
      } // data =>
  )
  .catch(error => {
      console.error('Error:', error);
      alert("USING TEST DATA!\n"+error)
      $("#chktestdata").prop('checked', true);
      json = JSON.parse('{"array":[  1,  2,  3],"boolean":true,"hostname":"wifi-ctrl-02","port":80,"ntpserver":"192.168.30.1","circuits":{  "1":{"name":"Main", "size":63},  "2":{"name":"Living room", "size":16},  "3":{"name":"Kitchen", "size":16},  "5":{"name":"Pump room", "size":16}, "7":{"name":"Aux Water Heater", "size":16}, "13":{"name":"Heat pump", "size":16}},"alarms":{  "lowmem":"ADC Low memory",  "wpruntime":"Max runtime exceeded",  "wpaccair":"Low air pressure accumulator tank",  "wptemproom":"Low temperature pump room",  "wppresssens":"Water pressure sensor fault",  "wppresssens":"Temp sensor fault pump room",  "emon_mains_o_r":"Mains voltage out of range",  "emon_gndfault":"Ground fault detected (voltages out of range)",  "emon_sensor":"Voltage/current sensor error"},"probe_devid_bathroom": "9457","probe_devid_pumphouse": "50406","version":"1.2"  }');
      localStorage.setItem('config', JSON.stringify(json));
    }
  ); // fetch

  // global
  var config = JSON.parse(localStorage.getItem('config')); 
  console.log(config);


  function updateData_Pri1() {

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
          json = JSON.parse('{"cmd":16,"devid":16,"firmware":"2.21","uptimesecs":18,"freemem":3211,"alarms":["wpaccair"],"lastAlarm":"-"}');
          console.log(json);
          localStorage.setItem('json0x10', JSON.stringify(json));
        } else {
          //console.log("NOT using test data");
          localStorage.setItem('json0x10', JSON.stringify("{}"));
        }
      }
    ); // fetch
    

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
          json = JSON.parse('{"cmd":18,"devid":16,"temp_c":22.9,"temp_inlet_c":90,"temp_motor_c":30,"hum_room_pct":31.9,"pressure_bar":3.722656,"WP":{"status":"RUN","t_state":145,"susp_r":0,"cnt_starts":1,"cnt_susp":0,"t_susp":0,"t_susp_tot":0,"t_totruntime":145,"t_press_st":3,"press_st":0}}');
          //console.log(json);
          localStorage.setItem('json0x12', JSON.stringify(json));
          //localStorage.setItem('json0x12', JSON.stringify("{}"));
        } else {
          //console.log("NOT using test data");
          localStorage.setItem('json0x12', JSON.stringify("{}"));
        }
      }
    ); // fetch


  }

  function updateData_Pri2() {

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
          json = JSON.parse('{"cmd":17,"devid":16,"emon_freq":50.2008,"emon_vrms_L_N":239.2254,"emon_vrms_L_PE":135.3039,"emon_vrms_N_PE":123.7783,"circuits":{"1":{"I":19.02676,"P_a":4551.683,"PF":0.18295},"2":{"I":15.572593,"P_a":376.0423,"PF":0},"3":{"I":0.753701,"P_a":180.2182,"PF":0},"7":{"I":5.753701,"P_a":100.2182,"PF":0},"13":{"I":6.06794,"P_a":1451.714,"PF":0.072817}}}');
          console.log(json); 
          localStorage.setItem('json0x11', JSON.stringify(json));
        } else {
          //console.log("NOT using test data");
          localStorage.setItem('json0x11', JSON.stringify("{}"));
        }
      }
    ); // fetch

    // Get Frontend data from JSON
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

  }


  function updateData_Pri3() {

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
          json = JSON.parse('[{"cmd":69,"devid":9457,"sid":1,"data":{"value":50.6}},{"cmd":69,"devid":50406,"sid":1,"data":{"value":22.8}},{"cmd":69,"devid":9457,"sid":2,"data":{"value":99.8}}]');
          console.log(json);
          localStorage.setItem('json0x45', JSON.stringify(json));
        } else {
          //console.log("NOT using test data");
          localStorage.setItem('json0x45', JSON.stringify("{}"));
        }
      }
    ); // fetch


  }

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
        // load static circuit config 
        let alarms_conf = config.alarms;        
        if(Object.keys(json.alarms).length > 0) {
          $("#alarms").append("ALARMS: ");
          for(var key in json.alarms) {
            let alarm_text = alarms_conf[json.alarms[key]] ? alarms_conf[json.alarms[key]] : json.alarms[key];
            $("#alarms").append(alarm_text + "&nbsp;");
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
        $("#temperature_inlet").empty().append(parseFloat(json.temp_inlet_c).toFixed(1));
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
        } else if(wp_status == "ON") {
          $("#wp_status").removeClass().addClass("ON");
          if(!$("#wp_status").is(':animated') )  {
            $("#wp_status").fadeOut(300).fadeIn(500);
          }
        } else {
          $("#wp_status").removeClass().addClass("OFF");
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
        
        // load static circuit config 
        let circuit_conf = config.circuits;        

        //dynamically add table rows from json.circuits array
        if(Object.keys(json.circuits).length > 0) {
          $("#table_emon_circuits tbody").empty();
          for(var id in json.circuits) {
            let name = circuit_conf[id]["name"] ? circuit_conf[id]["name"] : id + ' (Unconfigured)';
            let Ifuse = circuit_conf[id]["size"] ? circuit_conf[id]["size"] : 0;
            let I = json.circuits[id].I;

            if(id < 10) {
              name = "&nbsp;- " + name;
            } else {
              name = "- " + name;
            }

            markup ='<tr id="tr_circuit_' + id + '">'+
              '<td id="title_circuit_' + id + '" class="td_title">K' + id + '/' + Ifuse + 'A ' +name+'</td>' +
              '<td id="I_circuit_' + id + '" style="font-size:1.5em;" class="td_value small">I(rms) ' + parseFloat( I ).toFixed(1) + '<sup class="units_xs">A</sup></td>' +
              '<td id="P_a_circuit_' + id + '" class="td_value">' + parseInt(json.circuits[id].P_a,10) + '<sup class="units_xs">W</sup></td>' +
              '<td id="PF_circuit_' + id + '" style="font-size:1.2em;" class="td_value small">PF: ' + parseFloat(json.circuits[id].PF).toFixed(2) + '</td>' +
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
        
        if(!typeof json == "object" || Object.keys(json).length < 1) {
          console.log("0x45 invalid data, len=" + Object.keys(json).length);
          return;
        }

        let probe_devid_bathroom = config.probe_devid_bathroom;     

        for(var idx in json) {
          let probedata = json[idx];
          let devid = probedata["devid"];
          let sid = probedata["sid"];

          // if we have a static mapping devid(num) <-> name in config.json we prefer that name to be used in html
          // this way we only have to update json.conf if we replace probe/change devid
          if(devid == probe_devid_bathroom) {
            devid = "bathroom";
          }

          let objid = "probe_devid_" + devid + "_sid_" + sid;
          if($("#" + objid).length > 0) {
            $("#" + objid).empty().append(probedata.data["value"]);
          }

        }
          //$("#probe_devid9999_sid1").empty().append(parseFloat(json["50406"]["1"].toFixed(1))); // fridge wifi sensor - devid=50406, sid=1
      } catch(e) {
        let str="ox45 Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
        console.log(str);
        return;
      }
      
      
    }, 900 ) ;


    setInterval(updateData_Pri1, 1000 ) ;
    setInterval(updateData_Pri2, 2000 ) ;
    setInterval(updateData_Pri3, 5000 ) ;
  
  }); // page loaded
