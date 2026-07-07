# Weather Data Collector

A local weather data collector that aims to be low-power and self-sustaining. It utilizes the **BME280** and **DHT22** sensors to provide moderately accurate temperature, pressure, and humidity readings. Those readings are sent in batches in configurable intervals to a locally hosted flask server that pushes them to a sqlite database and can be accessed later on for data processing.


## Wiring Diagram
<p align="center">
    <img src="./schematic.jpg" width=700>
    </br>
    <small>Seeed XIAO ESP32 C3 is used in the diagram instead of the supermini.</small>
</p>

Note that an array of solar panels are wired to the TP4056 charging module input.