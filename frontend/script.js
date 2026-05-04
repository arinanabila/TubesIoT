document.addEventListener('DOMContentLoaded', () => {

    // =============================================
    // 1. TAB NAVIGATION
    // =============================================
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');

    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            tabBtns.forEach(b => b.classList.remove('active'));
            tabContents.forEach(c => c.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(`tab-${btn.getAttribute('data-tab')}`).classList.add('active');
        });
    });

    // =============================================
    // 2. INITIALIZE CHART.JS
    // =============================================
    const ctx = document.getElementById('tempChart').getContext('2d');
    let gradient = ctx.createLinearGradient(0, 0, 0, 400);
    gradient.addColorStop(0, 'rgba(67, 24, 255, 0.4)');
    gradient.addColorStop(1, 'rgba(67, 24, 255, 0.0)');

    const MAX_CHART_POINTS = 30;
    const chartLabels = [];
    const chartData = [];

    const tempChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: chartLabels,
            datasets: [{
                label: 'Suhu (°C)',
                data: chartData,
                borderColor: '#4318ff',
                backgroundColor: gradient,
                borderWidth: 2,
                pointRadius: 0,
                pointHoverRadius: 4,
                fill: true,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                tooltip: {
                    backgroundColor: '#2b3674',
                    padding: 10,
                    cornerRadius: 8,
                    displayColors: false,
                }
            },
            scales: {
                y: {
                    min: 0,
                    max: 60,
                    ticks: { stepSize: 15, color: '#a3aed1', font: { size: 11, family: 'Inter' } },
                    grid: { color: '#e2e8f0', borderDash: [5, 5] },
                    border: { display: false }
                },
                x: {
                    ticks: { color: '#a3aed1', font: { size: 11, family: 'Inter' }, maxTicksLimit: 10 },
                    grid: { display: false },
                    border: { display: false }
                }
            }
        }
    });

    // =============================================
    // 3. DOM ELEMENTS
    // =============================================
    const tempProgress  = document.getElementById('temp-progress');
    const tempValue     = document.getElementById('temp-value');
    const tempIcon      = document.getElementById('temp-icon');
    const humValue      = document.getElementById('hum-value');

    const fireBanner    = document.getElementById('fire-status-banner');
    const fireShieldIcon = document.getElementById('fire-shield-icon');
    const fireTitle     = document.getElementById('fire-status-title');
    const fireDesc      = document.getElementById('fire-status-desc');
    const fireAlert     = document.getElementById('fire-alert-box');
    const fireLastChecked = document.getElementById('fire-last-checked');

    const syncTerakhir  = document.getElementById('last-sync');
    const dataPointsEl  = document.getElementById('data-points');
    const uptimeEl      = document.getElementById('uptime');
    const statusNodeMCU = document.getElementById('status-nodemcu');
    const notificationList = document.getElementById('notification-list');

    const toggleKipas   = document.getElementById('toggle-kipas');
    const toggleAlarm   = document.getElementById('toggle-alarm');

    let totalDataPoints = 0;
    let uptimeMinutes   = 0;
    let lastDangerState = null;

    // =============================================
    // 4. HELPER: FORMAT TIME
    // =============================================
    function getTimeStr() {
        const now = new Date();
        const h = now.getHours().toString().padStart(2, '0');
        const m = now.getMinutes().toString().padStart(2, '0');
        const s = now.getSeconds().toString().padStart(2, '0');
        return `${h}.${m}.${s}`;
    }

    function getShortTimeStr() {
        const now = new Date();
        return `${now.getHours()}.${now.getMinutes().toString().padStart(2,'0')}`;
    }

    // =============================================
    // 5. HELPER: TAMBAH NOTIFIKASI
    // =============================================
    function addNotification(type, title) {
        const timeStr = getTimeStr();
        const icon      = type === 'danger' ? '<i class="fas fa-fire"></i>' : '<i class="fas fa-check-circle"></i>';
        const iconColor = type === 'danger' ? 'red' : 'green';
        const badgeClass = type === 'danger' ? 'badge-danger' : 'badge-info';
        const badgeText  = type === 'danger' ? 'Darurat' : 'Info';
        const titleColor = type === 'danger' ? 'var(--red)' : 'var(--text-primary)';

        const html = `
            <div class="notification-item">
                <div class="notif-content">
                    <div class="notif-icon ${iconColor}">${icon}</div>
                    <div class="notif-text">
                        <h4 style="color: ${titleColor}">${title}</h4>
                        <p>${new Date().toLocaleDateString('id-ID', {day:'2-digit', month:'short'})} - ${timeStr}</p>
                    </div>
                </div>
                <div class="notif-badge ${badgeClass}">${badgeText}</div>
            </div>`;
        notificationList.insertAdjacentHTML('afterbegin', html);
    }

    // =============================================
    // 6. UPDATE UI DENGAN DATA DARI FIREBASE
    // =============================================
    function updateUI(suhu, kelembapan, apiTerdeteksi, statusKipas, statusAlarm) {
        const timeStr = getTimeStr();

        // Update Suhu
        const safeTemp = Math.max(0, Math.min(60, suhu));
        tempValue.innerText = suhu.toFixed(1) + '°C';
        humValue.innerText  = kelembapan.toFixed(1) + '%';
        fireLastChecked.innerText = timeStr;
        syncTerakhir.innerText    = getShortTimeStr();

        // Update Circular Progress
        const deg   = (safeTemp / 60) * 360;
        const color = apiTerdeteksi ? 'var(--red)' : (safeTemp >= 32 ? 'var(--orange)' : 'var(--green)');
        tempProgress.style.background = `conic-gradient(${color} ${deg}deg, var(--border-color) 0deg)`;
        tempIcon.style.color = color;

        // Update Chart
        const chartLabel = timeStr;
        if (chartLabels.length >= MAX_CHART_POINTS) {
            chartLabels.shift();
            chartData.shift();
        }
        chartLabels.push(chartLabel);
        chartData.push(suhu);
        tempChart.update('none');

        // Update Data Points Counter
        totalDataPoints++;
        dataPointsEl.innerText = totalDataPoints;

        // --- FIRE / DANGER STATE ---
        if (apiTerdeteksi) {
            fireBanner.className     = 'status-banner danger';
            fireShieldIcon.className = 'fas fa-fire';
            fireTitle.innerText      = 'BAHAYA! API TERDETEKSI';
            fireDesc.innerText       = 'Status: Sistem Alarm Aktif';
            fireAlert.classList.remove('hidden');

            // Log notifikasi hanya saat pertama kali bahaya
            if (lastDangerState !== true) {
                addNotification('danger', 'BAHAYA! Api terdeteksi di ruangan!');
                lastDangerState = true;
            }
        } else {
            fireBanner.className     = 'status-banner safe';
            fireShieldIcon.className = 'fas fa-shield-alt';
            fireTitle.innerText      = 'AMAN';
            fireDesc.innerText       = 'Status: Normal';
            fireAlert.classList.add('hidden');

            if (lastDangerState === true) {
                addNotification('info', 'Kondisi ruangan kembali normal.');
                lastDangerState = false;
            }
        }

        // --- SYNC STATUS TOGGLE DARI ALAT (read-only dari Status) ---
        toggleKipas.checked = statusKipas;
        toggleAlarm.checked = statusAlarm;
    }

    // =============================================
    // 7. KONEKSI FIREBASE (tunggu Firebase siap)
    // =============================================
    function initFirebase() {
        const db       = window.firebaseDB;
        const ref      = window.firebaseRef;
        const onValue  = window.firebaseOnValue;
        const set      = window.firebaseSet;

        if (!db) {
            console.error('Firebase belum siap!');
            return;
        }

        // Status NodeMCU: jika ada data Sensor masuk berarti terhubung
        statusNodeMCU.innerText = 'Terhubung';
        statusNodeMCU.className = 'stat-value green-text';

        // Tambah notif awal
        addNotification('info', 'Sistem Smart Room Safety terhubung ke Firebase.');

        // ============================
        // LISTENER: Baca data Sensor
        // ============================
        const sensorRef = ref(db, '/Sensor');
        onValue(sensorRef, (snapshot) => {
            const data = snapshot.val();
            if (!data) return;

            const suhu        = parseFloat(data.Suhu)      || 0;
            const kelembapan  = parseFloat(data.Kelembapan) || 0;
            const apiDeteksi  = Boolean(data.Api);

            // Baca Status Perangkat
            const statusRef = ref(db, '/Status');
            // Gunakan onValue sekali saja untuk status sinkron
            onValue(statusRef, (statusSnap) => {
                const st = statusSnap.val() || {};
                updateUI(suhu, kelembapan, apiDeteksi, Boolean(st.Kipas), Boolean(st.Alarm));
            }, { onlyOnce: true });
        });

        // ============================
        // KIRIM KONTROL KE FIREBASE
        // ============================
        toggleKipas.addEventListener('change', () => {
            set(ref(db, '/Kontrol/Kipas'), toggleKipas.checked);
        });

        toggleAlarm.addEventListener('change', () => {
            set(ref(db, '/Kontrol/Alarm'), toggleAlarm.checked);
        });
    }

    // Tunggu Firebase module selesai load
    if (window.firebaseReady) {
        initFirebase();
    } else {
        window.addEventListener('firebaseReady', initFirebase);
    }

    // =============================================
    // 8. UPTIME COUNTER (lokal, bukan dari Firebase)
    // =============================================
    setInterval(() => {
        uptimeMinutes++;
        const h = Math.floor(uptimeMinutes / 60);
        const m = uptimeMinutes % 60;
        uptimeEl.innerText = `${h}h ${m}m`;
    }, 60000);

});
