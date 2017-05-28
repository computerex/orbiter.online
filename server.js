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


var persisters = {};
var newVessels = {};
var reconStates = [];
var dockEvents = [];

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
  var flattenedStates = flattenByGreatestMJD(tele);
  var retStates = [];
  var mjd = orb.time.JDtoMJD(orb.time.dateToJD(new Date()));
  for(var k = 0; k < flattenedStates.length; k++) {
    if (Object.keys(flattenedStates[k]) == 0) continue;
    var dt = (mjd - flattenedStates[k].mjd) * 60 * 60 * 24;
    if (dt > 3) {
      console.log("ignoring state tele: " + dt);
      continue;
    }
    retStates.push(flattenedStates[k]);
  }
  res.send(retStates, 200);
});

app.get('/mjd', function(req, res){
  res.status(200).send(""+orb.time.JDtoMJD(orb.time.dateToJD(new Date())));
});

app.get('/persister/register', function(req, res) {
  var pid = uuid();
  persisters[pid] = {};
  res.send(pid, 200);
});

function removeFromArray(arr, inxs) {
  var i = arr.length;
  inxs.sort();
  while(i--) {
    var k = inxs.pop();
    if (k == null || typeof k  == 'undefined') break;
    arr.splice(k, 1);
  }
  return arr;
}

app.get('/persister/init', function(req, res) {
  var pid = req.query.pid;
  var newStates = [];
  var persisterKeys = Object.keys(persisters);
  for (var k = 0; k < persisterKeys.length; k++) {
    var key = persisterKeys[k];
    var persister = persisters[key];
    if (persister.length == 0) continue;

    var vesselsToKeep = [];
    for (var j = 0; j < persister.length; j++) {
      var state = persister[j];
      if (state.originalPersister != null && typeof state.originalPersister != 'undefined' && state.originalPersister == pid) {
        delete state.originalPersister;
        state.persisterId = pid;
        newStates.push(state);
      } else {
        vesselsToKeep.push(state);
      }
    }
    persisters[key] = vesselsToKeep;
  }
  console.log("new states: ");
  console.log(newStates);
  persisters[pid] = newStates;
  res.send(pid, 200);
});

app.get('/vessel/delete', function(req, res){
  var vesselToDelete = req.query.name;
  console.log("delete vessel: " + vesselToDelete + " focus: " + req.query.focus);
  var persisterKeys = Object.keys(persisters);
  for(var k = 0; k < persisterKeys.length; k++) {
    var key = persisterKeys[k];
    var persister = persisters[key];
    var i = persister.length;
    var persisterNew = [];
    while(i--) {
      var state = persister[i];
      if (state.name == vesselToDelete) {
        console.log("deleting state: ");
        console.log(state);
        persister.splice(i, 1);
      }
      else persisterNew.push(state);
    }
    persisters[key] = persisterNew;
  }
  res.send({}, 200);
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

app.get('/persister/all', function(req, res){
  res.send(persisters, 200);
});

app.post('/persister/exit', function(req, res) {
  var pid = req.query.pid;
  // TODO move the vessels to an existing persister..
  if (pid != null) {
    var persister = persisters[pid];
    var persisterKeys = Object.keys(persisters);
    for (var k = 0; k < persister.length; k++) {
      if (persister[k].originalPersister == null || typeof persister[k].originalPersister == 'undefined' || persister[k].originalPersister == '')
        persister[k].originalPersister = pid;
    }
    // go through persister list and find a free persister
    var foundKey = null;
    for (var k = 0; k < persisterKeys.length; k++) {
      var key = persisterKeys[k];
      if (key == pid) continue;
      var per = persisters[key];
      if (per.length == 0) continue;
      foundKey = key;
      break;
    }
    if (foundKey != null) {
      for (var k = 0; k < persister.length; k++) {
        persister[k].persisterId = foundKey;
        persisters[foundKey].push(persister[k]);
      }
      console.log("deleting persister: " + pid);
      console.log(persisters[foundKey]);
      console.log(persister);
      persisters[pid] = [];
      newVessels[req.query.pid] = [];
    }
  }
  res.send({}, 200);
});

function reconPrune() {
  var mjd = orb.time.JDtoMJD(orb.time.dateToJD(new Date()));
  var reconNew = [];
  for(var k = 0; k < reconStates.length; k++) {
    var state = reconStates[k];
    var dt = (mjd - state.mjd) * 60 * 60 * 24;
    if (dt < 4) {
      reconNew.push(state);
    }
  }
  reconStates = reconNew;
}

function dockPrune() {
  var mjd = orb.time.JDtoMJD(orb.time.dateToJD(new Date()));
  var dockNew = [];
  for(var k = 0; k < dockEvents.length; k++) {
    var state = dockEvents[k];
    var dt = (mjd - state.mjd) * 60 * 60 * 24;
    if (dt < 4) {
      dockNew.push(state);
    }
  }
  dockEvents = dockNew;
}

app.post('/recon', function(req, res) {
  var copyRecon = JSON.parse(JSON.stringify(reconStates));
  for(var k = 0; k < req.body.length; k++) {
    reconStates.push(req.body[k]);
  }
  console.log("recons: ");
  console.log(reconStates);
  res.send(copyRecon, 200);
});

app.post('/dock', function(req, res) {
  var copyDock = JSON.parse(JSON.stringify(dockEvents));
  var mjd = orb.time.JDtoMJD(orb.time.dateToJD(new Date()));
  for(var k = 0; k < req.body.length; k++) {
    var state = req.body[k];
    state['mjd'] = mjd;
    dockEvents.push(state);
  }
  console.log("dock: ");
  console.log(dockEvents);
  res.send(copyDock, 200);
});

app.post('/posttele', function(req, res){
  var vesselsToSpawn = newVessels[req.query.pid];
  var per = persisters[req.query.pid];

  if (vesselsToSpawn == null || Object.keys(vesselsToSpawn).length == 0) vesselsToSpawn = [];
  if (persisters[req.query.pid] == null || Object.keys(persisters[req.query.pid]).length == 0) 
    persisters[req.query.pid] = [];
  var persisterKeys = Object.keys(persisters);
  var newBody = [];
  var mjd = orb.time.JDtoMJD(orb.time.dateToJD(new Date()));
  for(var k = 0; k < req.body.length; k++) {
    var state = req.body[k];
    var collision = false;
    var dt = (mjd - state.mjd) * 60 * 60 * 24;
    if (dt > 3) {
      console.log("ignoring state posttele: " + dt);
      continue;
    }
    for (var j = 0; j < persisterKeys.length; j++) {
      var key = persisterKeys[j];
      var persister = persisters[key];
      for (var h = 0; h < persister.length; h++) {
        if (persister[h].name == state.name) { collision = true; break; }
      }
      if (collision) break;
    }
    if (!collision) newBody.push(state);
  }
  req.body = newBody;
  var states = req.body.concat(vesselsToSpawn, persisters[req.query.pid]);
  states = flattenByGreatestMJD(states);
  var retStates = [];
  for(var k = 0; k < states.length; k++) {
    if (Object.keys(states[k]) == 0) continue;
    retStates.push(states[k]);
  }
  //if (states != null && Object.keys(states).length != 0)
  persisters[req.query.pid] = retStates;
  newVessels[req.query.pid] = [];
  res.send(persisters[req.query.pid], 200);
});

app.use(express.static(__dirname + '/public_html'));

console.log("magic happens on port 5000\n");

setInterval(reconPrune, 2000);
setInterval(dockPrune, 2000);
app.listen(5000);
