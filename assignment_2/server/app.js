//require the express nodejs module
var express = require('express');
//set an instance of exress
var app = express();
//require the body-parser nodejs module
var bodyParser = require('body-parser');
//require the path nodejs module
var path = require("path");

var MongoClient = require('mongodb').MongoClient;
var assert = require('assert');

var mongodb = null;

//support parsing of application/json type post data
app.use(bodyParser.json());
 
//support parsing of application/x-www-form-urlencoded post data
app.use(bodyParser.urlencoded({ extended: true }));

app.use(express.static('public'))

app.post('/', function(req, res){
    res.setHeader('Content-Type', 'application/json');
 
    mongodb.collection("messages").insertOne(req.body);

    res.send(JSON.stringify({
        Greeting: 'Yo Dude'
    }));
 
    //debugging output for the terminal
    console.log('Received post: ' + JSON.stringify(req.body));
});

app.post('/samples', function(req, res){
    mongodb.collection("samples").insertOne(req.body);
    res.sendStatus(200);

    //debugging output for the terminal
    console.log('Received samples: ' + JSON.stringify(req.body));
});

app.get('/csv', function(req, res){
    res.setHeader('Content-disposition', 'attachment; filename=hw2_samples.csv');
    res.setHeader('Content-Type', 'text/csv');
    res.status(200).send(csv);
});

app.get('/time', function(req, res){
    res.send((new Date).getTime().toString());
});
 
var port = 80;
// Connection URL
var url = 'mongodb://localhost:27017/hw2';
// Use connect method to connect to the server
MongoClient.connect(url, function(err, db) {
  if (err != null) {
  	console.log("Failed to connect to Mongo: " + err);
  	assert(false);
  }

  console.log("Connected successfully to Mongo at " + url);
  mongodb = db;

  //wait for a connection
  var server = app.listen(port, function () {
  	console.log('HTTP Server is running. Point your browser to: http://localhost:' + port);
  });

    server.on('connection', function(socket) {
        console.log("A new connection was made by a client.");
        socket.setTimeout(30 * 1000);
        // 30 second timeout. Change this as you see fit.
    })
});



