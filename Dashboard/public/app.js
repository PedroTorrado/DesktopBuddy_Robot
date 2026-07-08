const socket = io();

// UI Elements
const dot = document.getElementById('connection-dot');
const statusText = document.getElementById('connection-text');
const logContainer = document.getElementById('log-container');
const statId = document.getElementById('stat-id');
const statX = document.getElementById('stat-x');
const statY = document.getElementById('stat-y');

const statCo2 = document.getElementById('stat-co2');
const statTvoc = document.getElementById('stat-tvoc');
const statDist = document.getElementById('stat-dist');
const emotionBadge = document.getElementById('emotion-badge');
const robotFace = document.getElementById('robot-face');

// Canvas Setup for Radar
const canvas = document.getElementById('radarCanvas');
const ctx = canvas.getContext('2d');
const CAM_WIDTH = 320;
const CAM_HEIGHT = 240;

// Target state
let targetX = -1;
let targetY = -1;
let hasTarget = false;
let lastUpdateTime = 0;

// Chart.js Setup
const chartCanvas = document.getElementById('sensorChart');
const ctxChart = chartCanvas.getContext('2d');

// Create nice gradient fills for the chart
let gradientCO2 = ctxChart.createLinearGradient(0, 0, 0, 400);
gradientCO2.addColorStop(0, 'rgba(56, 189, 248, 0.5)'); // Oceanic Blue
gradientCO2.addColorStop(1, 'rgba(56, 189, 248, 0.0)');

let gradientTVOC = ctxChart.createLinearGradient(0, 0, 0, 400);
gradientTVOC.addColorStop(0, 'rgba(244, 63, 94, 0.5)'); // Danger Red
gradientTVOC.addColorStop(1, 'rgba(244, 63, 94, 0.0)');

Chart.defaults.color = '#94a3b8';
Chart.defaults.font.family = "'Outfit', sans-serif";

const maxDataPoints = 30; // Show last 30 readings
const sensorChart = new Chart(ctxChart, {
    type: 'line',
    data: {
        labels: [], // Timestamps
        datasets: [
            {
                label: 'CO2 (ppm)',
                data: [],
                borderColor: '#38bdf8',
                backgroundColor: gradientCO2,
                borderWidth: 2,
                pointBackgroundColor: '#0f172a',
                pointBorderColor: '#38bdf8',
                pointHoverBackgroundColor: '#38bdf8',
                pointHoverBorderColor: '#fff',
                fill: true,
                tension: 0.4,
                yAxisID: 'y'
            },
            {
                label: 'TVOC (ppb)',
                data: [],
                borderColor: '#f43f5e',
                backgroundColor: gradientTVOC,
                borderWidth: 2,
                pointBackgroundColor: '#0f172a',
                pointBorderColor: '#f43f5e',
                pointHoverBackgroundColor: '#f43f5e',
                pointHoverBorderColor: '#fff',
                fill: true,
                tension: 0.4,
                yAxisID: 'y1'
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
            mode: 'index',
            intersect: false,
        },
        scales: {
            y: {
                type: 'linear',
                display: true,
                position: 'left',
                grid: { color: 'rgba(255, 255, 255, 0.05)' },
                title: { display: true, text: 'CO2 (ppm)', color: '#38bdf8' }
            },
            y1: {
                type: 'linear',
                display: true,
                position: 'right',
                grid: { drawOnChartArea: false },
                title: { display: true, text: 'TVOC (ppb)', color: '#f43f5e' }
            },
            x: {
                grid: { color: 'rgba(255, 255, 255, 0.05)' }
            }
        },
        plugins: {
            legend: { 
                position: 'top',
                labels: {
                    usePointStyle: true,
                    boxWidth: 8
                }
            },
            tooltip: {
                backgroundColor: 'rgba(15, 23, 42, 0.9)',
                titleFont: { family: "'Outfit', sans-serif", size: 13 },
                bodyFont: { family: "'Space Mono', monospace", size: 12 },
                padding: 10,
                borderColor: 'rgba(255,255,255,0.1)',
                borderWidth: 1
            }
        },
        animation: {
            duration: 0 // Disable animation for live scrolling
        }
    }
});

// Historical Data Store
const allHistory = {
    labels: [],
    co2: [],
    tvoc: []
};
let viewMode = 'live'; // 'live' or 'all'

// UI Toggle Buttons for Chart Range
const btnLive = document.getElementById('btn-live');
const btnAll = document.getElementById('btn-all');

function setViewMode(mode) {
    if (viewMode === mode) return;
    viewMode = mode;
    
    if (mode === 'live') {
        btnLive.classList.add('active');
        btnAll.classList.remove('active');
        
        const startIdx = Math.max(0, allHistory.labels.length - maxDataPoints);
        sensorChart.data.labels = allHistory.labels.slice(startIdx);
        sensorChart.data.datasets[0].data = allHistory.co2.slice(startIdx);
        sensorChart.data.datasets[1].data = allHistory.tvoc.slice(startIdx);
    } else {
        btnAll.classList.add('active');
        btnLive.classList.remove('active');
        
        sensorChart.data.labels = [...allHistory.labels];
        sensorChart.data.datasets[0].data = [...allHistory.co2];
        sensorChart.data.datasets[1].data = [...allHistory.tvoc];
    }
    sensorChart.update();
}

btnLive.addEventListener('click', () => setViewMode('live'));
btnAll.addEventListener('click', () => setViewMode('all'));



// Socket events
socket.on('connect', () => {
    dot.className = 'dot connected';
    statusText.textContent = 'SYSTEM ONLINE';
    addLog('Connected to Dashboard Relay Server', true);
});

socket.on('disconnect', () => {
    dot.className = 'dot disconnected';
    statusText.textContent = 'SYSTEM OFFLINE';
    addLog('Disconnected from server!', true);
    hasTarget = false;
});

socket.on('telemetry', (dataStr) => {
    try {
        const data = JSON.parse(dataStr);
        addLog(dataStr, false, data.face_id === -2);
        
        // Handle Face Data
        if (data.face_id !== undefined) {
            statId.textContent = data.face_id;
            if (data.face_id >= -1) {
                hasTarget = true;
                targetX = data.x;
                targetY = data.y;
                lastUpdateTime = Date.now();
                statX.textContent = targetX;
                statY.textContent = targetY;
                
                // Instantly update dashboard face to smiling (HAPPY) when target is active on radar
                robotFace.className = 'robot-face happy';
                emotionBadge.textContent = 'HAPPY';
                emotionBadge.className = 'emotion-badge happy';
            } else {
                hasTarget = false;
                statX.textContent = '--';
                statY.textContent = '--';
            }
        }

        // Handle Sensor Data
        if (data.co2 !== undefined) {
            statCo2.textContent = data.co2;
            statTvoc.textContent = data.tvoc;
            statDist.textContent = data.distance + " cm";

            // Update Chart
            const time = new Date().toLocaleTimeString([], {hour12:false, hour:'2-digit', minute:'2-digit', second:'2-digit'});
            
            // Save to absolute history
            allHistory.labels.push(time);
            allHistory.co2.push(data.co2);
            allHistory.tvoc.push(data.tvoc);
            
            // Add to active chart view
            sensorChart.data.labels.push(time);
            sensorChart.data.datasets[0].data.push(data.co2);
            sensorChart.data.datasets[1].data.push(data.tvoc);

            if (viewMode === 'live') {
                if (sensorChart.data.labels.length > maxDataPoints) {
                    sensorChart.data.labels.shift();
                    sensorChart.data.datasets[0].data.shift();
                    sensorChart.data.datasets[1].data.shift();
                }
            } else {
                // In all-time mode, make sure active chart datasets match full history exactly
                sensorChart.data.labels = [...allHistory.labels];
                sensorChart.data.datasets[0].data = [...allHistory.co2];
                sensorChart.data.datasets[1].data = [...allHistory.tvoc];
            }
            sensorChart.update();
        }

        // Handle Emotion Data
        if (data.emotion !== undefined) {
            const emotionsMap = {
                0: { text: 'HAPPY', class: 'happy' },
                1: { text: 'ANGRY', class: 'angry' },
                2: { text: 'SEARCHING', class: 'searching' },
                3: { text: 'DIZZY', class: 'dizzy' }
            };
            const currentEmo = emotionsMap[data.emotion] || emotionsMap[0];
            
            // Update class of face container
            robotFace.className = `robot-face ${currentEmo.class}`;
            
            // Update text and class of badge
            emotionBadge.textContent = currentEmo.text;
            emotionBadge.className = `emotion-badge ${currentEmo.class}`;
        }

    } catch (e) {
        console.error("Failed to parse telemetry:", e);
    }
});

function addLog(msg, isSystem = false, isLost = false) {
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    if (isLost) entry.classList.add('lost');
    
    const time = new Date().toLocaleTimeString();
    entry.innerHTML = `<span class="timestamp">[${time}]</span> ${isSystem ? `<strong>${msg}</strong>` : msg}`;
    
    logContainer.prepend(entry);
    
    // Keep max 50 logs
    while (logContainer.children.length > 50) {
        logContainer.removeChild(logContainer.lastChild);
    }
}

// Animation Loop for Radar
function drawRadar() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // Draw crosshairs
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(canvas.width/2, 0);
    ctx.lineTo(canvas.width/2, canvas.height);
    ctx.moveTo(0, canvas.height/2);
    ctx.lineTo(canvas.width, canvas.height/2);
    ctx.stroke();

    // Draw Target
    if (hasTarget && Date.now() - lastUpdateTime < 2000) {
        // Flip the X axis
        const drawX = canvas.width - ((targetX / CAM_WIDTH) * canvas.width);
        const drawY = (targetY / CAM_HEIGHT) * canvas.height;
        
        // Draw blip
        ctx.fillStyle = '#38bdf8';
        ctx.beginPath();
        ctx.arc(drawX, drawY, 8, 0, Math.PI * 2);
        ctx.fill();
        
        // Draw pulse ring
        const time = Date.now() / 1000;
        const radius = 10 + Math.sin(time * 5) * 5;
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(drawX, drawY, radius, 0, Math.PI * 2);
        ctx.stroke();
        
        // Draw bounding box corners
        const boxSize = 40;
        ctx.strokeStyle = '#f43f5e';
        ctx.lineWidth = 2;
        
        // Top Left
        ctx.beginPath();
        ctx.moveTo(drawX - boxSize, drawX - boxSize/2);
        ctx.lineTo(drawX - boxSize, drawY - boxSize);
        ctx.lineTo(drawX - boxSize/2, drawY - boxSize);
        ctx.stroke();
        
        // Bottom Right
        ctx.beginPath();
        ctx.moveTo(drawX + boxSize, drawY + boxSize/2);
        ctx.lineTo(drawX + boxSize, drawY + boxSize);
        ctx.lineTo(drawX + boxSize/2, drawY + boxSize);
        ctx.stroke();
    } else {
        hasTarget = false;
    }
    
    requestAnimationFrame(drawRadar);
}

// Start animation
drawRadar();
