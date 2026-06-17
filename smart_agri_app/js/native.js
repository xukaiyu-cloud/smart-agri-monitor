/* ============================================
   native.js - Capacitor 原生桥接层
   在浏览器中优雅降级，在 App 中调用原生能力
   ============================================ */

const Native = (() => {
  // 检测是否运行在 Capacitor 环境
  const isCapacitor = typeof window.Capacitor !== 'undefined' && window.Capacitor.isNativePlatform();

  // 动态导入 Capacitor 插件（仅在原生环境）
  let Haptics = null;
  let LocalNotifications = null;
  let Network = null;
  let App = null;
  let StatusBar = null;

  async function init() {
    if (!isCapacitor) return;
    try {
      const mod = await import('https://cdn.jsdelivr.net/npm/@capacitor/core@6/+esm');
      Haptics = (await import('https://cdn.jsdelivr.net/npm/@capacitor/haptics@6/+esm')).Haptics;
      LocalNotifications = (await import('https://cdn.jsdelivr.net/npm/@capacitor/local-notifications@6/+esm')).LocalNotifications;
      Network = (await import('https://cdn.jsdelivr.net/npm/@capacitor/network@6/+esm')).Network;
      App = (await import('https://cdn.jsdelivr.net/npm/@capacitor/app@6/+esm')).App;
      StatusBar = (await import('https://cdn.jsdelivr.net/npm/@capacitor/status-bar@6/+esm')).StatusBar;

      if (StatusBar) {
        StatusBar.setBackgroundColor({ color: '#2e7d32' });
        StatusBar.setStyle({ style: 'DARK' });
      }
      if (LocalNotifications) {
        await LocalNotifications.requestPermissions();
      }
      console.log('[Native] Capacitor 插件初始化完成');
    } catch (e) {
      console.warn('[Native] 插件加载失败，使用浏览器降级模式:', e.message);
    }
  }

  // 振动反馈
  function vibrate(type = 'light') {
    if (Haptics) {
      if (type === 'heavy') Haptics.impact({ style: 'HEAVY' });
      else if (type === 'medium') Haptics.impact({ style: 'MEDIUM' });
      else Haptics.impact({ style: 'LIGHT' });
    }
    // 浏览器降级: 不振动
  }

  // 本地通知
  async function notify(title, body) {
    if (LocalNotifications) {
      await LocalNotifications.schedule({
        notifications: [{
          title, body,
          id: Date.now(),
          schedule: { at: new Date(Date.now() + 100) }
        }]
      });
    }
    // 浏览器降级: 使用 Web Notification API
    else if ('Notification' in window && Notification.permission === 'granted') {
      new Notification(title, { body, icon: 'assets/icon.png' });
    }
    console.log('[Notify]', title, body);
  }

  // 网络状态监听
  function onNetworkChange(callback) {
    if (Network) {
      Network.addListener('networkStatusChange', (status) => {
        callback(status.connected);
      });
    }
    // 浏览器降级: 使用 navigator.onLine
    else {
      window.addEventListener('online', () => callback(true));
      window.addEventListener('offline', () => callback(false));
    }
  }

  // App 前后台状态
  function onAppStateChange(callback) {
    if (App) {
      App.addListener('appStateChange', ({ isActive }) => {
        callback(isActive);
      });
    }
    // 浏览器降级: 使用 visibilitychange
    else {
      document.addEventListener('visibilitychange', () => {
        callback(!document.hidden);
      });
    }
  }

  return { init, vibrate, notify, onNetworkChange, onAppStateChange, isCapacitor };
})();
