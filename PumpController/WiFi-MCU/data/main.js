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
  
$('form').submit(function(e){
    e.preventDefault();
    var form = $('#upload_form')[0];
    var data = new FormData(form);
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
          $('#prg').html('progress: ' + Math.round(per*100) + '%');
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
        }, 20000);
      },
        error: function (a, b, c) {
            $('#error').show();
            console.log(a);
            console.log(b);
            console.log(c);
        }
    });
  });

