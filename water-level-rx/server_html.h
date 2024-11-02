#ifndef _SERVER_HTML_H
#define _SERVER_HTML_H

const char MAIN_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Water Level RX</title>
</head>
<body>
<style type="text/css">
  .main { display: flex; flex-direction: column; align-items: center; }

  .tabs { display: flex; padding: 0; }
  .tab { list-style-type: none; margin: 0 5px; padding: 2px; border: 1px solid gray; cursor: pointer; }
  .tab.active { background: aquamarine; }
  .tab-content { visibility: collapse; display: flex; flex-direction: column; }
  .tab-content.active { visibility: visible; }

  form { display: flex; flex-direction: column; }
  form > input { margin-bottom: 5px; }
  .error { color:red; }
</style>
<script>
  const $ = document.querySelector.bind(document);
  document.addEventListener('DOMContentLoaded', () => {
    function activateTab(tab) {
      const tabId = tab.getAttribute('data-tab');
      if (tabId) {
        document.querySelectorAll('.tab-content, .tab').forEach(el => el.classList.remove('active'));
        document.getElementById(tabId).classList.add('active');
        tab.classList.add('active');
      }
    }

    $('.tabs').addEventListener('click', e => {
      if (e.target instanceof HTMLElement) {
        activateTab(e.target);
      }
    });

    activateTab($('.tab:first-child'));

    async function fetchJson(url, request) {
      const response = await fetch(url, request);
      if (!response.ok) {
        throw new Error(`Request failed: ${response.status} ${response.statusText}`);
      }
      return await response.json();
    }

    setInterval(async () => {
      try {
        const data = await fetchJson('/sensor');
        $('#sensor-value').innerText = data.depth.toFixed(2);
        $('#sensor-sd').innerText = data.depthSd.toFixed(2);
        $('#sensor-raw').innerText = data.rawPressure.toFixed(4);
        $('#sensor-error').innerText = '';
      } catch (e) {
        $('#sensor-error').innerText = e.message;
      }
    }, 500);

    (async function () {
      try {
        const settings = await fetchJson('/connect-settings');
        $('#connect-ssid').value = settings.ssid;
        $('#connect-pass').value = settings.password;
        $('#connect-error').innerText = '';
      } catch (e) {
        $('#connect-error').innerText = e.message;
      }
    })();

    (async function () {
      try {
        const data = await fetchJson('/calibration');
        $('#calibrate-coeff').value = data.kCoeff;
        $('#calibrate-offset').value = data.pZero;
        $('#calibrate-error').innerText = '';
      } catch (e) {
        $('#calibrate-error').innerText = e.message;
      }
    })();
  });
</script>
<div class="main">
  <h2>Water Level RX</h2>

  <ul class="tabs">
    <li class="tab" data-tab="sensor-tab">Sensor</li>
    <li class="tab" data-tab="connect-tab">Connect</li>
    <li class="tab" data-tab="calibrate-tab">Calibrate</li>
  </ul>

  <section id="sensor-tab" class="tab-content">
    <h3>Sensor</h3>
    <div>Depth: <span id='sensor-value'>&mdash;</span> &plusmn; <span id='sensor-sd'>&mdash;</span>&nbsp;m</div>
    <div>Raw pressure: <span id='sensor-raw'>&mdash;</span></div>
    <div class='error' id='sensor-error'></div>
  </section>

  <section id="connect-tab" class="tab-content">
    <h3>Connect</h3>
    <form method="POST" action="/connect" autocomplete="off">
      <label for="connect-ssid">SSID:</label>
      <input type="text" id="connect-ssid" name="ssid">
      <label for="connect-pass">Password:</label>
      <input type="text" id="connect-pass" name="password">
      <input type="submit" value="Connect">
    </form>
    <div class='error' id='connect-error'></div>
  </section>

  <section id="calibrate-tab" class="tab-content">
    <h3>Calibrate</h3>
    <p><code>h = (p - p0) / k</code></p>
    <p>where <b>p</b> is measured pressure, <b>h</b> is computed depth.
    <form method="POST" action="/calibration" autocomplete="off">
      <label for="calibrate-coeff"><b>k</b> (&#x3c1;&times;g) (default: 120):</label>
      <input type="number" id="calibrate-coeff" name="kCoeff" min="1" max="1000" step="0.0001">
      <label for="calibrate-offset"><b>p0</b> (default: 100):</label>
      <input type="number" id="calibrate-offset" name="pZero" min="-1000" max="1000" step="0.0001">
      <input type="submit" value="Apply">
    </form>
    <div class='error' id='calibrate-error'></div>
  </section>
</div>
</body></html>)rawliteral";

#endif