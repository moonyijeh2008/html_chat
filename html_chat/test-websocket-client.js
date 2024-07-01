const WebSocket = require('ws');

const ws = new WebSocket('ws://112.172.248.92:6000');

ws.on('open', function open() {
  console.log('WebSocket connection opened');
  ws.send(JSON.stringify({ type: 'join', name: 'test', channel: 'test' }));
  setTimeout(() => {
    ws.send(JSON.stringify({ type: 'message', message: 'Hello from WebSocket client!' }));
  }, 1000);
});

ws.on('message', function incoming(data) {
  console.log('Received message:', data);
});

ws.on('error', function error(error) {
  console.log('WebSocket error:', error.message);
});

ws.on('close', function close() {
  console.log('WebSocket connection closed');
});