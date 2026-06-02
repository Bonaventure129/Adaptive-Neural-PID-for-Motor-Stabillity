Edge AI Motor Controller: Stability and Performance Evaluation

📌 Project Overview

This repository contains the source code and methodology for the design, integration, and evaluation of an Adaptive Neural-PID Controller using a Hardware-in-the-Loop (HIL) edge computing architecture.

Traditional fixed-gain PID controllers often fail to maintain stability in dynamic environments with physical load disturbances, stiction, and varying friction. To overcome this, a Multi-Layer Perceptron (MLP) Artificial Neural Network was trained offline using TensorFlow and deployed directly onto an embedded microcontroller (ESP32-S3) for real-time adaptive tuning (10Hz closed-loop frequency) and online learning.

✨ Key Features

Real-Time Edge Inference: Native C++ neural network feedforward matrix multiplication deployed directly on the ESP32-S3.

Online Retraining (Backpropagation): The edge device dynamically updates its own neural network weights in RAM to adapt to new physical disturbances.

1-Dimensional Kalman Filtering: Optimal state estimation implemented in C++ to mitigate encoder quantization error and analog sensor noise with near-zero phase lag.

Live Telemetry Web Dashboard: An HTML5 Canvas UI hosted locally on the ESP32 (http://aimotor.local) via WebSockets for real-time tracking, live graph plotting, and AI toggling.

Anti-Windup Safety Bounds: Integrated learning deadbands and absolute physical clamping constraints to guarantee mathematical stability during prolonged execution.

🛠 Hardware Architecture

Microcontroller: ESP32-S3 (Dual-core processor handling inference, control loops, and Wi-Fi Access Point).

Power Supply: 3S Li-ion Battery Pack (11.1V) with a 3S Battery Management System (BMS) and a 220µF Electrolytic Capacitor for transient spike decoupling.

Actuator & Driver: JGA25-370 geared DC motor (12V, 1:65, 11 PPR) driven by an L298N Dual H-Bridge.

Sensors: Quadrature Hall-effect Encoder (Velocity) and INA219 I2C Current Sensor (Torque/Load).

Peripherals: MicroSD Card Module (SPI data logging) and SSD1306 OLED (I2C local telemetry).

💻 Installation & Setup Guide

Phase 1: Python Environment (Machine Learning)

TensorFlow 2.15.0 requires Python 3.10 or 3.11. Please avoid Python 3.12+ as it may lack pre-built native binaries.

For Windows Users

Install Python: Download Python 3.11 from python.org. Critical: Check the box that says "Add python.exe to PATH" during installation.

Install C++ Redistributables: TensorFlow on Windows requires the latest Microsoft Visual C++ Redistributables. Download and install the x64 version.

Open Terminal (VS Code or Command Prompt) and Create Environment:

python -m venv env
.\env\Scripts\activate


Install Packages:

pip install numpy pandas tensorflow==2.15.0 scikit-learn matplotlib


For Linux Users (Ubuntu/Debian)

Install Python and Venv:

sudo apt update
sudo apt install python3.11 python3.11-venv python3-pip


Create and Activate Environment:

python3 -m venv env
source env/bin/activate


Install Packages:

pip install numpy pandas tensorflow==2.15.0 scikit-learn matplotlib


Phase 2: Arduino IDE Setup (Edge Deployment)

This is the same for both Windows and Linux users.

Download and install Arduino IDE 2.x.

Go to File > Preferences and add the ESP32 board URL:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Go to the Boards Manager (left sidebar) and install esp32 by Espressif Systems.

Go to the Library Manager (left sidebar) and install the following libraries:

Adafruit GFX Library

Adafruit SSD1306

Adafruit INA219

🚀 Usage Guide

1. Training the AI Model (Host PC)

Connect to the ESP32 network (AI_Motor_Network), open the web dashboard, and download your physical telemetry data (motor_data.csv).

Place the CSV file in the python_training/ folder.

Activate your Python virtual environment and run the training pipeline:

python train_ai.py


(This calculates ideal PID responses and trains the Neural Network).

Export the weights to C++:

python export_model.py


(This dynamically generates the ai_model.h file containing the optimized network matrices and safety limits).

2. Flashing the Edge Device (ESP32)

Move the newly generated ai_model.h file into the arduino_firmware/ directory.

Open main.ino in the Arduino IDE.

Select ESP32S3 Dev Module from the boards dropdown.

Click Upload to flash the new AI brain into the microcontroller.

📊 Results & Performance Evaluation

The system was rigorously evaluated by comparing the classical Static PID controller against the Adaptive Neural-PID under identical setpoint steps and induced physical load disturbances (stiction and induced friction). The primary stability metric evaluated was the Integral Time Absolute Error (ITAE).

Static PID Performance: Suffered from actuator saturation, exceedingly slow rise times, and massive steady-state errors under load. (ITAE Score: 2,395,538.0)

Adaptive Neural-PID Performance: Exhibited near-instantaneous disturbance rejection. As load spiked, the Neural Network dynamically predicted and surged the proportional and integral gains to force the motor to maintain the setpoint. (ITAE Score: 535,713.6)

(See docs/Figure_3_ITAE_Comparison.png for visual data comparison).

Conclusion: The Hardware-in-the-Loop edge AI controller successfully improved the overall mathematical stability and performance of the DC motor system by 77.6%.

👨‍💻 Author

Ugbah Awele Bonaventure

Mechatronics Engineering, Federal University of Technology Owerri (FUTO)

LinkedIn Profile | GitHub Profile
https://www.linkedin.com/in/awele-bonaventure-ugbah-56a729263 | https://github.com/Bonaventure129/
