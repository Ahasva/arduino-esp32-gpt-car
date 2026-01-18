let mode = 'chat';
let autopilotRunning = false;
let autopilotGoal = "";

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function stopAutopilotAndCar() {
  autopilotRunning = false;
  try { await fetch('/stopAll', { method: 'POST' }); } catch {}
}

document.getElementById('weatherModeButton').addEventListener('click', async () => {
    await stopAutopilotAndCar();
    mode = 'weather';
    addMessage("Weather mode activated!", "assistant");
    document.getElementById('carControlContainer').style.display = 'none';
});

document.getElementById('chatModeButton').addEventListener('click', async () => {
    await stopAutopilotAndCar();
    mode = 'chat';
    addMessage("Chat mode activated!", "assistant");
    document.getElementById('carControlContainer').style.display = 'none';
});

document.getElementById('carModeButton').addEventListener('click', async () => {
    await stopAutopilotAndCar();
    mode = 'car';
    addMessage("Car mode activated!", "assistant");
    document.getElementById('carControlContainer').style.display = 'block';
});

document.getElementById('autopilotStartBtn').addEventListener('click', () => {
    if (autopilotRunning) {
        addMessage("Autopilot already running.", "assistant");
        return;
    }
    const goal = document.getElementById('userInput').value.trim();
    if (!goal) {
        alert("Type an autopilot goal into the input first.");
        return;
    }
    autopilotGoal = goal;
    autopilotRunning = true;

    mode = 'car';
    document.getElementById('carControlContainer').style.display = 'block';

    addMessage("Autopilot START: " + autopilotGoal, "assistant");
    autopilotLoop();
});

document.getElementById('autopilotStopBtn').addEventListener('click', async () => {
    await stopAutopilotAndCar();
    addMessage("Autopilot STOP", "assistant");
    
});

document.getElementById('submitButton').addEventListener('click', () => {
    const userInput = document.getElementById('userInput').value;
    if (userInput.trim() === '') {
        alert("Please enter message!");
        return;
    }
    addMessage(userInput, "user");

    if (mode === 'weather') {
        fetchWeather(userInput);
    } else if (mode === 'chat') {
        fetchChatResponse(userInput);
    } else if (mode === 'car') {
        fetchCarResponse(userInput);
    }
    document.getElementById('userInput').value = '';
});

function addMessage(text, sender) {
  const messageContainer = document.getElementById('messageContainer');
  const messageDiv = document.createElement('div');
  messageDiv.classList.add('message', sender);

  const messageText = document.createElement('p');
  messageText.textContent = text;

  messageDiv.appendChild(messageText);
  messageContainer.appendChild(messageDiv);

  messageContainer.scrollTop = messageContainer.scrollHeight;
}

function fetchWeather(userInput) {
    fetch('/getWeather', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({message: userInput})
    })
    .then(response => response.json())
    .then(data => {
        addMessage(data.response, "assistant");
    })  
    .catch(error => {
        console.error("Error: ", error);
        addMessage("Failed to retrieve weather data", "assistant");
    });
}

function fetchChatResponse(userInput) {
    fetch('/chat', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({message: userInput})
    })
    .then(response => response.json())
    .then(data => {
        addMessage(data.response, "assistant");
    })
    .catch(error => {
        console.error("Error: ", error);
        addMessage("Failed to access chat endpoint", "assistant");
    });
}

function fetchCarResponse(userInput) {
  // 1) get plan from ChatGPT
  fetch('/carPlan', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ message: userInput })
  })
  .then(r => r.json())
  .then(plan => {
    // Show response
    addMessage("Plan: " + JSON.stringify(plan), "assistant");

    // ✅ if planning failed, STOP HERE
    if (!plan || !plan.sequence) {
        throw new Error(plan?.error || "Planner did not return sequence");
    }

    // execute plan
    return fetch('/carExec', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(plan)
    });
  })
  .then(r => r.json())
  .then(res => {
    if (res.ok) addMessage("Executed plan ✅", "assistant");
    else addMessage("Execution error: " + (res.error || "unknown"), "assistant");
  })
  .catch(err => {
    console.error("Car plan error:", err);
    addMessage("Car plan failed", "assistant");
  });
}

async function autopilotLoop() {
    while (autopilotRunning) {
        try {
        // 1) plan
        const planResp = await fetch('/carPlan', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ message: autopilotGoal })
        });

        const plan = await planResp.json();
        if (!planResp.ok || !plan || !plan.sequence) {
            addMessage("Autopilot plan error: " + (plan.error || "no sequence"), "assistant");
            await sleep(600);
            continue;
        }

        // (optional) show plan sometimes; comment out if spammy
        // addMessage("Plan: " + JSON.stringify(plan), "assistant");

        // 2) execute
        const execResp = await fetch('/carExec', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(plan)
        });

        const exec = await execResp.json();

        // 3) decision
        if (exec.ok) {
            await sleep(150); // small pacing delay
        } else {
            addMessage("Autopilot blocked: " + (exec.error || "unknown") + " → replanning...", "assistant");
            await sleep(500);
        }

        } catch (err) {
        console.error("autopilot error:", err);
        addMessage("Autopilot error → retrying...", "assistant");
        await sleep(800);
        }
    }

    // ensure stop when leaving autopilot
    try { await fetch('/stopAll', { method: 'POST' }); } catch {}
}

function sendCommand(command) {
    if (command === "stop") autopilotRunning = false;
    fetch('/car', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command })
    })
    .then(r => r.json())
    .then(res => {
        console.log("car:", res);
        if (!res.ok) addMessage("Manual drive error: " + (res.error || "unknown"), "assistant");
    })
    .catch(err => console.error("car error:", err));
}

function getObstacleStatus() {
    let xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (xhr.readyState === 4 && xhr.status === 200) {
            let status = xhr.responseText;
            console.log(status);
            updateSensorStatus(status);
        } else if (xhr.readyState === 4) {
            console.error("Error with XHR request");
        }
    }
    xhr.open("GET", "/status", true);
    xhr.send();
}

function updateSensorStatus(payload) {
  payload = payload.trim();
  if (!payload) return;

  const [statusRaw, distRaw] = payload.split(",");
  const status = (statusRaw || "").trim();
  const dist = (distRaw || "").trim();

  if (status.length < 3) return;

  const leftSensor = document.getElementById("leftSensor");
  const centerSensor = document.getElementById("centerSensor");
  const rightSensor = document.getElementById("rightSensor");
  const distanceText = document.getElementById("distanceText");

  leftSensor.style.backgroundColor   = (status[0] === '1') ? 'deepskyblue' : 'deeppink';
  centerSensor.style.backgroundColor = (status[1] === '1') ? 'deepskyblue' : 'deeppink';
  rightSensor.style.backgroundColor  = (status[2] === '1') ? 'deepskyblue' : 'deeppink';

  if (distanceText) {
    distanceText.textContent = dist ? `Distance: ${dist} cm` : "Distance: -- cm";
  }
}

window.onload = function() {
    getObstacleStatus();
};

window.addEventListener("blur", async () => {
  await stopAutopilotAndCar();
});

document.addEventListener("visibilitychange", async () => {
  if (document.hidden) await stopAutopilotAndCar();
});

setInterval(getObstacleStatus, 1000);