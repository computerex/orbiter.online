var express = require('express');
var app = express();
var uuid = require('uuid/v4');

var bodyParser = require('body-parser')
app.use( bodyParser.json() );       // to support JSON-encoded bodies
app.use(bodyParser.urlencoded({     // to support URL-encoded bodies
  extended: true
})); 

app.use(function (err, req, res, next) {
//call handler here
});

app.get('/', function(req, res){
  res.sendFile(__dirname + '/public_html/index.html')
});


var telemetry = [];
var tele_timestamp = {};
var persisters = {};
var newVessels = {};

app.get('/tele', function(req, res){
  var keys = Object.keys(persisters);
  if (keys.length < 1) {
    console.log("we have no persisters :(");
    res.send({}, 200);
    return;
  }
  var tele = [];
  for(var k = 0; k < keys.length; k++) {
    var pid = keys[k];
    tele = tele.concat(persisters[pid]);
  }
  res.send(tele, 200);
});

app.get('/persister/register', function(req, res) {
  var pid = uuid();
  persisters[pid] = {};
  console.log(pid);
  res.send(pid, 200);
});

app.post('/persist', function(req, res){
  console.log("current persisters: ");
  console.log(persisters);
  var keys = Object.keys(persisters);
  console.log(keys);
  if (keys.length < 1) {
    console.log("we have no persisters :(");
    res.send({}, 200);
    return;
  }
  var pid = keys[0];
  newVessels[pid] = req.body;
  res.send({}, 200);
});

app.post('/persister/exit', function(req, res) {
  var pid = req.query.pid;
  // TODO move the vessels to an existing persister..
  if (pid != null) {
    console.log("deleting persister: " + pid);
    delete persisters.pid;
  }
  res.send({}, 200);
});

app.post('/teletest', function(req, res) {
  console.log(req.body);
  console.log(req.query);
  res.send({}, 200);
});

app.post('/posttele', function(req, res){
  var vesselsToSpawn = newVessels[req.query.pid];
  if (vesselsToSpawn == null) vesselsToSpawn = [];
  var removeList = [];
  for(var k = 0; k < vesselsToSpawn.length; k++) {
    for(var j = 0; j < persisters[req.query.pid].length; j++) {
      if (persisters[req.query.pid][j].name == vesselsToSpawn[k].name) {
        removeList.push(vesselsToSpawn[k].name);
        break;
      }
    }
  }
  console.log("remove list: ");
  console.log(removeList);
  for(var k = 0; k < removeList.length; k++) {
    var nameToRemove = removeList[k];
    var i = vesselsToSpawn.length;
    while (i--) {
      if (vesselsToSpawn[i].name == nameToRemove) {
        vesselsToSpawn.splice(i, 1);
        break;
      }
    }
  }
  persisters[req.query.pid] = req.body.concat(vesselsToSpawn);
  console.log(persisters);
  res.send(persisters[req.query.pid], 200);
});

app.use(express.static(__dirname + '/public_html'));

console.log("magic happens on port 5000\n");
app.listen(5000);
