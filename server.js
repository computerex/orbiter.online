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
  res.send(flattenByGreatestMJD(tele), 200);
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


app.post('/posttele', function(req, res){
  var vesselsToSpawn = newVessels[req.query.pid];
  var per = persisters[req.query.pid];

  if (vesselsToSpawn == null || Object.keys(vesselsToSpawn).length == 0) vesselsToSpawn = [];
  if (persisters[req.query.pid] == null || Object.keys(persisters[req.query.pid]).length == 0) 
    persisters[req.query.pid] = [];
  var states = req.body.concat(vesselsToSpawn, persisters[req.query.pid]);
  console.log("states: ");
  console.log(states);
  states = flattenByGreatestMJD(states);
  if (states != null && Object.keys(states).length != 0)
    persisters[req.query.pid] = states;
  res.send(persisters[req.query.pid], 200);
});

app.use(express.static(__dirname + '/public_html'));

console.log("magic happens on port 5000\n");
app.listen(5000);
