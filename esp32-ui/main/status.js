// Shared live-status bar for converter.html and calibrate.html.
// Expects a <div id="statusBar"></div> in the page
// polls GET /status once a second and fills it in

(function() {
    const el = document.getElementById('statusBar');
    if (!el) return;

    // Uptime in h/m/s format
    function formatUptime(ms) {
        const totalSec = Math.floor(ms / 1000);
        const h = Math.floor(totalSec / 3600);
        const m = Math.floor((totalSec % 3600) / 60);
        const s = totalSec % 60;
        return `${h}h ${m}m ${s}s`;
    }

    // Poll status each second and update status line
    async function pollStatus() {
        try {
            const res = await fetch('/status');
            const data = await res.json();
            if (data.linked) {
                el.textContent = `RPM: ${data.rpm.toFixed(1)} | RP2350 uptime: ${formatUptime(data.rp2350_uptime_ms)}`;
                el.classList.remove('offline'); // To change colour
            } else {
                el.textContent = 'Link: no response from RP2350';
                el.classList.add('offline');
            }
        } catch (err) {
            el.textContent = `Link: failed (${err.message})`;
            el.classList.add('offline');
        }
    }

    pollStatus();
    setInterval(pollStatus, 1000);
})();
