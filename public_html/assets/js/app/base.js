var app = angular.module('app', []);
app.directive('ngEnter', function() {
        return function(scope, element, attrs) {
            element.bind("keydown keypress", function(event) {
                if(event.which === 13) {
                        scope.$apply(function(){
                                scope.$eval(attrs.ngEnter);
                        });
                        
                        event.preventDefault();
                }
            });
        };
});
app.controller('baseController', ['$scope', '$http', function($scope, $http){
	console.log("welcome to base controller");

	$scope.bodies = [];
	
	$http({
		url: "/loggedInUserInfo",
		method: "GET"
	}).success(function(data, status){
		console.log(data);
		console.log("this is losgin controller");
		if ( data == null || data.loggedIn == false )
			window.location.href = "/login.html";
		else 
			$scope.user = data;
	}).error(function(data, status){
	});
	var getBaseInfo = function(baseid) {
		return $http({
			url: "/baseInfo/" + baseid,
			method: "GET"
		});
	}

	$scope.baseid = $.QueryString['id'];
	getBaseInfo($scope.baseid).success(function(data, status){
		console.log(data);
		$scope.baseInfo = data;
		$http({
			url: "/baseVisits/"+$scope.baseid,
			method:"GET"
		}).success(function(data){
			$scope.visitors=data;
			console.log(data);
		});
		$http({
			url: "/userInfo/" + data.userid,
			method: "GET"
		}).success(function(data, status){
			console.log(data);
			$scope.userInfo = data;
		}).error(function(data, status){
			console.log("could not retreieve user info " + data.userid);
		});
	}).error(function(data, status){
		console.log(data);
		console.log("could not retrieve base info");
	});
}]);

