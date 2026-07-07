const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const mqtt = require('mqtt');
const path = require('path');
const fs = require('fs');
require('dotenv').config({ path: path.join(__dirname, '../.env') });

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static(path.join(__dirname, 'public')));

const AWS_ENDPOINT = process.env.AWS_IOT_ENDPOINT;
const TOPIC = process.env.AWS_IOT_TOPIC || "esp32/face_tracker/data";
const CLIENT_ID = process.env.AWS_IOT_CLIENT_ID ? (process.env.AWS_IOT_CLIENT_ID + "-Dashboard") : "ESP32-CAM-Tracker-Dashboard";

console.log(`Connecting to AWS IoT Endpoint: ${AWS_ENDPOINT}`);

try {
    const certsDir = path.join(__dirname, 'certs');
    const options = {
        host: AWS_ENDPOINT,
        port: 8883,
        protocol: 'mqtts',
        clientId: CLIENT_ID,
        key: fs.readFileSync(path.join(certsDir, 'private.pem.key')),
        cert: fs.readFileSync(path.join(certsDir, 'certificate.pem.crt')),
        ca: [fs.readFileSync(path.join(certsDir, 'AmazonRootCA1.pem'))],
        rejectUnauthorized: true,
    };

    const client = mqtt.connect(options);

    client.on('connect', () => {
        console.log('Connected to AWS IoT Core!');
        client.subscribe(TOPIC, (err) => {
            if (!err) {
                console.log(`Subscribed to topic: ${TOPIC}`);
            } else {
                console.error("Subscription error:", err);
            }
        });
    });

    client.on('message', (topic, message) => {
        const payload = message.toString();
        io.emit('telemetry', payload);
    });

    client.on('error', (err) => {
        console.error('MQTT Error:', err);
    });

} catch (err) {
    console.error("Failed to load certificates or connect to MQTT. Ensure certs are placed in the 'certs/' folder.");
    console.error(err.message);
}

io.on('connection', (socket) => {
    console.log('Web client connected:', socket.id);
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`Dashboard Server running on http://localhost:${PORT}`);
});
