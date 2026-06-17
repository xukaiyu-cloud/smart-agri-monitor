/* ============================================
   api.js - HTTP 请求层
   ============================================ */

const API = (() => {
  const BASE_URL = 'http://'+window.location.hostname+':8080';

  const SENSOR_FIELDS = ['temp', 'humidity', 'light', 'ph'];
  const FIELD_LABELS = { temp: '温度', humidity: '湿度', light: '光照', ph: 'pH值' };
  const FIELD_UNITS = { temp: '°C', humidity: '%', light: 'lx', ph: '' };
  const FIELD_THRESHOLDS = {
    temp:  { min: 15, max: 35 },
    humidity: { min: 30, max: 85 },
    light: { min: 2000, max: 80000 },
    ph:   { min: 5.5, max: 8.0 }
  };

  const deviceStates = {
    1: { water: false, fan: false },
    2: { water: false, fan: false },
    3: { water: false, fan: false },
    4: { water: false, fan: false },
    5: { water: false, fan: false }
  };

  // ========== HTTP 请求 ==========
  async function request(method, path, body = null) {
    const opts = { method, headers: { 'Content-Type': 'application/json' } };
    if (body) opts.body = JSON.stringify(body);
    try {
      const res = await fetch(BASE_URL + path, opts);
      const data = await res.json();
      if (!res.ok && data.error) throw new Error(data.error);
      return data;
    } catch (e) {
      console.warn('[API] ' + path + ' 请求失败:', e.message);
      throw e;
    }
  }

  return {
    getSensorCurrent: () => request('GET', '/api/sensor/current'),
    getSensorHistory: (point, field, reset) => request('GET', '/api/sensor/history?point=' + point + '&field=' + field + (reset ? '&reset=1' : '')),
    getAlertStatus: () => request('GET', '/api/alert/status'),
    getPestPredict: () => request('GET', '/api/pest/predict'),
    getEnergyStatus: () => request('GET', '/api/energy/status'),
        getAdvice: (point, device) => request('GET', '/api/sensor/advice?point=' + point + '&device=' + device),
    shouldStop: (point, device) => request('GET', '/api/sensor/should_stop?point=' + point + '&device=' + device),
    controlDevice: (point, device, action, dur, level) => request('POST', '/api/device/control', { point, device, action, dur, level }),
    FIELD_LABELS, FIELD_UNITS, FIELD_THRESHOLDS, SENSOR_FIELDS, deviceStates
  };
})();
