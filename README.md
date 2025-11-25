# BK Smart Garden - Intelligent Plant Care System

<div align="center">
  <img src="https://github.com/user-attachments/assets/748f693c-11b9-4a70-a34f-51f965948353" width="600" alt="Hydroponics Drip System Illustration">
  <br><br>
  <em>Hydroponics Drip System – An intelligent watering solution for modern gardens (Source: Shutterstock)</em>
</div>

<br>

<div align="center">
  <img src="https://github.com/user-attachments/assets/58b7dbe4-033b-41d2-bda7-c21b9eeb07b3" width="1726" alt="Auto mode UI">
  <br><br>
  <em>Auto Mode Control UI for local web server and public web server</em>
</div>

<br>

<div align="center">
  <img src="https://github.com/user-attachments/assets/9b28d8a6-a34f-4c09-be08-ad872b96eed0" width="1681" alt="manual mode UI">
  <br><br>
  <em>Manual Control UI for local web server and public web server</em>
</div>

<br>

<div align="center">
  <img src="https://github.com/user-attachments/assets/4a6d4efa-c265-4310-9897-9f805a4c42df" width="1681" alt="manual mode UI">
  <br><br>
  <em>Setting UI for local web server and public web server</em>
</div>

<br>

<div align="center">
  <img src="https://github.com/user-attachments/assets/1e680ec3-7964-4551-b896-5947488b85ef" width="1681" alt="manual mode UI">
  <br><br>
  <em>User manual on local web server and public web server</em>
</div>

## Overview

Welcome to the **BK Smart Garden** project! This is a comprehensive IoT solution designed to help you monitor and care for your garden automatically, intelligently, and effortlessly directly from your phone or computer.

## Project Goals

This project was developed to address common issues in home gardening:

- **Forgetting to water**: The system automatically waters when the soil is dry.
- **Overwatering**: Soil moisture sensors help regulate the appropriate amount of water.
- **Remote monitoring difficulties**: Monitor environmental temperature and humidity anytime, anywhere.
- **Complex connectivity**: Intelligent WiFi setup mechanism with auto-reconnect capabilities.

## Key Features

### 1. Real-time Environmental Monitoring
- Displays Air Temperature (°C).
- Displays Air Humidity (%).
- Displays Soil Moisture (%).
- Pump operation status (ON/OFF).

### 2. Flexible Control Modes
- **Auto Mode**: The system automatically decides when to water based on configured thresholds (e.g., water when soil moisture is below 30% AND temperature is above 30°C).
- **Manual Mode**: Users actively toggle the pump ON/OFF via the Web interface or control App.

### 3. Smart Connectivity & Synchronization
- **WiFi Manager**: Automatically broadcasts WiFi (AP Mode) for users to configure credentials upon initial startup or connection loss. While in AP mode, it features an automatic retry mechanism to connect to previously configured WiFi networks, handling intermittent or temporary connection drops.
- **Web Dashboard**: A visually appealing, intuitive local Web interface (Local), eliminating the need for app installation.
- **Cloud Sync (MQTT)**: Connects to HiveMQ Cloud for remote monitoring and control via the Internet. Data is synchronized bi-directionally in real-time.

## User Guide

### 1. Initial Setup (Provisioning)

When starting the device for the first time or changing WiFi networks:

1. Find and connect to the WiFi network named: **ESP32_Config_Wifi** (Password: 12345678).
2. The browser will automatically open the setup page: http://smartgarden.local/ (or access **192.168.4.1**).
3. Click **Scan** to find your home WiFi network.
4. Select your WiFi network, enter the password, and click **Connect**.
5. The device will automatically connect and switch to normal operation mode.

### 2. Monitoring & Control

After a successful connection, you can access the device's IP address (displayed on the setup screen or found in your Router) to enter the Main Dashboard.

- **Switch Modes**: Click the "Switch to Manual/Auto" button to change the operating mode.
- **Pump Control**: In Manual mode, click "Toggle Pump" to turn the pump on/off.
- **Auto Configuration**: Click "Auto Mode Settings" to set pump activation thresholds (Temperature, Humidity, Soil).

### 3. Status Indicators (On Device)
- **Light On**: Pump is running (Relay closed).
- **Light Off**: Pump is stopped (Relay open).

## System Requirements

- **Hardware**: ESP32 Development Board, DHT11 Sensor, Soil Moisture Sensor, Relay Module, Mini Pump.
- **Software**: Any web browser (Chrome, Safari, Firefox) on mobile or desktop.

## Contact & Support

This project was developed by a student from Ho Chi Minh City University of Technology - Le Loc Quoc Thinh. For any questions or feedback, please contact via email: **thinhle.hardware@gmail.com**.

**BK Smart Garden - Bringing technology to every sprout.**
