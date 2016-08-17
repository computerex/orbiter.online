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
app.controller('dashController', ['$scope', '$http', function($scope, $http){
	console.log("welcome to dash controller");
	$scope.displayIntro=true;
	$scope.bodies = [];
	$scope.toggleIntro = function() {
		$scope.displayIntro = !$scope.displayIntro;
	}

	$http({
		url: "/loggedInUserInfo",
		method: "GET"
	}).success(function(data, status){
		console.log(data);
		console.log("this is losgin controller");
		if ( data == null || data.loggedIn == false )
			window.location.href = "/login.html";
		else {
			$scope.user = data;
			$http({
					url: "/userInfo/" + data.id,
					method: "GET"
			}).success(function(data, status){
				console.log(data);
				if ( data != null )
					$scope.userInfo = data;
			}).error(function(data, status){
				alert("unable to retrieve user info");
			});
		}
	}).error(function(data, status){
	});

	$http({
		url:"/recent_transactions",
		method:"GET"
	}).success(function(data, status){
		console.log(data);
		$scope.transactions = data;
	}).error(function(data, status){
		alert("unable to retrieve recent transactions");
	});

	$http({
		url: "/getbodies",
		method: "GET"
	}).success(function(data, status){
		console.log(data);
		$scope.bodies = data;
	}).error(function(data, status){
	});

	$http({
		url: "/bases",
		method: "GET"
	}).success(function(data, status){
		console.log(data);
		$scope.bases = data;
	}).error(function(data, status){
	});

	$scope.addbase = function() {
		$http({
			url: "/addbase",
			method: "POST",
			data: {name: $scope.txtName, lon: $scope.txtLon, lat: $scope.txtLat, planet: $scope.txtPlanet, basedesc: $scope.txtDesc, baseurl: $scope.txtBaseUrl}
		}).success(function(data, status){
			console.log(data);
			if ( data != null && data.error == null ){
				location.reload();
			}
		}).error(function(data, status){
		});
	}

}]);

