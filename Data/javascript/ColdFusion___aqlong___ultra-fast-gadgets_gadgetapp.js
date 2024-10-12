var app = angular.module('gadgetapp', ['ngAnimate', 'ngSanitize', 'mgcrea.ngStrap']);

// in reality, we'd separate controllers, services, models, and views into separate files as needed
app.controller('MainCtrl', function($scope) {
});

'use strict';

app.service('callCfc', ['$http', '$httpParamSerializerJQLike' , function( $http, $httpParamSerializerJQLike ){

    return {

        baseurl : '',
        call : function( path, method, data, httpMethod ){ 

            var baseurl = this.baseurl;
            var data = data || {};
            var httpMethod = httpMethod || 'GET';

            var httpArgs = {};
            httpArgs.method = httpMethod;
            httpArgs.url = baseurl + path + '?method=' + method;

            if (httpMethod === 'GET') {
                httpArgs.params = data;
            } else {
                // for POST, you need to transform the data, and use the correct form encoding
                httpArgs.data = $httpParamSerializerJQLike(data);
                httpArgs.headers = {'Content-Type': 'application/x-www-form-urlencoded'};
            }

            return 	$http(httpArgs)
                    .then( function( response ){
                        //console.log( 'angular http then call', response );
                        return response.data;
                    }).catch( function(e){
                        // deal with error elegantly
                        //console.log( 'angular http catch', e );
                        return { success: false, error: 'Status: '+e.status+', Status Text: '+e.statusText };
                    });

        }
    };

}]);

app.controller('TypeaheadCtrl', function($scope, $templateCache, callCfc) {

    $scope.selectedGadget = '';

    getGadgets = function(){
        return callCfc.call( 'components/gadgetsRemote.cfc', 'getAll' );
    };

    // depending on the UI requirements, we could also capture various types of errors and exceptions and 
    // return growl notifications with http://janstevens.github.io/angular-growl-2/ and
    // could report them with something like https://www.bugsnag.com/ 
    getGadgets().then( function( data ) {
        $scope.gadgets = data;
    });

});