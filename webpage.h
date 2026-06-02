#ifndef WEBPAGE_H
#define WEBPAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AI Motor Control</title>
  <style>
    :root { --primary: #007bff; --success: #28a745; --danger: #dc3545; --warning: #ffc107; --bg: #f4f7f6; --card-bg: #ffffff; --text: #333; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 20px; text-align: center; }
    h1 { color: #2c3e50; font-weight: 600; margin-bottom: 20px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; max-width: 900px; margin: 0 auto; }
    .card { background: var(--card-bg); padding: 20px; border-radius: 12px; box-shadow: 0 8px 15px rgba(0,0,0,0.05); }
    .card-title { font-size: 1.1em; color: #7f8c8d; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 5px; }
    .value { font-size: 2.5em; font-weight: bold; color: var(--primary); margin: 5px 0; }
    .unit { font-size: 0.4em; color: #95a5a6; }
    
    .control-panel { background: var(--card-bg); padding: 20px; border-radius: 12px; box-shadow: 0 8px 15px rgba(0,0,0,0.05); max-width: 900px; margin: 20px auto; border: 2px solid transparent; }
    .ai-active { border: 2px solid var(--warning); background-color: #fffdf5; }
    
    input[type=range] { width: 80%; margin: 15px 0; }
    .pwm-display { font-size: 1.5em; font-weight: bold; }
    
    .btn { padding: 10px 20px; border: none; border-radius: 6px; font-size: 1em; font-weight: bold; cursor: pointer; transition: 0.3s; color: white; margin: 5px; text-decoration: none; display: inline-block;}
    .btn-primary { background-color: var(--primary); }
    .btn-success { background-color: var(--success); }
    .btn-danger { background-color: var(--danger); }
    .btn-warning { background-color: var(--warning); color: #333; }
    
    canvas { background: #fafafa; border: 1px solid #ddd; border-radius: 8px; width: 100%; max-height: 250px; margin-top: 15px; }
    .legend { display: flex; justify-content: center; gap: 20px; margin-top: 10px; font-size: 0.9em; font-weight: bold; }
    .leg-sp { color: #007bff; }
    .leg-rp { color: #dc3545; }
  </style>
</head>
<body>
  <h1>AI Motor Dashboard</h1>
  <div class="grid">
    <div class="card">
      <div class="card-title">Motor Speed</div>
      <div class="value" id="rpm">0.0<span class="unit"> RPM</span></div>
    </div>
    <div class="card">
      <div class="card-title">Motor Load</div>
      <div class="value" id="current">0.0<span class="unit"> mA</span></div>
    </div>
    <div class="card" id="ai_card">
      <div class="card-title">Controller</div>
      <div class="value" id="ai_state" style="color: #7f8c8d;">STATIC</div>
      <div style="font-size: 0.9em; color: #555;">Kp: <span id="val_kp">--</span> | Ki: <span id="val_ki">--</span> | Kd: <span id="val_kd">--</span></div>
    </div>
  </div>

  <div class="control-panel" id="control_box">
    <div class="card-title">Live Performance Graph</div>
    <canvas id="rpmChart" width="800" height="250"></canvas>
    <div class="legend">
      <span class="leg-sp">-- Setpoint (Target)</span>
      <span class="leg-rp">-- Real RPM</span>
    </div>
    <hr style="border: 0; border-top: 1px solid #eee; margin: 20px 0;">
    <div class="card-title">Speed Control (RPM)</div>
    <div class="pwm-display">Setpoint: <span id="target_val" style="color: var(--primary);">0</span> RPM</div>
    <input type="range" id="target_slider" min="0" max="200" value="0" oninput="updateSliderText(this.value)" onchange="sendTarget(this.value)">
    <br>
    <button class="btn btn-warning" onclick="toggleAI()">Toggle Adaptive AI</button>
    <button class="btn btn-primary" onclick="stopMotor()">Stop Motor</button>
  </div>

  <div class="control-panel">
    <div class="card-title">Data Management</div>
    <a href="/download" class="btn btn-success">Download CSV</a>
    <button class="btn btn-danger" onclick="clearSD()">Clear Dataset</button>
  </div>
  <div id="status" style="margin-top: 10px; color: #7f8c8d; font-size: 0.9em;">Connecting...</div>

  <script>
    let aiActive = false;
    const canvas = document.getElementById('rpmChart');
    const ctx = canvas.getContext('2d');
    const maxDataPoints = 60; 
    let targetData = new Array(maxDataPoints).fill(0);
    let realData = new Array(maxDataPoints).fill(0);

    function drawGraph() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.strokeStyle = '#eaeaea';
      ctx.lineWidth = 1;
      for(let i=0; i<5; i++) {
        ctx.beginPath(); ctx.moveTo(0, i * (canvas.height/4)); ctx.lineTo(canvas.width, i * (canvas.height/4)); ctx.stroke();
      }

      let maxRPM = Math.max(200, Math.max(...targetData) * 1.2, Math.max(...realData) * 1.2);
      let stepX = canvas.width / (maxDataPoints - 1);

      function drawLine(dataArray, color) {
        ctx.beginPath(); ctx.strokeStyle = color; ctx.lineWidth = 3;
        for(let i=0; i<maxDataPoints; i++) {
          let x = i * stepX;
          let y = canvas.height - ((dataArray[i] / maxRPM) * canvas.height);
          if(i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }

      drawLine(targetData, 'rgba(0, 123, 255, 0.5)'); 
      drawLine(realData, '#dc3545'); 
    }

    function updateSliderText(val) { document.getElementById('target_val').innerText = val; }
    function sendTarget(val) { fetch('/set_target?val=' + val); }
    function stopMotor() {
      document.getElementById('target_slider').value = 0;
      updateSliderText(0); fetch('/set_target?val=0');
    }

    function toggleAI() {
      fetch('/toggle_ai').then(response => response.text()).then(state => {
        aiActive = (state === "ON");
        if(aiActive) {
          document.getElementById('ai_state').innerText = "ADAPTIVE AI";
          document.getElementById('ai_state').style.color = "#ffc107";
          document.getElementById('control_box').classList.add('ai-active');
        } else {
          document.getElementById('ai_state').innerText = "STATIC";
          document.getElementById('ai_state').style.color = "#7f8c8d";
          document.getElementById('control_box').classList.remove('ai-active');
        }
      });
    }

    function clearSD() {
      if(confirm("Delete all data?")) fetch('/clear').then(response => response.text()).then(msg => alert(msg));
    }

    setInterval(function() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('rpm').innerHTML = data.rpm.toFixed(1) + '<span class="unit"> RPM</span>';
          document.getElementById('current').innerHTML = data.current.toFixed(1) + '<span class="unit"> mA</span>';
          document.getElementById('val_kp').innerText = data.kp.toFixed(2);
          document.getElementById('val_ki').innerText = data.ki.toFixed(3);
          document.getElementById('val_kd').innerText = data.kd.toFixed(3);
          
          aiActive = data.ai_mode;
          targetData.shift(); targetData.push(data.target);
          realData.shift(); realData.push(data.rpm);
          drawGraph();
          document.getElementById('status').innerText = "Status: Live Data Active | " + (data.sd_ok ? "SD Card OK" : "SD Card ERROR");
        });
    }, 500);
  </script>
</body>
</html>
)rawliteral";

#endif