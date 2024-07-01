document.getElementById('join-btn').addEventListener('click', joinChat);
document.getElementById('send-btn').addEventListener('click', sendMessage);

let ws;

function joinChat() {
    const name = document.getElementById('name').value;
    const channel = document.getElementById('channel').value;
    if (name && channel) {
        console.log('Attempting to open WebSocket connection...');

        ws = new WebSocket('ws://112.172.248.92:6000');
        console.log('WebSocket object created:', ws);

        ws.onopen = () => {
            console.log('WebSocket connection opened');
            ws.send(JSON.stringify({ type: 'join', name, channel }));
            document.getElementById('message-input').style.display = 'flex';
        };
        ws.onerror = (error) => {
            console.error('WebSocket Error: ', error);
        };
        ws.onmessage = (event) => {
            console.log('Received message:', event.data);
            try {
                const msg = JSON.parse(event.data);
                const chat = document.getElementById('chat');
                chat.innerHTML += `<p><strong>${msg.name}:</strong> ${msg.message}</p>`;
                chat.scrollTop = chat.scrollHeight;
            } catch (e) {
                console.error('Error parsing message:', e);
            }
        };
        ws.onclose = () => {
            console.log('WebSocket is closed now.');
        };
    } else {
        alert("Please enter both your name and the channel name.");
    }
}

function sendMessage() {
    const message = document.getElementById('message').value;
    if (message) {
        console.log('Sending message:', message);
        ws.send(JSON.stringify({ type: 'message', message }));
        document.getElementById('message').value = '';
    }
}
