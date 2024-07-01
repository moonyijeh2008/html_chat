const WebSocket = require('ws');
const net = require('net');

const wss = new WebSocket.Server({ port: 6000 });

console.log('WebSocket server started on port 6000');

wss.on('connection', function connection(ws) {
    console.log('WebSocket client connected');
    
    const tcpSocket = new net.Socket();
    tcpSocket.connect(54972, '127.0.0.1', function() {
        console.log('Connected to TCP server');
    });

    tcpSocket.on('data', function(data) {
        ws.send(data.toString());
    });

    tcpSocket.on('close', function() {
        ws.close();
    });

    tcpSocket.on('error', function(err) {
        console.log('TCP Socket error: ' + err.message);
        ws.close();
    });

    ws.on('message', function incoming(message) {
        tcpSocket.write(message);
    });

    ws.on('close', function() {
        tcpSocket.end();
    });

    ws.on('error', function(err) {
        console.log('WebSocket error: ' + err.message);
        tcpSocket.end();
    });
});