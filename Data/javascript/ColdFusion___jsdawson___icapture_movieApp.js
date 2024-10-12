// ================================================================================================
// Movie Module
// ================================================================================================
var movieApp = movieApp || (function(window, $) {

  'use strict';

  // ----------------------------------------------------------------------------------------------
  //   Private Members
  // ----------------------------------------------------------------------------------------------
  var _apiBaseUrl = 'http://candidate-test.icapture.com/dawsonj/api.cfc';
  var _currentPage = 1;
  var _sortCol = 'vote_count';
  var _sortOrder = 'DESC';

  var _movieListTemplate = ' \
    <% _.each(movies, function(movie, key, list) { %> \
      <tr> \
        <td data-col="title"> \
          <a href="#" data-id="<%= movie[0] %>"><%= movie[1] %></a> \
        </td> \
        <td data-col="release_date"><%= movie[2] %></td> \
        <td data-col="vote_count"><%= movie[3] %></td> \
      </tr> \
    <% }); %>';

  var _movieModalTemplate = ' \
    <% _.each(movie, function(columns, key, list) { %> \
      <div id="movie-modal" class="modal fade" tabindex="-1" role="dialog" aria-hidden="true"> \
        <div class="modal-dialog modal-lg"> \
          <div class="modal-header"> \
            <button type="button" class="close" data-dismiss="modal" aria-hidden="true">×</button> \
            <h3><%= columns[10] %></h3> \
          </div> \
          <div class="modal-body"> \
            <div class="left-col"> \
              <img src="http://image.tmdb.org/t/p/w185<%= columns[8] %>" alt="Movie poster"> \
            </div> \
            <div class="right-col"> \
              <p><%= columns[6] %></p> \
              <p><strong>Released:</strong> <%= columns[9] %></p> \
              <p><strong>Original Title:</strong> <%= columns[5] %></p> \
              <p><strong>Original Language:</strong> <%= columns[4] %></p> \
              <p><strong>Popularity:</strong> <%= columns[7] %></p> \
              <p><strong>Votes:</strong> <%= columns[13] %></p> \
              <p><strong>Average Vote:</strong> <%= columns[12] %></p> \
              <p> \
                <strong>Video:</strong> \
                <span class="initial-caps"><%= columns[11] %></span> \
              </p> \
              <p> \
                <strong>Adult:</strong> \
                <span class="initial-caps"><%= columns[1] %></span> \
              </p> \
            </div> \
          </div> \
          <div class="modal-footer"> \
            <button class="btn" data-dismiss="modal" aria-hidden="true">Close</button> \
          </div> \
        </div> \
      </div> \
    <% }); %>';

    var _paginationTemplate = ' \
    <strong>Page:</strong> \
    <% _.each(rowCount, function(columns, key, list) { %> \
      <ul> \
        <% var rowsPerPage = 20; %> \
        <% var totalRows = columns[0]; %> \
        <% var page = 1; %> \
        <% for (var x = 0;  x < totalRows; x += rowsPerPage) { %> \
          <li><a href="#" data-page="<%= page %>"><%= page %></a></li> \
          <% page++; %> \
        <% } %> \
      </ul> \
    <% }); %>';

  function _bindEvents() {
    $('thead').on('click', 'th a', function() {
      var $link = $(this);
      var column = $link.data('col');

      _sortTable(column);

      _updateSortHeaders();
    });

    $('tbody').on('click', 'td a', function() {
      var $link = $(this);
      var id = $link.data('id');

      _getMovie(id);
    });

    $('#pagination').on('click', 'a', function() {
      var $link = $(this);
      var page = $link.data('page');

      _currentPage = page;

      _getMovies(_currentPage, _sortCol, _sortOrder);

      _updatePagination();
    });
  }

  function _getMovies(page, sortCol, sortOrder) {
    $.ajax({
      url : _apiBaseUrl,
      type: 'get',
      data: { 
        method : 'getMovies',
        page   : page,
        sortCol: sortCol,
        sort   : sortOrder
      },
      success: function(data, status) {
        var movies = JSON.parse(data);

        _populateTable(movies.DATA, $('#movies tbody'));
      },
      error: function(xhr, err) {
        console.log('Error in _getMovies: ' + err);
      }
    });
  }

  function _getMovie(id) {
    $.ajax({
      url : _apiBaseUrl,
      type: 'get',
      data: { 
        method : 'getMovie',
        id     : id
      },
      success: function(data, status) {
        var movie = JSON.parse(data);

        _showMovieModal(movie.DATA);
      },
      error: function(xhr, err) {
        console.log('Error in _getMovie: ' + err);
      }
    });
  }

  function _getRowCount() {
    $.ajax({
      url : _apiBaseUrl,
      type: 'get',
      data: { 
        method : 'getRowCount'
      },
      success: function(data, status) {
        var rowCount = JSON.parse(data);

        _showPagination(rowCount.DATA);
      },
      error: function(xhr, err) {
        console.log('Error in _getRowCount: ' + err);
      }
    });
  }

  function _populateTable(movies, $target) { 
    $target.html(_.template(_movieListTemplate, {variable: 'movies'})(movies));
  }

  function _sortTable(sortCol) {
    var ascending = 'ASC';
    var descending = 'DESC';
    var prevSortCol = _sortCol;
    var prevSortOrder = _sortOrder;

    if (sortCol === prevSortCol) {
      _sortOrder = (prevSortOrder === ascending) ? descending : ascending;
    } else {
      _sortOrder = ascending;
    }

    _sortCol = sortCol;

    _getMovies(_currentPage, _sortCol, _sortOrder);
  }

  function _showMovieModal(movie) {
    var html = _.template(_movieModalTemplate, {variable: 'movie'})(movie);

    $('#movie-modal').remove();

    $(html).appendTo('.content');

    $('#movie-modal').modal('show'); 
  }

  function _showPagination(rowCount) {
    var html = _.template(_paginationTemplate, {variable: 'rowCount'})(rowCount);

    $('#pagination').html('');

    $(html).appendTo('#pagination');

    _updatePagination();
  }

  function _updatePagination() {
    $('#pagination a').removeClass('active');

    $('#pagination li a[data-page="' + _currentPage + '"]').addClass('active');
  }

  function _updateSortHeaders() {
    var sortClass = 'fa-sort-' + _sortOrder.toLowerCase();

    $('#movies thead i.fa-sort').removeClass('fa-sort-asc fa-sort-desc');

    $('#movies thead a[data-col="' + _sortCol + '"]').next('i.fa-sort').addClass(sortClass);
  }

  // ----------------------------------------------------------------------------------------------
  //   Public Members
  // ----------------------------------------------------------------------------------------------
  var init = function() {
    _bindEvents();
    _getRowCount();
    _getMovies(_currentPage, _sortCol, _sortOrder);
    _updateSortHeaders();
  };

  // ----------------------------------------------------------------------------------------------
  //   Public Interface
  // ----------------------------------------------------------------------------------------------
  return {
    init : init
  };
})(window, jQuery);