// ================================================================================================
// Main Movie Site JS
// ================================================================================================

$(document).ready(function() {
  'use strict';

  var today = new Date();
  var year = today.getFullYear();

  $('#crYear').attr('datetime', year).html(year);
  
  movieApp.init();
});
