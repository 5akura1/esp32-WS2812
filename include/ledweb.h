#ifndef __LEDWEB_H__
#define __LEDWEB_H__
#include <pgmspace.h>

char index_html[] PROGMEM = R"=====(
<!doctype html>
<html lang='en' dir='ltr'>
<head>
  <meta http-equiv='Content-Type' content='text/html; charset=utf-8' />
  <meta name='viewport' content='width=device-width, initial-scale=1.0' />
  <title>ESP32-WS2812s Control</title>
  <script type='text/javascript' src='main.js'></script>

  <style>
    body {
      font-family:Arial,sans-serif;
      margin:10px;
      padding:0;
      background-color:#202020;
      color:#909090;
      text-align:center;
    }

    .flex-row {
      display:flex;
      flex-direction:row;
    }

    .flex-row-wrap {
      display:flex;
      flex-direction:row;
      flex-wrap:wrap;
    }

    .flex-col {
      display:flex;
      flex-direction:column;
      align-items:center;
    }

    input[type='text'] {
      background-color: #d0d0d0;
      color:#404040;
    }

    ul {
      list-style-type: none;
    }

    ul li a {
      display:block;
      margin:3px;
      padding:10px;
      border:2px solid #404040;
      border-radius:5px;
      color:#909090;
      text-decoration:none;
    }

    ul#modes li a {
      min-width:220px;
    }

    ul.control li a {
      min-width:60px;
      min-height:24px;
    }

    ul.control {
      display:flex;
      flex-direction:row;
      justify-content: flex-end;
      align-items: center;
      padding: 0px;
    }

    ul li a.active {
      border:2px solid #909090;
    }
  </style>
</head>
<body>
  <h1>ESP32-WS2812s Control</h1>
  <div class='flex-row'>

    <div class='flex-col'>
      <div><canvas id='color-canvas' width='360' height='360'></canvas><br/></div>
      <div><input type='text' id='color-value' oninput='onColor(event, this.value)'/></div>

      <div>
        <ul class='control'>
          <li>Brightness:</li>
          <li><a href='#' onclick="onBrightness(event, '-')">&#9788;</a></li>
          <li><a href='#' onclick="onBrightness(event, '+')">&#9728;</a></li>
        </ul>

        <ul class='control'>
          <li>Speed:</li>
          <li><a href='#' onclick="onSpeed(event, '-')">&#8722;</a></li>
          <li><a href='#' onclick="onSpeed(event, '+')">&#43;</a></li>
        </ul>

        <ul class='control'>
          <li>Auto cycle:</li>
          <li><a href='#' onclick="onAuto(event, '-')">&#9632;</a></li>
          <li><a href='#' onclick="onAuto(event, '+')">&#9658;</a></li>
        </ul>
      </div>
    </div>

    <div>
      <ul id='modes' class='flex-row-wrap'>
    </div>
  </div>
</body>
</html>
)=====";

char main_js[] PROGMEM = R"=====(

var activeButton = null;
var colorCanvas = null;

window.addEventListener('DOMContentLoaded', (event) => {
  // init the canvas color picker
  colorCanvas = document.getElementById('color-canvas');
  var colorctx = colorCanvas.getContext('2d');

  // Create color gradient
  var gradient = colorctx.createLinearGradient(0, 0, colorCanvas.width - 1, 0);
  gradient.addColorStop(0,    "rgb(255,   0,   0)");
  gradient.addColorStop(0.16, "rgb(255,   0, 255)");
  gradient.addColorStop(0.33, "rgb(0,     0, 255)");
  gradient.addColorStop(0.49, "rgb(0,   255, 255)");
  gradient.addColorStop(0.66, "rgb(0,   255,   0)");
  gradient.addColorStop(0.82, "rgb(255, 255,   0)");
  gradient.addColorStop(1,    "rgb(255,   0,   0)");

  // Apply gradient to canvas
  colorctx.fillStyle = gradient;
  colorctx.fillRect(0, 0, colorCanvas.width - 1, colorCanvas.height - 1);

  // Create semi transparent gradient (white -> transparent -> black)
  gradient = colorctx.createLinearGradient(0, 0, 0, colorCanvas.height - 1);
  gradient.addColorStop(0,    "rgba(255, 255, 255, 1)");
  gradient.addColorStop(0.48, "rgba(255, 255, 255, 0)");
  gradient.addColorStop(0.52, "rgba(0,     0,   0, 0)");
  gradient.addColorStop(1,    "rgba(0,     0,   0, 1)");

  // Apply gradient to canvas
  colorctx.fillStyle = gradient;
  colorctx.fillRect(0, 0, colorCanvas.width - 1, colorCanvas.height - 1);

  // setup the canvas click listener
  colorCanvas.addEventListener('click', (event) => {
    var imageData = colorCanvas.getContext('2d').getImageData(event.offsetX, event.offsetY, 1, 1);

    var selectedColor = 'rgb(' + imageData.data[0] + ',' + imageData.data[1] + ',' + imageData.data[2] + ')'; 
    //console.log('click: ' + event.offsetX + ', ' + event.offsetY + ', ' + selectedColor);
    document.getElementById('color-value').value = selectedColor;

    selectedColor = imageData.data[0] * 65536 + imageData.data[1] * 256 + imageData.data[2];
    submitVal('c', selectedColor);
  });

  // get list of modes from ESP
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
   if (xhttp.readyState == 4 && xhttp.status == 200) {
     document.getElementById('modes').innerHTML = xhttp.responseText;
     modes = document.querySelectorAll('ul#modes li a');
     modes.forEach(initMode);
   }
  };
  xhttp.open('GET', 'modes', true);
  xhttp.send();
});

function initMode(mode, index) {
  mode.addEventListener('click', (event) => onMode(event, index));
}

function onColor(event, color) {
  event.preventDefault();
  var match = color.match(/rgb\(([0-9]*),([0-9]*),([0-9]*)\)/);
  if(match) {
    var colorValue = Number(match[1]) * 65536 + Number(match[2]) * 256 + Number(match[3]);
    //console.log('onColor:' + match[1] + "," + match[2] + "," + match[3] + "," + colorValue);
    submitVal('c', colorValue);
  }
}

function onMode(event, mode) {
  event.preventDefault();
  if(activeButton) activeButton.classList.remove('active')
  activeButton = event.target;
  activeButton.classList.add('active');
  submitVal('m', mode);
}

function onBrightness(event, dir) {
  event.preventDefault();
  submitVal('b', dir);
}

function onSpeed(event, dir) {
  event.preventDefault();
  submitVal('s', dir);
}

function onAuto(event, dir) {
  event.preventDefault();
  submitVal('a', dir);
}

function submitVal(name, val) {
  var xhttp = new XMLHttpRequest();
  xhttp.open('GET', 'set?' + name + '=' + val, true);
  xhttp.send();
}
)=====";

#endif