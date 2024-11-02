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
