/* ============================================
   app.js - 主应用逻辑 (精简版)
   ============================================ */

const App = (() => {
  let sensorTimer, alertTimer, chartTimer, pestTimer, energyTimer;
  let isForeground = true;
  let alertUnread = 0, lastAlertIds = new Set();
  let retryDelay = 1000;
  const MAX_RETRY = 16000;

  function updateClock() {
    var n = new Date();
    document.getElementById('clock').textContent =
      n.getHours().toString().padStart(2,'0')+':'+n.getMinutes().toString().padStart(2,'0')+':'+n.getSeconds().toString().padStart(2,'0');
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
    var t = document.getElementById('sensor-time');
    data.points.forEach(function(p){
      var w = (p.temp>35||p.temp<15||p.humidity>85||p.humidity<30||p.light>80000||p.ph>8||p.ph<5.5);
      h += '<div class="sensor-card'+(w?' warning':'')+'">';
      h += '<div class="card-title">📍 '+p.name+'</div>';
      h += '<div class="sensor-row"><span class="sensor-label">🌡️ 温度</span><span class="sensor-value'+(p.temp>35||p.temp<15?' danger':'')+'">'+p.temp+'°C</span></div>';
      h += '<div class="sensor-row"><span class="sensor-label">💧 湿度</span><span class="sensor-value'+(p.humidity>85||p.humidity<30?' danger':'')+'">'+p.humidity+'%</span></div>';
      h += '<div class="sensor-row"><span class="sensor-label">☀️ 光照</span><span class="sensor-value'+(p.light>80000?' danger':'')+'">'+p.light+' lx</span></div>';
      h += '<div class="sensor-row"><span class="sensor-label">🧪 pH值</span><span class="sensor-value'+(p.ph>8||p.ph<5.5?' danger':'')+'">'+p.ph+'</span></div>';
      h += '</div>';
    });
    g.innerHTML = h;
    var nn = new Date();
    t.textContent = '更新于 '+nn.getHours().toString().padStart(2,'0')+':'+nn.getMinutes().toString().padStart(2,'0')+':'+nn.getSeconds().toString().padStart(2,'0');
  }

  // ===== 告警面板 =====
  function renderAlerts(data) {
    var l = document.getElementById('alert-list');
    var c = document.getElementById('alert-count');
    var b = document.getElementById('nav-alert-badge');

    if (!data.alerts || !data.alerts.length) {
      l.innerHTML = '<div class="alert-empty">✅ 当前无告警</div>';
      c.textContent = '0'; c.style.display = 'none';
      b.style.display = 'none'; alertUnread = 0; return;
    }

    // 按状态分组
    var groups = { active: [], acked: [], recovered: [] };
    data.alerts.forEach(function(a) {
      var s = a.status || 'active';
      (groups[s] = groups[s] || []).push(a);
    });

    var icons = {temp:'🌡️', humidity:'💧', light:'☀️', ph:'🧪'};
    var groupLabels = { active: '🔴 紧急', acked: '🟡 已确认', recovered: '⚪ 已恢复' };
    var groupClass = { active: 'alert-group-critical', acked: 'alert-group-warn', recovered: 'alert-group-ok' };
    var html = '';

    ['active', 'acked', 'recovered'].forEach(function(status) {
      var items = groups[status] || [];
      if (!items.length) return;
      var collapsed = status !== 'active';
      html += '<div class="alert-group ' + (groupClass[status]||'') + '">';
      html += '<div class="alert-group-header" onclick="this.parentElement.classList.toggle(&quot;collapsed&quot;)">';
      html += '<span>' + groupLabels[status] + ' (' + items.length + ')</span>';
      html += '<span class="group-arrow">' + (collapsed ? '▶' : '▼') + '</span>';
      html += '</div>';
      html += '<div class="alert-group-body">';
      items.forEach(function(a){
        html += '<div class="alert-item' + (a.level==='critical'?' critical':'') + '">';
        html += '<span class="alert-icon">' + (icons[a.field]||'⚠️') + '</span>';
        html += '<div class="alert-body">';
        html += '<div class="alert-text">监测点' + a.point + ' - ' + a.label + '：' + a.value + '（阈值：' + a.threshold + '）</div>';
        html += '<div class="alert-meta">开始于 ' + (a.started_at||'--') + '</div>';
        html += '</div>';
        if (status === 'active') {
          html += '<button class="alert-ack-btn" onclick="App.ackAlert(' + a.id + ',this)" title="确认告警">✔</button>';
        }
        html += '</div>';
      });
      html += '</div></div>';
    });

    l.innerHTML = html;

    // 折叠非 active 组
    if (!groups.active || !groups.active.length) {
      var firstGroup = l.querySelector('.alert-group');
      if (firstGroup) firstGroup.classList.remove('collapsed');
    }

    // 更新角标
    var cnt = (groups.active||[]).length;
    c.textContent = cnt; c.style.display = cnt > 0 ? 'flex' : 'none';
    b.textContent = cnt; b.style.display = cnt > 0 ? 'flex' : 'none';

    // 新告警通知
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

  // ===== 设备控制 =====
  function handleToggle(cb) {
    var point = parseInt(cb.dataset.point), device = cb.dataset.device, action = cb.checked?'on':'off';
    var name = device==='water'?'灌溉':'通风';
    API.controlDevice(point, device, action).then(function(r){
      if (r.success) { Native.vibrate('light'); showToast('✅ 监测点'+point+' '+name+(action==='on'?'已开启':'已关闭')); }
      else throw new Error('fail');
    }).catch(function(){
      cb.checked = !cb.checked; showToast('❌ 控制失败');
    });
  }

  // ===== 数据加载 =====
  function loadSensor() {
    API.getSensorCurrent().then(function(d){ renderSensors(d); updateConn(true); retryDelay=1000; }).catch(function(){ updateConn(false); retryDelay=Math.min(retryDelay*2,MAX_RETRY); });
  }
  function loadAlert() { API.getAlertStatus().then(renderAlerts).catch(function(){}); }
  function loadPest() { API.getPestPredict().then(renderPest).catch(function(){}); }
  function loadEnergy() { API.getEnergyStatus().then(renderEnergy).catch(function(){}); }

  function startPolling() {
    stopPolling();
    sensorTimer = setInterval(loadSensor, 3000);
    alertTimer = setInterval(loadAlert, 5000);
    pestTimer = setInterval(loadPest, 10000);
    chartTimer = setInterval(function(){ Charts.loadHistory(); }, 30000);
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
    updateClock(); setInterval(updateClock, 1000);

    // 初始化图表
    Charts.init();

    // 绑定事件
    document.getElementById('btn-refresh').onclick = function(){ Native.vibrate('light'); refreshAll(); };
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
    document.getElementById('device-grid').onchange = function(e){ if(e.target.type==='checkbox') handleToggle(e.target); };
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

    // 加载数据
    Promise.all([loadSensor(), loadAlert(), loadPest(), loadEnergy(), Charts.loadHistory()]).then(function(){
      startPolling();
      console.log('[App] Ready');
    });
  }

  // 暴露 stopPolling
  // 告警确认
  function ackAlert(id, btn) {
    btn.disabled = true;
    btn.textContent = '...';
    fetch('http://'+window.location.hostname+':8080/api/alert/ack', {
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
    document.getElementById('auth-user-count').textContent = 300 + Math.floor(Math.random()*80);
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
      fetch('http://'+window.location.hostname+':8080/api/auth/register', {
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
    fetch('http://'+window.location.hostname+':8080/api/auth/login', {
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
