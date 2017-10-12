//require the express nodejs module
var express = require('express');
//set an instance of exress
var app = express();
//require the body-parser nodejs module
var bodyParser = require('body-parser');
//require the path nodejs module
var path = require("path");

//support parsing of application/json type post data
app.use(bodyParser.json());
 
//support parsing of application/x-www-form-urlencoded post data
app.use(bodyParser.urlencoded({ extended: true }));

//tell express that www is the root of our public web folder
app.use(express.static(path.join(__dirname, 'www')));
 
//tell express what to do when the /about route is requested
app.post('/', function(req, res){
    res.setHeader('Content-Type', 'application/json');
 
    res.send(JSON.stringify({
        Greeting: 'Yo Dude'
    }));
 
    //debugging output for the terminal
    console.log('Received post: ' + JSON.stringify(req.body));
});
 
//wait for a connection
var port = 80;
app.listen(port, function () {
  console.log('Server is running. Point your browser to: http://localhost:' + port);
});