document.addEventListener("DOMContentLoaded", () => {
    // Опитуємо сервер щосекунди
    setInterval(fetchWeatherData, 1000);
});

function fetchWeatherData() {
    fetch('/api/data')
        .then(response => {
            if (!response.ok) {
                throw new Error('Помилка мережі');
            }
            return response.json();
        })
        .then(data => {
            document.getElementById("temp").innerText = data.temperature.toFixed(1);
            document.getElementById("humidity").innerText = data.humidity.toFixed(1);
            document.getElementById("status").innerText = `Аптайм: ${data.uptime} сек | Статус: OK`;
        })
        .catch(error => {
            console.error('Не вдалося отримати дані:', error);
            document.getElementById("status").innerText = "Втрачено зв'язок з ESP8266...";
        });
}