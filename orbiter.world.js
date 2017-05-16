var express = require('express');
var app = express();
var moment = require('moment');
var bcrypt = require('bcrypt-nodejs');
var session = require('express-session');
var FileStore = require('session-file-store')(session);

app.use(session({
    
    secret: '',
    resave: true,
    saveUninitialized: true
  })
);

var bodyParser = require('body-parser')
app.use( bodyParser.json() );       // to support JSON-encoded bodies
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
  extended: true
})); 

var mysql      = require('mysql');
var connection = mysql.createConnection({
  host     : 'localhost',
  user     : 'orbiteronline',
  password : '',
  database: 'orbiteronline'
});

/**
 * Setup a client to automatically replace itself if it is disconnected.
 *
 * @param {Connection} client
 *   A MySQL connection instance.
 */
function replaceClientOnDisconnect(client) {
  client.on("error", function (err) {
    if (!err.fatal) {
      return;
    }
 
    if (err.code !== "PROTOCOL_CONNECTION_LOST") {
      throw err;
    }
 
    // client.config is actually a ConnectionConfig instance, not the original
    // configuration. For most situations this is fine, but if you are doing
    // something more advanced with your connection configuration, then
    // you should check carefully as to whether this is actually going to do
    // what you think it should do.
    client = mysql.createConnection(client.config);
    replaceClientOnDisconnect(client);
    client.connect(function (error) {
      if (error) {
        // Well, we tried. The database has probably fallen over.
        // That's fairly fatal for most applications, so we might as
        // call it a day and go home.
        //
        // For a real application something more sophisticated is
        // probably required here.
        process.exit(1);
      }
    });
  });
}
 
// And run this on every connection as soon as it is created.
replaceClientOnDisconnect(connection);
 
/**
 * Every operation requiring a client should call this function, and not
 * hold on to the resulting client reference.
 *
 * @return {Connection}
 */
exports.getClient = function () {
  return connection;
};


connection.connect();




function doesUserExist(username, processResult){
	connection.query('select * from users where username = ?', [username], function(err, rows, fields){
		if (err) throw err;
		processResult(rows);
	});
}

function registerUser(username, hash, email, processResult){
	console.log("storing hash: " + hash);
	connection.beginTransaction(function(err){
		if ( err ) throw err;

		var id = -1;
		connection.query('insert into users (username, hash, email) values (?, ?, ?);', [username, hash, email], function(err, result, fields){
			if (err) {
				connection.rollback(function(){
					throw err;
				});
			}
			id=result.insertId;	
			connection.query('insert into transactions (balance_change, userid, name) values (?, ?, ?)', [1000000, id, 'pre-alpha surprise'], function(err, results){
				if ( err ){
					connection.rollback(function(){
						throw err;
					});
				}
			});
			processResult(id);
		});
		connection.commit(function(err){
			if (err){
				connection.rollback(function(){
					throw err;
				});
			}
		});
	});
}

app.get('/', function(req, res){
  res.sendFile(__dirname + '/public_html/index.html')
});

app.get('/bases', function(req, res){	
  connection.query('select * from bases;', function(err, rows, fields) {
	  if (err) throw err;
	  res.send(rows);
  });
});

app.get('/loggedInUserInfo', function(req, res){
	if ( req.session.user != null )
		res.send(req.session.user);
	else
		res.send({loggedIn:false});
});
app.get('/addbody/:body', function(req, res){
	connection.query('insert into bodies (name) values (?)', [req.params.body], function(err, result, fields){
		if ( err ) throw err;
		res.send({});
	});
});

app.post('/addbase', function(req, res){
	if ( req.session.user == null ) {
		res.send({error:"could not add base, user not logged in."});
		return;
	}
	var user = req.session.user;
	var name = req.body.name.trim();
	var lon = req.body.lon;
	var lat = req.body.lat;
	var planet = req.body.planet.trim();
	var basedesc = req.body.basedesc;
	var baseurl = req.body.baseurl;
	if ( name.length < 3 || planet.length <= 0 ) return;
	connection.query('insert into bases (landed, planet, lon, lat, name, basedesc, baseurl, userid) values (1, ?, ?, ?, ?, ?, ?, ?)', [planet, lon, lat, name, basedesc, baseurl, user.id], function(err, result, fields){
		res.send({baseid:result.insertId});
	});
});

app.get('/getbodies', function(req, res){
	connection.query('select * from bodies;', function(err, result, fields){
		if ( err ) throw err;
		res.send(result);
	});
});

app.get('/userInfo/:id', function(req, res){
	console.log(req.params);
	console.log(req.params.id);
	connection.query('select users.*, sum(transactions.balance_change) as balance from users join transactions on transactions.userid = users.id where users.id = ?', [req.params.id], function(err, result, fields){
		if (err) throw err;
		if ( result.length > 0 ){
			var first = result[0];
			delete first["hash"];
			res.send(first);
		}else{
			res.send({error:"user doesn't exist"});
		}
	});
});
/*
app.post('/post_transaction', function(req, res){
	if ( req.session.user == null ){
		res.send({error:"user not logged in."});
		return;
	}
	var user = req.session.user;
	var delta = req.body.change;

});*/

app.get('/baseVisits/:id', function(req, res){
	connection.query('select visitors.*, users.username from visitors join users on users.id = visitors.userid where baseid = ? order by created desc limit 10', [req.params.id], function(err,results){
		if(err)throw err;
		res.send(results);
	});
});

app.get('/recent_transactions', function(req, res){
	if ( req.session.user== null ){
		res.send({error:"user not logged in"});
		return;
	}
	var user = req.session.user;
	connection.query('select * from transactions where userid = ? order by created desc limit 10', [user.id], function(err, results){
		if(err) throw err;
		res.send(results);
	});
});

app.post('/post_visit', function(req, res){
	if ( req.session.user == null ){
		res.send({error:-1});
		return;
	}
	var user = req.session.user;
	connection.beginTransaction(function(err){
		if ( err ) throw err;
		connection.query('insert into visitors (userid, baseid) values (?,?)', [user.id, req.body.baseid], function(err, results, fields){
			if (err) {
				connection.rollback(function(){
					throw err;
				});
			}
		});
		connection.query('insert into transactions (balance_change, userid, name) values (?,?,?)', [-5000, user.id, 'base service fee'], function(err, result, fields){
			if ( err ) {
				connection.rollback(function(){
					throw err; 
				});
			}
		});
		connection.query('insert into transactions (balance_change, userid, name) select ?, userid, ? from bases where id = ?', [4800, 'base service fee', req.body.baseid], function(err, result, fields){
			if ( err ) {
				connection.rollback(function(){
					throw err; 
				});
			}
		});
		connection.query('insert into transactions (balance_change, userid, name) values (?, 1, ?)', [200, 'platform fee'], function(err, result, fields){
			if ( err ) {
				connection.rollback(function(){
					throw err; 
				});
			}
		});
		connection.commit(function(err){
			if(err){
				connection.rollback(function(){
					throw err;
				});
			}
		});
		res.send({error:null});
	});
});

app.get('/baseInfo/:id', function(req, res){
	connection.query('select * from bases where id = ?', [req.params.id], function(err, result, fields){
		if ( err ) throw err;
		res.send(result[0]);
	});
});

app.post('/register', function(req, res){
	var username = req.body.username;
	var password = req.body.password;
	var email = req.body.email;
	var hash = bcrypt.hashSync(password);

	// validation
	if ( username.trim().length == 0 || password.trim().length < 5 || email.trim().length < 4 ){
		res.send(null);
		return;
	}
	// check if user already exists
	doesUserExist(username, function(result){
		if ( result == null || result.length == 0 ){
			registerUser(username, hash, email, function(result){
				if ( result != null ) {
					var insertid = result;
					res.send({userId:insertid});
				}else {
					res.send({error:"unable to register user..."});
				}
			});
		}else{
			res.send({error:"user already exists"});
		}
	});
});

var telemetry = [];
var tele_timestamp = {};
function findTeleUser(userId){
	for(var i=0;i<telemetry.length;i++){
		if ( telemetry[i].id == userId )
			return i;
	}
	return -1;
}

app.get('/tele', function(req, res){
	res.send(telemetry);
});

app.post('/posttele', function(req, res){
	if ( req.session.user == null ){
		res.send({error:-1});
		console.log("user not authenticated!");
		return;
	}
	var userid = req.session.user.id;
	var inx = findTeleUser(userid);
	if ( inx == -1 ){
		telemetry.push({});
		inx = telemetry.length - 1;
		telemetry[inx].id = userid;
	}
	if ( tele_timestamp[userid] == null )
		tele_timestamp[userid] = {time:moment()};
	else
		tele_timestamp[userid].time = moment();
	telemetry[inx].ld 	= req.body.ld;
	telemetry[inx].pl 	= req.body.pl;
	telemetry[inx].lon 		= req.body.lon;
	telemetry[inx].lat 		= req.body.lat
	telemetry[inx].rpx 	= req.body.rpx;
	telemetry[inx].rpy 	= req.body.rpy;
	telemetry[inx].rpz 	= req.body.rpz;
	telemetry[inx].rvx 	= req.body.rvx;
	telemetry[inx].rvy 	= req.body.rvy;
	telemetry[inx].rvz 	= req.body.rvz;
	telemetry[inx].arx 	= req.body.arx;
	telemetry[inx].ary 	= req.body.ary;
	telemetry[inx].arz 	= req.body.arz;
	telemetry[inx].h  = req.body.h;
	telemetry[inx].tm     = req.body.tm;
	telemetry[inx].th    = req.body.th;
	telemetry[inx].tr    = req.body.tr;
	telemetry[inx].cn    = req.body.cn;
	telemetry[inx].mjd    = req.body.mjd;
	// for now, just return all vessels...
	// TODO - employ LOD
	var returnTel= [];
	for(var k=0;k<telemetry.length;k++)
	{
		if(telemetry[k].id == userid){
			continue;
		}
		if ( tele_timestamp[telemetry[k].id] != null ){
			var time = tele_timestamp[telemetry[k].id].time;
			var delta = moment().diff(time, 'seconds');
			//console.log(delta);
			if ( delta > 15 ) continue;
		}
		returnTel.push(telemetry[k]);
	}
	/*if ( returnTel.length != telemetry.length-1){
		console.log("For userID: " + userid);
		console.log("returnTel:");
		console.log(returnTel);
		console.log(telemetry);
	}*/
	res.send(returnTel);
});

app.post('/login', function(req, res){
	var username = req.body.username;
	var password = req.body.password;
	var hash = bcrypt.hashSync(password);
	console.log("Checking user: " + username +  " " + password);
	// check if user exists
	doesUserExist(username, function(result){
		if ( result == null || result.length == 0 ){
			res.send({error:"user doesn't exist"});
		}else {
			// user exists
			// check if password is correct
			var user = result[0];
			if ( bcrypt.compareSync(password, user.hash) ){
				delete user["hash"];
				req.session.user = user;
				req.session.save();
				console.log("logging user in");
				res.send({user:user.id})
;			}else{
				res.send({error:"incorrect password"});
			}
		}
	});
});

app.get('/logout', function(req, res){
	req.session.user = null;
	req.session.save();
	res.send(null);
});

app.use(express.static(__dirname + '/public_html'));

app.listen(5000);
