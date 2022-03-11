var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
//var connection = new WebSocket('ws://192.168.178.94:81/', ['arduino']);
connection.onopen = function () {
    console.log("Start WebSocket")
  connection.send('Connect ' + new Date());
};

connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
    console.log('Server: ', e.data);

    try {
        var status = JSON.parse(e.data)
        updateStatus(status)
    } catch(e) {
        console.log('no json')
        console.log(e)
    }
};
connection.onclose = function () {
    console.log('WebSocket connection closed');
};

function updateStatus(status) {
    document.getElementById("ssid").textContent = status['ssid']
    document.getElementById("version").textContent = status['version']
    document.getElementById("voltage").textContent = status['batVoltage']
    document.getElementById("capacity").textContent = status['batRate']

    var speed = status['speed'];
    document.getElementById("speed").textContent = speed;

    var direction = status['direction']
    var elemRev = document.getElementById("iconrev")
    var elemFwd = document.getElementById("iconfwd")
    if (direction === 0) {
        // vorwärts
        elemFwd.style = ''
        elemRev.style = 'display: none;'
    } else {
        // rückwärts
        elemFwd.style = 'display: none;'
        elemRev.style = ''
    }
    if (speed === 0) {
        document.getElementById("buttonForward").classList.remove('pure-button-disabled');
        document.getElementById("buttonBackward").classList.remove('pure-button-disabled');
    } else {
        document.getElementById("buttonForward").classList.add('pure-button-disabled');
        document.getElementById("buttonBackward").classList.add('pure-button-disabled');
    }
    
}

/**
 * Button-Action methods
 */

function onBackward() {
    console.log('click BACKWARD')
    connection.send("#DIRBACK")
}

function onForward() {
    console.log('click FORWARD')
    connection.send("#DIRFWD")
}

function onSlower() {
    console.log('click SLOWER')
    connection.send("#SLOWER")
}

function onFaster() {
    console.log('click FASTER')
    connection.send("#FASTER")
}

function onStop() {
    console.log('click STOP')
    connection.send("#STOP")
}