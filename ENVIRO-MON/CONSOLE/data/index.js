
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

  function getConfig() {
    let testjson = '{"array":[  1,  2,  3],"boolean":true,"hostname":"console-01","port":80,"ntpserver":"192.168.30.1","devid_sensor1": "50406", "devid_sensor2": "33832","devid_sensor3": "22664","devid_hub": "2233","version":"1.2"  }';
    console.log("Loading config.json");
    
    //json = JSON.parse(testjson);
    //localStorage.setItem('config', JSON.stringify(json));

    fetch('/config.json?nocache=' + Math.random()) // Load configuration
    .then(response => response.json())
    .then(data => {            
        localStorage.setItem('config', JSON.stringify(data));
        console.log("/config.json loaded OK");
        } // data =>
    )
    .catch(error => {
        const timeTaken= (new Date())-start;
        localStorage.setItem('loadtime', timeTaken);    
        console.error('Error:', error);
        alert("USING TEST DATA!\n"+error)
        $("#chktestdata").prop('checked', true);
        json = JSON.parse(testjson);
        localStorage.setItem('config', JSON.stringify(json));
      }
    ); // fetch
  }

  // global
  //var config = JSON.parse(localStorage.getItem('config')); 
  //console.log(config);



  function updateData_Pri3() {
    const start = new Date();
    let config = JSON.parse(localStorage.getItem('config')); 

    fetch('/json/0x45?nocache='+Math.random()) //REMOTE_SENSOR_DATA
    .then(response => response.json())
    .then(data => {
      console.log('updateData_Pri3 fetch 0x45:'); console.log(data);
        const timeTaken= (new Date())-start;
        localStorage.setItem('loadtime', timeTaken);

        localStorage.setItem('json0x45', JSON.stringify(data));
        } // data =>
    )
    .catch(error => {
        console.log('Error:'); console.log(error);
        if($("#chktestdata").is(":checked")){
          console.log("using test data json0x45");
          localStorage.setItem('json0x45', '[{"cmd":69,"devid":50406,"sid":0,"data":{"firmware":"4.51","IP":"192.168.88.95","port":2323,"uptime_sec":1751,"rssi":-48},"ts":14429},{"cmd":69,"devid":50406,"sid":1,"data":{"value":19.7},"ts":14431},{"cmd":69,"devid":50406,"sid":2,"data":{"value":76},"ts":14426},{"cmd":69,"devid":33832,"sid":1,"data":{"value":25.7},"ts":14429},{"cmd":69,"devid":33832,"sid":2,"data":{"value":67.4},"ts":14431},{"cmd":69,"devid":33832,"sid":0,"data":{"firmware":"4.51","IP":"192.168.88.96","port":2323,"uptime_sec":1933,"rssi":-33},"ts":14428}]');
        } else {
          console.log("ERR: 0x45 EMPTY and NOT using test data");
          localStorage.setItem('json0x45', JSON.stringify("{}"));
        }
      }
    ); // fetch


  }

    // Update the page
    setInterval(function ( ) {
            
      let json;
      $("#loadtime").empty().append(localStorage.getItem('loadtime') + 'ms');


      // -------------- REMOTE SENSORS ---------------------------------
      try {
        let config = JSON.parse(localStorage.getItem('config'));   
        json = JSON.parse(localStorage.getItem('json0x45'));
        console.log("json0x45:"); console.log(json);
        
        if(!typeof json == "object" || Object.keys(json).length < 1) {
          console.log("0x45 invalid data, len=" + Object.keys(json).length);
          return;
        }

        console.log("CONFIG:"); console.log(config);

        for(var key in json) {          
          let devid = null; //json[key].devid;           
          let sid = json[key].sid; 
          let value = null; 
          let age = Math.floor(Date.now() / 1000) - parseInt(json[key].ts, 10);
          // TODO, figure out better TZ handling, or flag it as expired in Frontend http_print 0x45 loop instead
          age += 3600; // DIRTY TZ HACK for now..Frontend returns GMT.


          // if we have a static mapping devid(num) <-> name in config.json we prefer that name to be used in html
          // this way we only have to update json.conf if we replace probe/change devid
          switch(String(json[key].devid)) {            
            case config.devid_sensor1: devid = "sensor1"; $("#alias_sensor1").empty().append(config.alias_sensor1); break;
            case config.devid_sensor2: devid = "sensor2"; $("#alias_sensor2").empty().append(config.alias_sensor2); break;
            case config.devid_sensor3: devid = "sensor3"; $("#alias_sensor3").empty().append(config.alias_sensor3); break;
            case config.devid_sensor4: devid = "sensor4"; $("#alias_sensor4").empty().append(config.alias_sensor4); break;
            case config.devid_hub: devid = "hub"; alias = 'HUB'; break;
            default: devid = json[key].devid;
          }

          //console.log("1: SID: " + sid + ", VALUE=" + value);
          let objid = "devid_" + devid + "_sid_" + sid;
          let objids = {};
          if(sid == 0) {
            if(typeof json[key].data.firmware != "undefined") { objids[objid + '_firmware'] = json[key].data.firmware; } 
            if(typeof json[key].data.uptime_sec != "undefined") { objids[objid + '_uptime_sec'] = formatSecs( json[key].data.uptime_sec ); }
            if(typeof json[key].data.rssi != "undefined") { 
              let rssi = json[key].data.rssi;
              //console.log(objid + " RSSI: "+rssi);
              objids[objid + '_rssi'] = rssi; 
              if(rssi >= -50) { // -50 as good as it gets
                objids[objid + '_rssi_class'] = 'good'; 
                objids[objid + '_rssi_level'] = 'five-bars';
              } else if(rssi > -60) {
                objids[objid + '_rssi_class'] = 'good'; 
                objids[objid + '_rssi_level'] = 'four-bars';
              } else if(rssi > -70) {
                objids[objid + '_rssi_class'] = 'ok'; 
                objids[objid + '_rssi_level'] = 'three-bars';
              } else if(rssi > -80) {
                objids[objid + '_rssi_class'] = 'ok'; 
                objids[objid + '_rssi_level'] = 'two-bars';
              } else if(rssi > -90) {
                objids[objid + '_rssi_class'] = 'bad'; 
                objids[objid + '_rssi_level'] = 'one-bar';
              } else if(rssi >= -100) {
                objids[objid + '_rssi_class'] = 'bad'; 
                objids[objid + '_rssi_level'] = '0';
              }
              //console.log("#wifi_" + devid + " class=" + objids[objid + '_rssi_level'] + " " + objids[objid + '_rssi_level']);  
              $("#wifi_" + devid).removeClass();
              //$("#wifi_" + devid).addClass('signal-bars').addClass('mt1').addClass('sizing-box');
              $("#wifi_" + devid).addClass('signal-bars').addClass('sizing-box');
              $("#wifi_" + devid).addClass('' + objids[objid + '_rssi_class']).addClass('' + objids[objid + '_rssi_level']);
              
            }

          } else if(sid > 0 && sid < 9) {
            let prefix = '';
            if(sid == 3) prefix = ', '; // divider between temperatures when multiple sensors/ds18b20 is used
            if(typeof json[key].data["value"] != "undefined") { objids[objid + ''] = prefix + json[key].data["value"]; }
            if(objid == 'devid_sensor2_sid_3') {
              // also add value to heater radiator row
              $("#devid_sensor2_sid_3x").empty().append( json[key].data["value"] );

            }
          }
          //console.log("OBJIDS: ");  
          //console.log(objids);  

          

          $.each( objids, function( objid, value ) {
            //console.log(" OBJID: " + objid + ", VALUE=" + value);
            if($("#" + objid).length > 0) {
                        
              if(value != $("#" + objid).html()) {
                $("#" + objid).empty().append(value);
  
                if(!$("#" + objid).is(':animated') )  {
                  $("#" + objid).stop(true, true).fadeOut(500).fadeIn(800)
                } 
              }
            }
          });
          //console.log("OBJID: " + objid);
            
            if(devid == "sensor1") { // outside
              if(value < 0.0) {
                $("#" + objid).removeClass().addClass("temp_negative");
              } else {
                $("#" + objid).removeClass("temp_negative");
              }
            }

            if(age > 600) {
              $("#" + objid).removeClass().addClass("OLD_DATA");
            } else {
              $("#" + objid).removeClass("OLD_DATA");
            }
          

        }

      } catch(e) {
        
        console.log(json);
        let str="ox45 Failed to parse JSON, len=" + Object.keys(json).length + " EX="+e;
        console.log(str);
        //return;
      }
      
    }, 900 ) ;

    setInterval(getConfig, 1000*60 ) ;
    setInterval(updateData_Pri3, 2000 ) ;
  
    $("#table_pump_expand").click(function(){

      $('.tr_minimized').each(function(i, obj) {
        if("a certain condition") {
          $(this).toggleClass('tr_maximized'); // trying to access the index 0 of the array that contains the elements of that class;
         }
      });

  });

  json = JSON.parse('{"cmd":99,"devid":50406,"sid":2,"data":{"value":30.4}}');
  console.log("DEBUG");
  console.log(json);
  }); // page loaded
