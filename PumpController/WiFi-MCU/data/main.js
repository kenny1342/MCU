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
