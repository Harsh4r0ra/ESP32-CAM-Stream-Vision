# ESP32-CAM Stream Vision

A flexible real-time camera streaming system with OpenCV integration for computer vision projects. This repository contains both the ESP32-CAM firmware and Python client code for processing the camera stream.

## Features

- **Dual Network Operation**: Connect to a WiFi hotspot while maintaining fallback AP mode
- **Camera Stream Endpoint**: Access raw camera feed for computer vision projects
- **Web Dashboard**: Monitor sensors and camera feed through an intuitive interface
- **OpenCV Integration**: Python client for capturing and processing the video stream
- **REST API**: Access sensor data and system information programmatically
- **Fallback Mechanisms**: Automatic recovery from network interruptions

## Hardware Requirements

- ESP32-CAM module (AI-Thinker ESP32-CAM recommended)
- Optional: Additional sensors (DHT22, water level sensor, etc.)
- A computer or mobile device to view the stream and run OpenCV code

## Repository Structure

```
ESP32-CAM-Stream-Vision/
├── firmware/
│   └── ESP32_CAM_Dual_Network/
│       └── ESP32_CAM_Dual_Network.ino
├── client/
│   ├── esp32_opencv.py
│   └── advanced_processing.py
├── docs/
│   ├── images/
│   └── api_reference.md
└── README.md
```

## Getting Started

### 1. Upload the Firmware

1. Open `firmware/ESP32_CAM_Dual_Network/ESP32_CAM_Dual_Network.ino` in the Arduino IDE
2. Update the WiFi credentials:

```cpp
// WiFi client mode settings (your hotspot credentials)
const char* wifi_ssid = "YOUR_HOTSPOT_SSID";
const char* wifi_password = "YOUR_HOTSPOT_PASSWORD";
```

3. Select the correct board (ESP32 AI-Thinker CAM)
4. Upload the code to your ESP32-CAM

### 2. Access the Web Interface

The ESP32-CAM creates its own access point by default, but will also try to connect to your specified network:

- **Access Point Mode**: Connect to the WiFi network `ESP32-CAM-AP` with password `12345678`
- **Client Mode**: The ESP32-CAM will also connect to your specified WiFi network

Navigate to one of these addresses in your web browser:
- AP Mode: `http://192.168.4.1`
- Client Mode: Check the Serial Monitor output for the assigned IP address

### 3. Run the Python Client

The Python client requires OpenCV and can capture and process the camera stream:

```bash
# Install dependencies
pip install opencv-python numpy

# Run the client
python client/esp32_opencv.py --ip 192.168.X.X  # Replace with your ESP32-CAM's IP
```

## Available Endpoints

The ESP32-CAM provides several HTTP endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main dashboard with JavaScript-based updates |
| `/auto` | GET | Simple dashboard with meta tag auto-refresh (3s) |
| `/test` | GET | Minimal test page with auto-refresh |
| `/stream` | GET | MJPEG camera stream (for browsers and OpenCV) |
| `/sensor-data` | GET | JSON API with current sensor readings |
| `/update-sensor` | POST | Endpoint to update sensor data |
| `/network-status` | GET | Network information page |

## OpenCV Integration

The Python client demonstrates basic image processing with OpenCV:

- Edge detection
- Frame manipulation
- Basic computer vision

Modify `process_frame()` in `esp32_opencv.py` to implement your own computer vision algorithms.

## Example Projects

This platform can be used for various applications:

- Home security and surveillance
- Plant growth monitoring
- Water level detection
- Motion detection and tracking
- Object recognition (with additional ML models)

## Troubleshooting

- **Cannot connect to ESP32-CAM**: Try the AP mode first (192.168.4.1)
- **Stream is slow or unstable**: Reduce the frame size in the ESP32-CAM code
- **Camera not detected**: Check connections and power supply
- **Python client crashes**: Ensure OpenCV is installed correctly

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- ESP32-CAM examples by Espressif
- OpenCV library contributors
