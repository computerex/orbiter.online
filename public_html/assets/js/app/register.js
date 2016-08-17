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
app.controller('registerController', ['$scope', '$http', function($scope, $http){
	$scope.register = function() {
		$scope.register = function() {
			var password = $scope.txtPassword.trim();
			var confirm = $scope.txtPasswordConfirm.trim();
			var username = $scope.txtUserName.trim();
			var email = $scope.txtEmail.trim();
			if ( password != confirm ) {
				alert("passwords do not match.");
				return;
			}
			if ( password.length < 4 || password.length > 18 ) {
				alert("password does not meet length criteria.");
				return;
			}
			if ( username.length < 2 || username.length > 33 ) {
				alert("your username is too long...");
				return;
			}
			if ( email.length < 4 || email.length > 350 ) {
				alert("please specify a valid email address! it's important for communication.");
				return;
			}
			var url = "/register";
			$http({
				url: url,
				method: "POST",
				data: {username:$scope.txtUserName, password:$scope.txtPassword, email: $scope.txtEmail}
			}).success(function(data, status){
				console.log(data);
				console.log(status);
				if ( data.error != null ){
					alert(data.error);
				}else {
					alert("successfully registered!");
					window.location.href = "/login.html";
				}
			}).error(function(data, statuts){
				console.log(data);
				console.log(status);
				alert("uh oh! something bad happend, could not register.");
			});
		}
	}
}]);

