//require the express nodejs module
var express = require('express');
//set an instance of exress
var app = express();
//require the body-parser nodejs module
var bodyParser = require('body-parser');
//require the path nodejs module
var path = require("path");
var json2csv = require('json2csv');

var MongoClient = require('mongodb').MongoClient;
var assert = require('assert');

var mongodb = null;

//support parsing of application/json type post data
app.use(bodyParser.json());
 
//support parsing of application/x-www-form-urlencoded post data
app.use(bodyParser.urlencoded({ extended: true }));

app.use(express.static('public'))

// getDateTime from stackoverflow: https://stackoverflow.com/questions/7357734/how-do-i-get-the-time-of-day-in-javascript-node-js
function getDateTime() {

    var date = new Date();

    var hour = date.getHours();
    hour = (hour < 10 ? "0" : "") + hour;

    var min  = date.getMinutes();
    min = (min < 10 ? "0" : "") + min;

    var sec  = date.getSeconds();
    sec = (sec < 10 ? "0" : "") + sec;

    var year = date.getFullYear();

    var month = date.getMonth() + 1;
    month = (month < 10 ? "0" : "") + month;

    var day  = date.getDate();
    day = (day < 10 ? "0" : "") + day;

    return year + ":" + month + ":" + day + ":" + hour + ":" + min + ":" + sec;

}

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
    var collection = mongodb.collection("samples");
    collection.findOne({"pwr":req.body.pwr}, function(err, result) {
        if (err) throw err;
        if (result) {
            console.log("Received " + req.body.samples.length +" samples for " + req.body.pwr + " " + getDateTime());
            collection.update(
                { "_id": result._id},
                {
                    "$push": {
                        "samples": {
                            "$each": req.body.samples
                        }
                    }
                }
            )
        } else {
            console.log("Received " + req.body.samples.length +" samples for " + req.body.pwr + " (NEW) " + getDateTime());
            collection.insertOne(req.body);
        }
    });
    res.sendStatus(200);
});

app.post('/ppg_accel_samples', function(req, res){
    var collection = mongodb.collection("samples2");
    collection.findOne({"pwr":req.body.pwr}, function(err, result) {
        if (err) throw err;
        if (result) {
            console.log("Received " + req.body.samples.length +" samples for " + req.body.pwr + " " + getDateTime());
            collection.update(
                { "_id": result._id},
                {
                    "$push": {
                        "samples": {
                            "$each": req.body.samples
                        }
                    }
                }
            )
        } else {
            console.log("Received " + req.body.samples.length +" samples for " + req.body.pwr + " (NEW) " + getDateTime());
            collection.insertOne(req.body);
        }
    });
    res.sendStatus(200);
});

app.get('/hw2csv', function(req, res){
    var collection = mongodb.collection("samples");
    var number_of_samples = 6000;
    //{pwr:"6.4"}, { samples: { $slice: -5 } }
    collection.findOne({pwr:"0.4"}, { samples: { $slice: (number_of_samples*-1) } }, function(err, result1) {
        collection.findOne({pwr:"6.4"}, { samples: { $slice: (number_of_samples*-1) } }, function(err, result2) {
            collection.findOne({pwr:"25.4"}, { samples: { $slice: (number_of_samples*-1) } }, function(err, result3) {
                collection.findOne({pwr:"50.0"}, { samples: { $slice: (number_of_samples*-1) } }, function(err, result4) {
                    var combined_columns = [];
                    // console.log(number_of_samples);
                    // console.log(result1.samples.length);
                    for(var i=0; i<result1.samples.length; i++){
                        combined_columns.push({
                            IR1:result1.samples[i].ir,
                            RED1:result1.samples[i].r,
                            IR2:result2.samples[i].ir,
                            RED2:result2.samples[i].r,
                            IR3:result3.samples[i].ir,
                            RED3:result3.samples[i].r,
                            IR4:result4.samples[i].ir,
                            RED4:result4.samples[i].r
                        });
                    }

                    // console.log(combined_columns);
                    var fields = ['IR1','RED1','IR2','RED2','IR3','RED3','IR4','RED4',];
                    var csv = json2csv({ data: combined_columns, fields: fields });
                    res.setHeader('Content-disposition', 'attachment; filename=team8_assignment2.csv');
                    res.set('Content-Type', 'text/csv');
                    res.status(200).send(csv);
                });
            });
        });
    });

});

app.get('/hw4csv', function(req, res){
    var collection = mongodb.collection("samples2");
    var number_of_samples = 250;
    collection.findOne({pwr:"6.4"}, { samples: { $slice: (number_of_samples*-1) } }, function(err, result) {

        var columns = [];
        for(var i=0; i<result.samples.length; i++){
            columns.push({
                X:result.samples[i].x,
                Y:result.samples[i].y,
                Z:result.samples[i].z
            });
        }

        // console.log(combined_columns);
        var fields = ['X','Y','Z'];
        var csv = json2csv({ data: columns, fields: fields });
        res.setHeader('Content-disposition', 'attachment; filename=team8_assignment4.csv');
        res.set('Content-Type', 'text/csv');
        res.status(200).send(csv);

    });

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




