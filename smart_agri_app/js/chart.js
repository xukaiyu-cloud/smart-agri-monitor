/* ============================================
   chart.js - ECharts 图表管理
   管理 3 个图表：历史曲线、病虫害仪表盘、能耗柱状图
   ============================================ */

const Charts = (() => {
  let historyChart = null;
  let pestGauge = null;
  let energyChart = null;
  let currentPoint = 1;
  let currentField = 'temp';

  // 初始化所有图表
  function init() {
    const d1 = document.getElementById('chart-container');
    const d2 = document.getElementById('pest-gauge');
    const d3 = document.getElementById('energy-chart');
    const w = Math.max(document.documentElement.clientWidth - 24, 300);
    historyChart = echarts.init(d1, null, {width: d1.offsetWidth||w, height: d1.offsetHeight||320});
    pestGauge = echarts.init(d2, null, {width: d2.offsetWidth||w, height: d2.offsetHeight||200});
    energyChart = echarts.init(d3, null, {width: d3.offsetWidth||w, height: d3.offsetHeight||200});

    // 窗口大小变化时重绘
    window.addEventListener('resize', () => {
      historyChart && historyChart.resize();
      pestGauge && pestGauge.resize();
      energyChart && energyChart.resize();
    });

    // 监听屏幕方向变化 (移动端)
    if (window.matchMedia) {
      window.matchMedia('(orientation: portrait)').addListener(() => {
        setTimeout(() => {
          historyChart && historyChart.resize();
          pestGauge && pestGauge.resize();
          energyChart && energyChart.resize();
        }, 300);
      });
    }

    // 初始化病虫害仪表盘（空状态）
    updatePestGauge('low', 0);
  }

  // ========== 历史曲线 ==========
  function updateHistory(data) {
    if (!historyChart) return;

    const labels = data.data.map(d => {
      const dt = new Date(d[0]);
      return dt.getHours().toString().padStart(2, '0') + ':' + dt.getMinutes().toString().padStart(2, '0');
    });
    const values = data.data.map(d => d[1]);
    const field = data.field;
    const unit = API.FIELD_UNITS[field] || '';
    const threshold = data.threshold;

    const option = {
      tooltip: {
        trigger: 'axis',
        confine: true,
        textStyle: { fontSize: 12 }
      },
      legend: {
        data: [API.FIELD_LABELS[field]],
        top: 0,
        textStyle: { fontSize: 12 }
      },
      grid: {
        left: 50, right: 20, top: 30, bottom: 30
      },
      xAxis: {
        type: 'category',
        boundaryGap: false,
        data: labels,
        axisLabel: {
          fontSize: 10,
          interval: 23 // 每小时显示一个标签
        }
      },
      yAxis: {
        type: 'value',
        name: unit,
        nameTextStyle: { fontSize: 11 },
        axisLabel: { fontSize: 10 }
      },
      dataZoom: [{
        type: 'inside',
        start: 0,
        end: 100
      }, {
        type: 'slider',
        start: 0,
        end: 100,
        height: 20,
        bottom: 0,
        handleSize: '80%',
        textStyle: { fontSize: 10 }
      }],
      series: [{
        name: API.FIELD_LABELS[field],
        type: 'line',
        data: values,
        smooth: true,
        lineStyle: { width: 2, color: '#4caf50' },
        itemStyle: { color: '#4caf50' },
        areaStyle: {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(76,175,80,0.3)' },
            { offset: 1, color: 'rgba(76,175,80,0.05)' }
          ])
        },
        markLine: threshold ? {
          silent: true,
          symbol: 'none',
          data: [
            { yAxis: threshold.min, lineStyle: { color: "#f57c00", type: "dashed", width: 2 }, label: { show: true, position: "insideEndTop", formatter: "下限 {c}", color: "#f57c00", fontSize: 11 } },
            { yAxis: threshold.max, lineStyle: { color: "#d32f2f", type: "dashed", width: 2 }, label: { show: true, position: "insideEndTop", formatter: "上限 {c}", color: "#d32f2f", fontSize: 11 } }
          ]
        } : undefined
      }]
    };

    historyChart.setOption(option, true); historyChart.resize();
  }

  // ========== 病虫害仪表盘 ==========
  function updatePestGauge(risk, probability) {
    if (!pestGauge) return;

    const riskColors = { low: '#4caf50', medium: '#f57c00', high: '#d32f2f' };
    const riskLabels = { low: '低风险', medium: '中风险', high: '高风险' };

    const option = {
      series: [{
        type: 'gauge',
        startAngle: 180,
        endAngle: 0,
        center: ['50%', '70%'],
        radius: '90%',
        min: 0,
        max: 100,
        splitNumber: 10,
        axisLine: {
          show: true,
          lineStyle: {
            width: 20,
            color: [
              [0.33, '#4caf50'],
              [0.66, '#f57c00'],
              [1, '#d32f2f']
            ]
          }
        },
        pointer: {
          icon: 'path://M12.8,0.7l12,40.1H0.7L12.8,0.7z',
          length: '60%',
          width: 8,
          offsetCenter: [0, '-50%'],
          itemStyle: {
            color: riskColors[risk] || '#4caf50'
          }
        },
        axisTick: {
          length: 10,
          lineStyle: { color: 'auto', width: 1 }
        },
        splitLine: {
          length: 20,
          lineStyle: { color: 'auto', width: 3 }
        },
        axisLabel: {
          color: '#757575',
          fontSize: 11,
          distance: -40,
          formatter: function (v) { return v + '%'; }
        },
        title: {
          offsetCenter: [0, '-15%'],
          fontSize: 14,
          color: '#212121'
        },
        detail: {
          fontSize: 28,
          offsetCenter: [0, '30%'],
          valueAnimation: true,
          formatter: function (v) { return v.toFixed(1) + '%'; },
          color: riskColors[risk] || '#4caf50'
        },
        data: [{ value: probability, name: riskLabels[risk] || '低风险' }]
      }]
    };

    pestGauge.setOption(option, true); pestGauge.resize();
  }

  // ========== 能耗柱状图 ==========
  function updateEnergy(data) {
    if (!energyChart) return;

    const points = data.points.map(p => '监测点 ' + p.id);
    const waterData = data.points.map(p => p.water);
    const fanData = data.points.map(p => p.fan);
    const lightingData = data.points.map(p => p.lighting);

    const option = {
      tooltip: {
        trigger: 'axis',
        axisPointer: { type: 'shadow' },
        confine: true
      },
      legend: {
        data: ['灌溉', '通风', '照明'],
        top: 0,
        textStyle: { fontSize: 11 }
      },
      grid: {
        left: 40, right: 20, top: 35, bottom: 25
      },
      xAxis: {
        type: 'category',
        data: points,
        axisLabel: { fontSize: 10 }
      },
      yAxis: {
        type: 'value',
        name: 'kWh',
        axisLabel: { fontSize: 10 }
      },
      series: [
        {
          name: '灌溉',
          type: 'bar',
          data: waterData,
          itemStyle: { color: '#2196f3', borderRadius: [4, 4, 0, 0] },
          barMaxWidth: 24
        },
        {
          name: '通风',
          type: 'bar',
          data: fanData,
          itemStyle: { color: '#4caf50', borderRadius: [4, 4, 0, 0] },
          barMaxWidth: 24
        },
        {
          name: '照明',
          type: 'bar',
          data: lightingData,
          itemStyle: { color: '#ff9800', borderRadius: [4, 4, 0, 0] },
          barMaxWidth: 24
        }
      ]
    };

    energyChart.setOption(option, true); energyChart.resize();
  }

  // 切换历史曲线
  function switchHistory(point, field) {
    currentPoint = point;
    currentField = field;
    loadHistory();
  }

  async function loadHistory() {
    try {
      const data = await API.getSensorHistory(currentPoint, currentField);
      updateHistory(data);
    } catch (e) {
      console.error('[Chart] 历史数据加载失败:', e);
    }
  }

  return { init, updateHistory, updatePestGauge, updateEnergy, switchHistory, loadHistory, resizeAll() { historyChart && historyChart.resize(); pestGauge && pestGauge.resize(); energyChart && energyChart.resize(); } };
})();
