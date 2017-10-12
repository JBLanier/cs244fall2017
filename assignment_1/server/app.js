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
var messageCollection = null;

//support parsing of application/json type post data
app.use(bodyParser.json());
 
//support parsing of application/x-www-form-urlencoded post data
app.use(bodyParser.urlencoded({ extended: true }));

app.use(express.static('public'))



//tell express what to do when the /about route is requested
app.post('/', function(req, res){
    res.setHeader('Content-Type', 'application/json');
 
    mongodb.insertOne(req.body);

    res.send(JSON.stringify({
        Greeting: 'Yo Dude'
    }));
 
    //debugging output for the terminal
    console.log('Received post: ' + JSON.stringify(req.body));
});
 
var port = 80;
// Connection URL
var url = 'mongodb://localhost:27017/hw1';
// Use connect method to connect to the server
MongoClient.connect(url, function(err, db) {
  if (err != null) {
  	console.log("Failed to connect to Mongo: " + err);
  	assert(false);
  }

  console.log("Connected successfully to Mongo at " + url);
  mongodb = db;
  messageCollection = db.collection('messages');

  //wait for a connection
  app.listen(port, function () {
  	console.log('HTTP Server is running. Point your browser to: http://localhost:' + port);
  });
});



