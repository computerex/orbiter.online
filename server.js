var orb = require('orbjs');
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
    res.send([], 200);
    return;
  }
  var tele = [];
  for(var k = 0; k < keys.length; k++) {
    var pid = keys[k];
    tele = tele.concat(persisters[pid]);
  }
  res.send(tele, 200);
});

app.get('/mjd', function(req, res){
  res.status(200).send(""+orb.time.JDtoMJD(orb.time.dateToJD(new Date())));
});

app.get('/persister/register', function(req, res) {
  var pid = uuid();
  persisters[pid] = {};
  res.send(pid, 200);
});

app.post('/persist', function(req, res){
  var persisterId;
  if (req.query.pid != null && persisters[req.query.pid] != null) {
    persisterId = req.query.pid;
  }
  else {
    var keys = Object.keys(persisters);
    if (keys.length < 1) {
      console.log("we have no persisters :(");
      res.send({}, 200);
      return;
    }
    persisterId = keys[0];
  }
  req.body[persisterId] = persisterId;
  newVessels[persisterId] = req.body;
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

function flattenByGreatestMJD(states) {
  var nameToStateHash = {};
  for(var k = 0; k < states.length; k++) {
    var name = states[k].name;
    if (nameToStateHash[name] == null) {
      nameToStateHash[name] = [states[k]];
    } else {
      nameToStateHash[name].push(states[k]);
    }
  }
  var newStates = [];
  var uniqueStateNames = Object.keys(nameToStateHash);
  for(var k = 0; k < uniqueStateNames.length; k++) {
    var allStates = nameToStateHash[uniqueStateNames[k]];
    // select by greatest mjd
    var maxIndex = 0;
    for(var j = 1; j < allStates.length; j++) {
      if (Object.keys(allStates[j]).length == 0) continue;
      if (allStates[j].mjd > allStates[maxIndex]) {
        maxIndex = j;
      }
    }
    newStates.push(allStates[maxIndex]);
  }

  return newStates;
}

app.post('/posttele', function(req, res){
  var vesselsToSpawn = newVessels[req.query.pid];
  if (vesselsToSpawn == null || !Array.isArray(vesselsToSpawn)) vesselsToSpawn = [];
/*  var removeList = [];
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
  if (removeList.length > 0) {
    console.log("!!!!!!!!! PERSISTER STATE !!!!!!!!!!!!!!");
    console.log(persisters[req.query.pid]);
  }
*/
  var per = persisters[req.query.pid];
  if (per == null || !Array.isArray(req.per)) per = [];
  if (req.body == null || !Array.isArray(req.body)) req.body = [];
  var states = flattenByGreatestMJD(req.body.concat(vesselsToSpawn.concat(per)));
  if (Array.isArray(states)) {
    persisters[req.query.pid] = states;
  }
  res.send(persisters[req.query.pid], 200);
});

app.use(express.static(__dirname + '/public_html'));

console.log("magic happens on port 5000\n");
app.listen(5000);
