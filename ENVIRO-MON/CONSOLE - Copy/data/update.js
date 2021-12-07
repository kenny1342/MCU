function formatSecs(secs=0) {
    
  dateObj = new Date(secs * 1000); 
  days = Math.round(secs/86400);
  hours = dateObj.getUTCHours(); 
  minutes = dateObj.getUTCMinutes(); 
  seconds = dateObj.getSeconds(); 

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

//<script>window.jQuery || document.write('<script src="http://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"><\/script>')</script> 
//<script>window.jQuery || document.write('<script src="jquery.min.js"><\/script>')</script>


jQuery(document).ready(function () {  


  
  $('#upload_form').submit(function(e){
    e.preventDefault();


    // Firmware Update
    let form = $('#upload_form')[0];
    let data = new FormData(form);

    if(!$("#upload_form input[type=file]").val()) {
      alert('You must select a file!');
      return false;
    }

    $.ajax({
      url: '/update',
      type: 'POST',
      data: data,
      contentType: false,
      processData:false,
      xhr: function() {
      var xhr = new window.XMLHttpRequest();
      xhr.upload.addEventListener('progress', function(evt) {
        if (evt.lengthComputable) {
          var per = evt.loaded / evt.total;
          $('#prg').html('Progress: ' + Math.round(per*100) + '%');
        }
      }, false);
        return xhr;
      },
      success:function(d, s) {
        console.log('success!');
        $('#success').show();
        setTimeout(function(){ 
          // TODO: check if responding or wait another few secs
          window.location.href = "/";
        }, 40000);
      },
        error: function (a, b, c) {
            $('#error').show();
            console.log(a);
            console.log(b);
            console.log(c);
        }
    });
  });




  $('#config_upload_form').submit(function(e){
    e.preventDefault();


    // Firmware Update
    let form = $('#config_upload_form')[0];
    let data = new FormData(form);

    if(!$("#config_upload_form input[type=file]").val()) {
      alert('You must select a file!');
      return false;
    }

    $.ajax({
      url: '/upload',
      type: 'POST',
      data: data,
      contentType: false,
      processData:false,
      xhr: function() {
      var xhr = new window.XMLHttpRequest();
      xhr.upload.addEventListener('progress', function(evt) {
        if (evt.lengthComputable) {
          var per = evt.loaded / evt.total;
          $('#prg').html('Progress: ' + Math.round(per*100) + '%');
        }
      }, false);
        return xhr;
      },
      success:function(d, s) {
        console.log('success!');
        $('#success').show();
        setTimeout(function(){ 
          // TODO: check if responding or wait another few secs
          window.location.href = "/";
        }, 40000);
      },
        error: function (a, b, c) {
            $('#error').show();
            console.log(a);
            console.log(b);
            console.log(c);
        }
    });
  });



}); // page loaded