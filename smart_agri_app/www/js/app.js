/* ============================================
   app.js - 主应用逻辑 (精简版)
   ============================================ */

let simAnchor = 0;
function simNow() { return simAnchor + (Date.now() - simAnchor) * 300; }
function fmtSimTime() { var d = new Date(simNow()); return d.getHours().toString().padStart(2,"0") + ":" + d.getMinutes().toString().padStart(2,"0") + ":" + d.getSeconds().toString().padStart(2,"0"); }

const App = (() => {
  let sensorTimer, alertTimer, chartTimer, pestTimer, energyTimer;
  let isForeground = true;
  let alertUnread = 0, lastAlertIds = new Set();
  let retryDelay = 1000;
  const MAX_RETRY = 16000;

  function updateAllTimes() {
    var ts = fmtSimTime();
    document.getElementById('clock').textContent = ts;
    var st = document.getElementById('sensor-time');
    if (st) {
      var label = "\u66f4\u65b0\u4e8e " + ts;
      var h = "<span class=\"vf-text\">";
      for (var k = 0; k < label.length; k++) {
        h += '<span class=\"vf-letter\" style=\"animation-delay:' + (k*0.03).toFixed(2) + 's\">' + label[k] + '</span>';
      }
      h += "</span>";
      st.innerHTML = h;
    }
    var ct = document.getElementById("chart-header-time");
    if (ct) {
      var h2 = "<span class=\"vf-text\">\u23f0 ";
      for (var jj = 0; jj < ts.length; jj++) {
        h2 += '<span class=\"vf-letter\" style=\"animation-delay:' + (jj*0.03).toFixed(2) + 's\">' + ts[jj] + '</span>';
      }
      h2 += "</span>";
      ct.innerHTML = h2;
    }
    var ct2 = document.getElementById("chart-sim-time");
    if (ct2) ct2.textContent = "\u23f0 " + ts;
  }

  function showToast(msg, dur) {
    dur = dur || 2000;
    var t = document.getElementById('toast');
    t.textContent = msg; t.classList.add('show');
    clearTimeout(t._tid);
    t._tid = setTimeout(function(){ t.classList.remove('show'); }, dur);
  }

  function updateConn(ok) {
    var d = document.getElementById('conn-status');
    d.classList.toggle('offline', !ok);
  }

  // ===== 传感器面板 =====
  function renderSensors(data) {
    var g = document.getElementById('sensor-grid'), h = '';
    data.points.forEach(function(p){
      var w = (p.temp>35||p.temp<15||p.humidity>85||p.humidity<30||p.light>80000||p.ph>8||p.ph<5.5);
      h += '<div class="sensor-card'+(w?' warning':'')+'">';
      var stHTML = "";
      var ds = (API.deviceStates && API.deviceStates[p.id]) ? API.deviceStates[p.id] : null;
      if (ds && ds.water) stHTML += '<span class="card-status rain">\uD83D\uDCA7</span>';
      if (ds && ds.fan) stHTML += '<span class="card-status wind">\uD83D\uDCA8</span>';
      if (stHTML) stHTML = '<div class="card-status-bar">' + stHTML + "</div>";
      h += '<div class="card-title">\uD83D\uDCCD '+p.name+stHTML+'</div>';
      h += '<div class="sensor-row"><span class="sensor-label">🌡️ 温度</span><span class="sensor-value'+(p.temp>35||p.temp<15?' danger':'')+'">'+p.temp+'°C</span></div>';
      h += '<div class="sensor-row"><span class="sensor-label">💧 湿度</span><span class="sensor-value'+(p.humidity>85||p.humidity<30?' danger':'')+'">'+p.humidity+'%</span></div>';
      h += '<div class="sensor-row"><span class="sensor-label">☀️ 光照</span><span class="sensor-value'+(p.light>80000?' danger':'')+'">'+p.light+' lx</span></div>';
      h += '<div class="sensor-row"><span class="sensor-label">🧪 pH值</span><span class="sensor-value'+(p.ph>8||p.ph<5.5?' danger':'')+'">'+p.ph+'</span></div>';
      h += '</div>';
    });
    g.innerHTML = h;
  }

  // ===== 告警面板 =====
  function renderAlerts(data) {
    var l = document.getElementById("alert-list");
    var c = document.getElementById("alert-count");
    var b = document.getElementById("nav-alert-badge");

    if (!data.alerts || !data.alerts.length) {
      l.innerHTML = '<div class="alert-empty">✅ 当前无告警</div>';
      c.textContent = '0'; c.style.display = 'none';
      b.style.display = 'none'; alertUnread = 0; return;
    }

    var icons = {temp:'🌡️', humidity:'💧', light:'☀️', ph:'🧪'};
    var html = '';
    data.alerts.forEach(function(a){
      html += '<div class="alert-item' + (a.level==='critical'?' critical':'') + '">';
      html += '<span class="alert-icon">' + (icons[a.field]||'⚠️') + '</span>';
      html += '<div class="alert-body">';
      html += '<div class="alert-text">监测点' + a.point + ' - ' + a.label + '：' + a.value + '（阈值：' + a.threshold + '）</div>';
      html += '</div>';
      html += '</div>';
    });

    l.innerHTML = html;

    var cnt = data.alerts.length;
    c.textContent = cnt; c.style.display = cnt > 0 ? 'flex' : 'none';
    b.textContent = cnt; b.style.display = cnt > 0 ? 'flex' : 'none';

    if (cnt > alertUnread && cnt > 0) {
      Native.vibrate('heavy');
      Native.notify('🚨 告警', cnt + '条新告警');
    }
    alertUnread = cnt;
  }

  // ===== 病虫害 =====
  function renderPest(data) {
    document.getElementById('pest-disease').textContent = data.disease||'--';
    document.getElementById('pest-prob').textContent = (data.probability||0).toFixed(1)+'%';
    document.getElementById('pest-advice').textContent = data.advice||'--';
    Charts.updatePestGauge(data.risk||'low', data.probability||0);
  }

  // ===== 能耗 =====
  function renderEnergy(data) {
    document.getElementById('energy-total-val').textContent = data.total.toFixed(1);
    Charts.updateEnergy(data);
    var tips = document.getElementById('energy-tips'), h = '';
    (data.tips||[]).forEach(function(t){ h += '<div class="energy-tip"><span class="tip-icon">💡</span><span>'+t+'</span></div>'; });
    tips.innerHTML = h;
  }

  // ===== 设备面板初始化 =====
  function initDevices() {
    var g = document.getElementById('device-grid'), h = '';
    for (var i=1; i<=5; i++) {
      h += '<div class="device-row"><span class="device-name">📍 监测点 '+i+'</span><div class="device-switches">';
      h += '<label class="toggle-label"><input type="checkbox" data-point="'+i+'" data-device="water"><span class="toggle-track"></span>💧 灌溉</label>';
      h += '<label class="toggle-label"><input type="checkbox" data-point="'+i+'" data-device="fan"><span class="toggle-track"></span>🌀 通风</label>';
      h += '</div></div>';
    }
    g.innerHTML = h;
  }

  // ===== è®¾å¤æ§å¶å¼¹æ¡ =====
  var modalCtx = { point: 0, device: '', cb: null, dur: 60, level: 'mid' };
  var lastSensorData = null;
  var activeDevices = {}; // { '1_water': Date.now() }

  function showDeviceModal(cb) {
    var point = parseInt(cb.dataset.point), device = cb.dataset.device;
    modalCtx.point = point; modalCtx.device = device; modalCtx.cb = cb;
    modalCtx.dur = 60;
    var isWater = device === 'water';
    document.getElementById('modal-icon').textContent = isWater ? '💧' : '🌀';
    document.getElementById('modal-title').textContent = '监测点 ' + point + ' - ' + (isWater ? '灌溉控制' : '通风控制');
    // Check advice first
    document.getElementById('modal-warning').style.display = 'none';
    document.getElementById('modal-duration').style.display = 'none';
    document.getElementById('modal-intensity').style.display = 'none';
    document.getElementById('modal-confirm').textContent = '确认开启';
    document.getElementById('modal-cancel').textContent = '取消';
    document.getElementById('modal-confirm').onclick = confirmDevice;
    document.getElementById('device-modal-overlay').style.display = 'flex';
    API.getAdvice(point, device).then(function(adv){
      if (!adv.advisable) {
        // Show warning
        document.getElementById('modal-warning-msg').textContent = adv.reason;
        document.getElementById('modal-warning').style.display = '';
        document.getElementById('modal-duration').style.display = 'none';
        document.getElementById('modal-intensity').style.display = 'none';
        document.getElementById('modal-cancel').textContent = '听从建议取消';
        document.getElementById('modal-confirm').textContent = '继续开启';
        document.getElementById('modal-confirm').onclick = function(){
          // Hide warning, show controls
          document.getElementById('modal-warning').style.display = 'none';
          document.getElementById('modal-duration').style.display = '';
          document.getElementById('modal-intensity').style.display = '';
          document.getElementById('modal-cancel').textContent = '取消';
          document.getElementById('modal-confirm').textContent = '确认开启';
          document.getElementById('modal-confirm').onclick = confirmDevice;
          setupModalControls();
        };
      } else {
        // No warning, show controls directly
        document.getElementById('modal-warning').style.display = 'none';
        document.getElementById('modal-duration').style.display = '';
        document.getElementById('modal-intensity').style.display = '';
        setupModalControls();
      }
    }).catch(function(){
      document.getElementById('modal-duration').style.display = '';
      document.getElementById('modal-intensity').style.display = '';
      setupModalControls();
    });
  }

  function setupModalControls() {
    var chips = document.querySelectorAll('.duration-chip');
    chips.forEach(function(c){ c.classList.remove('active'); });
    if (chips.length >= 2) chips[1].classList.add('active');
    var iChips = document.querySelectorAll('.intensity-chip');
    modalCtx.level = 'mid';
    iChips.forEach(function(c){ c.classList.remove('active'); });
    if (iChips.length >= 2) iChips[1].classList.add('active');
    document.getElementById('custom-dur-input').value = '';
  }

  function closeDeviceModal() {
    document.getElementById('device-modal-overlay').style.display = 'none';
    modalCtx.cb = null;
  }

  function confirmDevice() {
    if (!modalCtx.cb) return;
    var cb = modalCtx.cb;
    var point = modalCtx.point, device = modalCtx.device;
    cb.checked = true;
    API.deviceStates[point][device] = true;
    var name = device==='water'?'灌溉':'通风';
    var dur = modalCtx.dur, level = modalCtx.level;
    var customVal = parseInt(document.getElementById('custom-dur-input').value);
    if (customVal > 0) dur = customVal;
    var key = point + '_' + device;
    activeDevices[key] = { startTime: Date.now(), dur: dur, checkedTime: simNow() };
    API.controlDevice(point, device, 'on', dur, level).then(function(r){
      if (r.success) {
        Native.vibrate('light');
        showToast('✅ 监测点'+point+' '+name+'已开启 ('+dur+'分钟)');
      } else throw new Error('fail');
    }).catch(function(){
      cb.checked = false; API.deviceStates[point][device] = false;
      delete activeDevices[key];
      showToast('❌ 控制失败');
    });
    closeDeviceModal();
  }

  function loadSensor() {
    API.getSensorCurrent().then(function(d){
      lastSensorData = d;
      renderSensors(d); updateConn(true); retryDelay=1000;
      checkStopConditions(d);
    }).catch(function(){ updateConn(false); retryDelay=Math.min(retryDelay*2,MAX_RETRY); });
  }

  function checkStopConditions(data) {
    var now = simNow();
    var keys = Object.keys(activeDevices);
    keys.forEach(function(key){
      var info = activeDevices[key];
      var elapsed = (now - info.checkedTime) / 60000;
      if (elapsed < 20) return;
      var parts = key.split('_');
      var pt = parseInt(parts[0]), dev = parts[1];
      var s = null;
      data.points.forEach(function(p){ if (p.id === pt) s = p; });
      if (!s) return;
      var shouldStop = false, reason = '';
      if (dev === 'water' && s.humidity > 60) {
        shouldStop = true;
        reason = '监测点'+pt+' 湿度已回升至'+s.humidity+'%，建议关闭灌溉';
      } else if (dev === 'fan' && s.temp < 30 && s.humidity < 70) {
        shouldStop = true;
        reason = '监测点'+pt+' 温度已降至'+s.temp+'°C，环境恢复正常，建议关闭通风';
      }
      if (shouldStop) {
        showStopModal(pt, dev, key, reason);
      }
    });
  }

  function showStopModal(pt, dev, key, reason) {
    var isWater = dev === 'water';
    document.getElementById('modal-icon').textContent = isWater ? '💧' : '🌀';
    document.getElementById('modal-title').textContent = '监测点 ' + pt + ' - 建议关闭';
    document.getElementById('modal-warning').style.display = '';
    document.getElementById('modal-warning-msg').textContent = reason;
    document.getElementById('modal-duration').style.display = 'none';
    document.getElementById('modal-intensity').style.display = 'none';
    document.getElementById('modal-cancel').textContent = '保持开启';
    document.getElementById('modal-confirm').textContent = '立即关闭';
    document.getElementById('modal-cancel').onclick = function(){
      activeDevices[key].checkedTime = simNow();
      closeDeviceModal();
    };
    document.getElementById('modal-confirm').onclick = function(){
      var cb = document.querySelector('input[data-point="'+pt+'"][data-device="'+dev+'"]');
      if (cb) { cb.checked = false; API.deviceStates[pt][dev] = false; }
      delete activeDevices[key];
      var name = isWater ? '灌溉' : '通风';
      API.controlDevice(pt, dev, 'off').then(function(){ showToast('✅ 监测点'+pt+' '+name+'已关闭'); }).catch(function(){});
      closeDeviceModal();
    };
    document.getElementById('device-modal-overlay').style.display = 'flex';
    if ('Notification' in window && Notification.permission === 'granted') {
      new Notification('智慧农业 · 建议关闭设备', { body: reason });
    }
  }
  function loadAlert() { API.getAlertStatus().then(renderAlerts).catch(function(){}); }
  function loadPest() { API.getPestPredict().then(renderPest).catch(function(){}); }
  function loadEnergy() { API.getEnergyStatus().then(renderEnergy).catch(function(){}); }

  function startPolling() {
    stopPolling();
    sensorTimer = setInterval(loadSensor, 3000);
    alertTimer = setInterval(loadAlert, 5000);
    pestTimer = setInterval(loadPest, 10000);
    chartTimer = setInterval(function(){ Charts.loadHistory(); }, 1000);
    energyTimer = setInterval(loadEnergy, 30000);
  }

  function stopPolling() {
    clearInterval(sensorTimer); clearInterval(alertTimer); clearInterval(chartTimer);
    clearInterval(pestTimer); clearInterval(energyTimer);
  }

  function refreshAll() {
    showToast('🔄 刷新中...');
    Promise.all([API.getSensorCurrent(),API.getAlertStatus(),API.getPestPredict(),API.getEnergyStatus(),Charts.loadHistory()])
      .then(function(){ showToast('✅ 刷新完成'); });
  }

  // ===== 导航 =====
  function setNav(id) {
    document.querySelectorAll('.nav-item').forEach(function(n){ n.classList.toggle('active', n.dataset.target===id); });
  }

  function updateNavScroll() {
    var ms = document.querySelectorAll('.module'), best = null, min = Infinity;
    ms.forEach(function(m){ var d = Math.abs(m.getBoundingClientRect().top-120); if(d<min){min=d;best=m;} });
    if(best) setNav(best.id);
  }

  // ===== 主初始化 =====
  function start() {
    initDevices();
    console.log('[App] Starting...');
    updateAllTimes(); setInterval(updateAllTimes, 1000);

    // 初始化图表
    Charts.init();

    // 绑定事件
    document.getElementById('btn-refresh').onclick = function(){ Native.vibrate('light'); refreshAll(); };
    // 流式菜单
    document.getElementById('fm-toggle').onclick = function(e){ e.stopPropagation(); document.getElementById('fluid-menu').classList.toggle('expanded'); };
    document.querySelectorAll('.fm-item').forEach(function(item){ item.onclick = function(){ document.getElementById('fluid-menu').classList.remove('expanded'); }; });
    document.addEventListener('click', function(){ document.getElementById('fluid-menu').classList.remove('expanded'); });
    document.getElementById('chart-point').onchange = function(){
      Charts.switchHistory(parseInt(this.value), document.getElementById('chart-field').value);
    };
    document.getElementById('chart-field').onchange = function(){
      Charts.switchHistory(parseInt(document.getElementById('chart-point').value), this.value);
    };
    document.getElementById('btn-pest-refresh').onclick = function(){ loadPest(); showToast('🐛 重新预测...'); };
    document.querySelectorAll('.nav-item').forEach(function(item){
      item.onclick = function(e){ e.preventDefault(); document.getElementById(this.dataset.target).scrollIntoView({behavior:'smooth'}); };
    });
    document.getElementById("device-grid").addEventListener("change", function(e){ if(e.target.type==="checkbox"){ if(e.target.checked){ e.target.checked = false; showDeviceModal(e.target); } else { var p=parseInt(e.target.dataset.point), d=e.target.dataset.device, n=d==="water"?"灌溉":"通风"; API.deviceStates[p][d]=false; delete activeDevices[p+"_"+d]; API.controlDevice(p,d,"off").then(function(r){ if(r.success){Native.vibrate("light");showToast("✅ 监测点"+p+" "+n+"已关闭");} }).catch(function(){ e.target.checked=true; API.deviceStates[p][d]=true; activeDevices[p+"_"+d]={startTime:Date.now(),dur:0,checkedTime:simNow()}; showToast("❌ 控制失败"); }); } } });
    // å¼¹æ¡äºä»¶
    var durChips = document.querySelectorAll('.duration-chip');
    durChips.forEach(function(c){ c.onclick = function(){ durChips.forEach(function(x){ x.classList.remove('active'); }); c.classList.add('active'); modalCtx.dur = parseInt(c.dataset.dur); }; });
    var iChips2 = document.querySelectorAll('.intensity-chip');
    iChips2.forEach(function(c){ c.onclick = function(){ iChips2.forEach(function(x){ x.classList.remove('active'); }); c.classList.add('active'); modalCtx.level = c.dataset.level; }; });
    var customInput = document.getElementById('custom-dur-input');
    customInput.oninput = function(){ var v = parseInt(this.value); if (v > 0) { document.querySelectorAll('.duration-chip').forEach(function(c){ c.classList.remove('active'); }); modalCtx.dur = v; } else { modalCtx.dur = parseInt(document.querySelector('.duration-chip.active').dataset.dur) || 60; } };
    document.getElementById('modal-cancel').onclick = closeDeviceModal;
    document.getElementById('modal-confirm').onclick = confirmDevice;
    document.getElementById('device-modal-overlay').onclick = function(e){ if(e.target===this) closeDeviceModal(); };
    window.addEventListener('scroll', function(){
      window._st = window._st || 0;
      if(Date.now()-window._st>100){ updateNavScroll(); window._st=Date.now(); }
    });

    // 网络监听
    Native.onNetworkChange(function(c){ updateConn(c); });
    Native.onAppStateChange(function(a){
      isForeground = a;
      if(a){ startPolling(); } else { stopPolling(); }
    });


    // 每次 reset=1 重置锚点
    simAnchor = Date.now();
    Promise.all([loadSensor(), loadAlert(), loadPest(), loadEnergy(), Charts.loadHistory(true)]).then(function(){
      startPolling();
      console.log('[App] Ready');
    });
  }

  // 暴露 stopPolling
  // 告警确认
  function ackAlert(id, btn) {
    btn.disabled = true;
    btn.textContent = '...';
    fetch(API.BASE_URL+'/api/alert/ack', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({id: String(id)})
    }).then(function(r){ return r.json(); }).then(function(d){
      if (d.success) {
        var item = btn.closest('.alert-item');
        if (item) item.style.opacity = '0.4';
        btn.textContent = '✓';
      }
    }).catch(function(){ btn.disabled = false; btn.textContent = '✔'; });
  }

  return { start: start, stopPolling: stopPolling, refreshAll: refreshAll, ackAlert: ackAlert };
})();

/* ============================================
   登录逻辑
   ============================================ */
var Auth = (function(){
  var isReg = false;

  function init(){
    document.body.classList.add('auth-mode');

    var form = document.getElementById('auth-form'); var btn = document.getElementById('auth-btn'); if(form){ form.onsubmit = function(e){ e.preventDefault(); e.stopPropagation(); submit(); return false; }; } if(btn){ btn.onclick = function(e){ e.preventDefault(); submit(); return false; }; }
    document.getElementById('switch-link').onclick = function(e){ e.preventDefault(); toggle(); };
  }

  function toggle(){
    isReg = !isReg;
    document.getElementById('auth-heading').textContent = isReg?'创建账号':'欢迎回来';
    document.getElementById('auth-desc').textContent = isReg?'注册后管理您的农田':'登录您的账号';
    document.getElementById('auth-btn').textContent = isReg?'注 册':'登 录';
    document.getElementById('switch-text').textContent = isReg?'已有账号？':'还没有账号？';
    document.getElementById('switch-link').textContent = isReg?'立即登录':'立即注册';
    document.getElementById('field-confirm').style.display = isReg?'block':'none';
    document.getElementById('auth-form').reset();
    document.getElementById('auth-error').textContent = '';
  }

  function submit(){
    var phone = document.getElementById('input-phone').value.trim();
    var pwd = document.getElementById('input-pwd').value.trim();
    var err = document.getElementById('auth-error');
    var btn = document.getElementById('auth-btn');
    if(!/^1[3-9]\d{9}$/.test(phone)){ err.textContent='请输入正确的11位手机号'; return; }
    if(pwd.length<6){ err.textContent='密码至少6位'; return; }
    err.textContent = '';

    if(isReg){
      var cfm = document.getElementById('input-confirm').value.trim();
      if(pwd!==cfm){ err.textContent='两次密码不一致'; return; }
      btn.textContent = '注册中...'; btn.disabled = true;
      fetch(API.BASE_URL+'/api/auth/register', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body:JSON.stringify({phone:phone,password:pwd,name:'农户'+phone.slice(-4)})
      }).then(function(r){return r.json()}).then(function(d){
        btn.textContent = '注 册'; btn.disabled = false;
        if(d.error){ err.textContent = d.error; return; }
        localStorage.setItem('agri_user', JSON.stringify({name:'农户'+phone.slice(-4),phone:phone}));
        showDash();
      }).catch(function(){ btn.textContent = '注 册'; btn.disabled = false; err.textContent = '服务器连接失败，请确认C++后端已启动'; });
      return;
    }

    btn.textContent = '登录中...'; btn.disabled = true;
    fetch(API.BASE_URL+'/api/auth/login', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body:JSON.stringify({phone:phone,password:pwd})
    }).then(function(r){return r.json()}).then(function(d){
      btn.textContent = '登 录'; btn.disabled = false;
      if(d.error){ err.textContent = d.error; return; }
      localStorage.setItem('agri_user', JSON.stringify({name:d.name,phone:phone}));
      showDash();
    }).catch(function(){ btn.textContent = '登 录'; btn.disabled = false; err.textContent = '服务器连接失败，请确认C++后端已启动'; });
  }

  function showDash(){
    var u = JSON.parse(localStorage.getItem('agri_user'));
    document.getElementById('auth-page').style.display = 'none';
    document.getElementById('app').style.display = 'block';
    document.body.classList.remove('auth-mode');
    document.querySelector('.top-left .title').textContent = '智慧农业 · ' + u.name;
    setTimeout(function(){ App.start(); }, 200);
  }

  return {init:init, showDash:showDash};
})();

document.addEventListener('DOMContentLoaded', function(){
  Auth.init();
});
