import serial
import json
import time

# Change this if your Arduino appears on another port.
SERIAL_PORT = "/dev/ttyACM0"
BAUD_RATE = 115200

print("SMART REFRIGERATOR - RASPBERRY PI")
print("----------------------------------")

while True:
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2) as arduino:
            time.sleep(2)
            print("Arduino connected successfully!\n")

            while True:
                line = arduino.readline().decode(
                    "utf-8", errors="ignore"
                ).strip()

                if not line:
                    continue

                try:
                    data = json.loads(line)

                    if "temperature" not in data:
                        print(data)
                        continue

                    print("\n========== REFRIGERATOR ==========")
                    print("Temperature :", data.get("temperature"), "°C")
                    print("Humidity    :", data.get("humidity"), "%")
                    print("Door        :", data.get("door"))
                    print("Motion      :", data.get("motion"))
                    print("Distance    :", data.get("distance"), "cm")
                    print("Obstacle    :", data.get("obstacle"))
                    print("LED / Relay :", data.get("relay"))
                    print("Buzzer      :", data.get("buzzer"))
                    print("Temp Warning:", data.get("temperature_warning"))
                    print("==================================")

                except json.JSONDecodeError:
                    print("Arduino:", line)

    except serial.SerialException as error:
        print("Serial connection error:", error)
        print("Check USB cable and serial port.")
        time.sleep(3)

    except KeyboardInterrupt:
        print("\nProgram stopped.")
        break