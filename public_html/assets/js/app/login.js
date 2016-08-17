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
app.controller('loginController', ['$scope', '$http', function($scope, $http){
    $scope.login = function() {
        console.log("logging in!");
        var username = $scope.txtUserName.trim();
        var password = $scope.txtPassword.trim();

        if ( username.length < 3 || password.length < 3 ) return;

        var url = "/login";
        $http({
            url: url, 
            method: "POST",
            data: {username: username, password: password}
        }).success(function(data, status){
            console.log(data);
            if ( data.error != null ){
                alert(data.error);
            }else {
                window.location.href = "/dash.html";    
            }
        }).error(function(data, status){
            console.log(data);
            alert("uh oh, something bad happened");
        });
    }
	console.log("welcome to login controller");
}]);

