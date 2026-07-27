const canvLatest = document.getElementById("canv-latest");
const canvLatestH = document.getElementById("canv-latest-hour");
const canvLatestPres = document.getElementById("canv-latest-pressure");
const canvLatestBatt = document.getElementById("canv-latest-voltage");
const readings_per_hour = 7;
const clamp = (num, min, max) => Math.min(Math.max(num, min), max);

async function fetchLatest(n) {
    const resp = await fetch(`/get-latest?n=${n}`);
    if (!resp.ok) return null;

    return await resp.json();
}

function fmtDate(timestamp) {
    const d = new Date(timestamp * 1000);

    return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

function calcSoc(v) {
    const v_cutoff = 2.5;
    const v_full = 4.2;
    return ((clamp(v, v_cutoff, v_full) - v_cutoff) / (v_full - v_cutoff)) * 100
}

document.addEventListener("DOMContentLoaded", async () => {
    var latestData = await fetchLatest(readings_per_hour * 24);
    latestData = latestData.reverse();
    var latestDataH = latestData.slice(latestData.length - readings_per_hour * 1, -1);

    new Chart(canvLatestH, {
        type: "line",
        data: {
            labels: latestDataH.map((r) => fmtDate(r.timestamp)),
            datasets: [
                {
                    label: "Temperature (C)",
                    data: latestDataH.map((r) => r.temp1),
                },
                {
                    label: "Humidity (%)",
                    data: latestDataH.map((r) => r.humidity),
                },
                {
                    label: "Heat Index (C)",
                    data: latestDataH.map((r) => r.heat_index),
                },
            ],
        },
        options: {
            scales: {
                y: {
                    beginAtZero: false,
                },
            },
        },
    });

    new Chart(canvLatest, {
        type: "line",
        data: {
            labels: latestData.map((r) => fmtDate(r.timestamp)),
            datasets: [
                {
                    tension: 0.5,
                    label: "Temperature (C)",
                    data: latestData.map((r) => r.temp1),
                    yAxisID: 'y',
                },
                {
                    tension: 0.5,
                    label: "Humidity (%)",
                    data: latestData.map((r) => r.humidity),
                    yAxisID: 'y',
                },
                {
                    tension: 0.5,
                    label: "Heat Index (C)",
                    data: latestData.map((r) => r.heat_index),
                    yAxisID: 'y',
                },
                {
                    tension: 0.5,
                    label: "Pressure (mb)",
                    data: latestData.map((r) => r.pressure),
                    yAxisID: 'y1',
                },
            ],
        },
        options: {
            scales: {
                y: {
                    beginAtZero: false,
                    ticks: {
                        callback: function (value, index, ticks) {
                            (0.0).toP;
                            return `${value}`;
                        },
                    },
                },
                y1: {
                    beginAtZero: false,
                    position: "right",
                    ticks: {
                        callback: function (value, index, ticks) {
                            return `${value}mb`;
                        },
                    },
                    grid: {
                        drawOnChartArea: false
                    }
                },
            },
        },
    });

    new Chart(canvLatestBatt, {
        type: "line",
        data: {
            labels: latestData.map((r) => fmtDate(r.timestamp)),
            datasets: [
                {
                    tension: 0.5,
                    label: "Battery %",
                    data: latestData.map(
                        (r) => calcSoc(r.battery),
                    ),
                    yAxisID: "y",
                },
                {
                    tension: 0.5,
                    label: "Temperature (C)",
                    data: latestData.map((r) => (r.temp1 + r.temp2) / 2.0),
                    yAxisID: "y1",
                },
            ],
        },
        options: {
            scales: {
                y: {
                    beginAtZero: false,
                    ticks: {
                        callback: function (value, index, ticks) {
                            (0.0).toP;
                            return `${value.toPrecision(3)}%`;
                        },
                    },
                },
                y1: {
                    beginAtZero: false,
                    position: "right",
                    ticks: {
                        callback: function (value, index, ticks) {
                            return `${value}C`;
                        },
                    },
                },
            },
        },
    });
});
