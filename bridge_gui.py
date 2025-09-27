import paho.mqtt.client as mqtt
import tkinter as tk
from tkinter import ttk
import threading
import json

#---------------------------------------GLOBAL DEFINE-----------------------------------------
# Thresholds from bridge_subscriber.c
TEMPERATURE_THRESHOLD = 60.0
STRAIN_THRESHOLD = 0.005
VIBRATION_THRESHOLD = 2.5
ULTRASONIC_THRESHOLD = 2.0  # Alert if < 2.0

# Initial states
temperature_data = {"value": 0.0, "color": "#4CAF50", "device_id": "N/A"}
strain_data = {"value": 0.0, "color": "#4CAF50", "device_id": "N/A"}
vibration_data = {"value": 0.0, "color": "#4CAF50", "device_id": "N/A"}
ultrasonic_data = {"value": 0.0, "color": "#4CAF50", "device_id": "N/A"}

broker_address = "127.0.0.1"
port = 1883

# Styles for progress bars
TEMPERATURE_STYLE = "temp.Horizontal.TProgressbar"
STRAIN_STYLE = "strain.Horizontal.TProgressbar"
VIBRATION_STYLE = "vib.Horizontal.TProgressbar"
ULTRASONIC_STYLE = "ultra.Horizontal.TProgressbar"

# Max values for progress bars
TEMP_MAX = 100.0
STRAIN_MAX = 0.01
VIBRATION_MAX = 5.0
ULTRASONIC_MAX = 10.0

canvas = None
temperature_bar = None
strain_bar = None
vibration_bar = None
ultrasonic_bar = None
style = None
root = None

# Global labels for device info
temp_label = None
strain_label = None
vibration_label = None
ultrasonic_label = None

ALERT_TOPIC = "bridges/mostA/actuators/alert"

#--------------------------------------UI FUNCTIONS---------------------------------------
def update_ui():
    global root
    # Update progress bars
    fill_progress_bar(temperature_bar, temperature_data["color"], temperature_data["value"], TEMPERATURE_STYLE, TEMP_MAX)
    fill_progress_bar(strain_bar, strain_data["color"], strain_data["value"] * 1000, STRAIN_STYLE, STRAIN_MAX * 1000)
    fill_progress_bar(vibration_bar, vibration_data["color"], vibration_data["value"], VIBRATION_STYLE, VIBRATION_MAX)
    fill_progress_bar(ultrasonic_bar, ultrasonic_data["color"], ultrasonic_data["value"], ULTRASONIC_STYLE, ULTRASONIC_MAX)
    
    # Update labels
    if temp_label:
        temp_label.config(text=f"Temperature: {temperature_data['value']:.2f}°C from {temperature_data['device_id']}")
    if strain_label:
        strain_label.config(text=f"Strain: {strain_data['value']:.4f} from {strain_data['device_id']}")
    if vibration_label:
        vibration_label.config(text=f"Vibration: {vibration_data['value']:.2f} from {vibration_data['device_id']}")
    if ultrasonic_label:
        ultrasonic_label.config(text=f"Ultrasonic: {ultrasonic_data['value']:.2f} from {ultrasonic_data['device_id']}")
    
    root.after(500, update_ui)

def fill_progress_bar(progress_bar, color, value, style_name, max_val):
    global style
    progress_bar["maximum"] = max_val
    progress_bar["value"] = value
    style.configure(style_name, foreground=color, background="#B0BEC5")  # Light grey background

def configure_screen():
    global canvas, temperature_bar, strain_bar, vibration_bar, ultrasonic_bar, style, root, temp_label, strain_label, vibration_label, ultrasonic_label
    
    root = tk.Tk()
    root.title("Bridge Monitoring Dashboard")
    root.geometry("650x600")
    root.configure(bg="#212121")  # Dark theme background

    # Modern style configuration
    style = ttk.Style()
    style.theme_use('clam')  # Cleaner theme
    style.configure("Custom.TFrame", background="#424242")  # Dark grey frame
    style.configure("Custom.TLabel", background="#424242", foreground="#E0E0E0", font=("Helvetica", 12))
    style.configure(TEMPERATURE_STYLE, thickness=10, troughcolor="#616161", background="#B0BEC5")
    style.configure(STRAIN_STYLE, thickness=10, troughcolor="#616161", background="#B0BEC5")
    style.configure(VIBRATION_STYLE, thickness=10, troughcolor="#616161", background="#B0BEC5")
    style.configure(ULTRASONIC_STYLE, thickness=10, troughcolor="#616161", background="#B0BEC5")

    # Title
    title_label = tk.Label(root, text="Bridge Monitoring System", font=("Helvetica", 18, "bold"), bg="#212121", fg="#BBDEFB")
    title_label.pack(pady=(20, 10))

    # Main content frame
    main_frame = ttk.Frame(root, style="Custom.TFrame", padding="20")
    main_frame.pack(expand=True, fill="both")

    # Temperature Section
    temp_frame = ttk.Frame(main_frame, style="Custom.TFrame", padding="10")
    temp_frame.pack(fill="x", pady=5)
    ttk.Label(temp_frame, text="Temperature (°C):", style="Custom.TLabel").pack(side="left")
    temp_label = ttk.Label(temp_frame, text="Temperature: 0.00°C from N/A", style="Custom.TLabel")
    temp_label.pack(side="left", padx=10)
    temperature_bar = ttk.Progressbar(temp_frame, style=TEMPERATURE_STYLE, length=400, maximum=TEMP_MAX)
    temperature_bar.pack(side="left", padx=10)

    # Strain Section
    strain_frame = ttk.Frame(main_frame, style="Custom.TFrame", padding="10")
    strain_frame.pack(fill="x", pady=5)
    ttk.Label(strain_frame, text="Strain:", style="Custom.TLabel").pack(side="left")
    strain_label = ttk.Label(strain_frame, text="Strain: 0.0000 from N/A", style="Custom.TLabel")
    strain_label.pack(side="left", padx=10)
    strain_bar = ttk.Progressbar(strain_frame, style=STRAIN_STYLE, length=400, maximum=STRAIN_MAX * 1000)
    strain_bar.pack(side="left", padx=10)

    # Vibration Section
    vib_frame = ttk.Frame(main_frame, style="Custom.TFrame", padding="10")
    vib_frame.pack(fill="x", pady=5)
    ttk.Label(vib_frame, text="Vibration:", style="Custom.TLabel").pack(side="left")
    vibration_label = ttk.Label(vib_frame, text="Vibration: 0.00 from N/A", style="Custom.TLabel")
    vibration_label.pack(side="left", padx=10)
    vibration_bar = ttk.Progressbar(vib_frame, style=VIBRATION_STYLE, length=400, maximum=VIBRATION_MAX)
    vibration_bar.pack(side="left", padx=10)

    # Ultrasonic Section
    ultra_frame = ttk.Frame(main_frame, style="Custom.TFrame", padding="10")
    ultra_frame.pack(fill="x", pady=5)
    ttk.Label(ultra_frame, text="Ultrasonic Distance:", style="Custom.TLabel").pack(side="left")
    ultrasonic_label = ttk.Label(ultra_frame, text="Ultrasonic: 0.00 from N/A", style="Custom.TLabel")
    ultrasonic_label.pack(side="left", padx=10)
    ultrasonic_bar = ttk.Progressbar(ultra_frame, style=ULTRASONIC_STYLE, length=400, maximum=ULTRASONIC_MAX)
    ultrasonic_bar.pack(side="left", padx=10)

    # Status Canvas with modern indicators
    canvas = tk.Canvas(root, width=600, height=120, bg="#424242", highlightthickness=0)
    canvas.pack(pady=20)

    # Create modern status indicators (rectangles with rounded edges)
    canvas.create_rectangle(50, 20, 140, 80, fill="#4CAF50", outline="#424242", tags="temp_rect")
    canvas.create_text(95, 50, text="Temp OK", fill="white", font=("Helvetica", 10))

    canvas.create_rectangle(160, 20, 250, 80, fill="#4CAF50", outline="#424242", tags="strain_rect")
    canvas.create_text(205, 50, text="Strain OK", fill="white", font=("Helvetica", 10))

    canvas.create_rectangle(270, 20, 360, 80, fill="#4CAF50", outline="#424242", tags="vib_rect")
    canvas.create_text(315, 50, text="Vib OK", fill="white", font=("Helvetica", 10))

    canvas.create_rectangle(380, 20, 470, 80, fill="#4CAF50", outline="#424242", tags="ultra_rect")
    canvas.create_text(425, 50, text="Ultra OK", fill="white", font=("Helvetica", 10))

def update_circles():
    # Temperature indicator
    if temperature_data["value"] > TEMPERATURE_THRESHOLD:
        canvas.itemconfig("temp_rect", fill="#F44336")
        canvas.itemconfig(canvas.find_withtag("temp_rect")[0] + 1, text="Temp ALERT")
    else:
        canvas.itemconfig("temp_rect", fill="#4CAF50")
        canvas.itemconfig(canvas.find_withtag("temp_rect")[0] + 1, text="Temp OK")
    
    # Strain indicator
    if strain_data["value"] > STRAIN_THRESHOLD:
        canvas.itemconfig("strain_rect", fill="#F44336")
        canvas.itemconfig(canvas.find_withtag("strain_rect")[0] + 1, text="Strain ALERT")
    else:
        canvas.itemconfig("strain_rect", fill="#4CAF50")
        canvas.itemconfig(canvas.find_withtag("strain_rect")[0] + 1, text="Strain OK")
    
    # Vibration indicator
    if vibration_data["value"] > VIBRATION_THRESHOLD:
        canvas.itemconfig("vib_rect", fill="#F44336")
        canvas.itemconfig(canvas.find_withtag("vib_rect")[0] + 1, text="Vib ALERT")
    else:
        canvas.itemconfig("vib_rect", fill="#4CAF50")
        canvas.itemconfig(canvas.find_withtag("vib_rect")[0] + 1, text="Vib OK")
    
    # Ultrasonic indicator
    if ultrasonic_data["value"] < ULTRASONIC_THRESHOLD:
        canvas.itemconfig("ultra_rect", fill="#F44336")
        canvas.itemconfig(canvas.find_withtag("ultra_rect")[0] + 1, text="Ultra ALERT")
    else:
        canvas.itemconfig("ultra_rect", fill="#4CAF50")
        canvas.itemconfig(canvas.find_withtag("ultra_rect")[0] + 1, text="Ultra OK")
    
    root.after(500, update_circles)

#----------------------------MQTT FUNCTIONS-------------------------------------------
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("GUI: Connected to MQTT Broker.")
    else:
        print(f"GUI: Error connecting with code: {rc}")
        return 
    
    client.subscribe(ALERT_TOPIC)
    print(f"GUI: Subscribed to topic: {ALERT_TOPIC}")
    
def on_message(client, userdata, msg):
    try: 
        json_message = msg.payload.decode('utf-8')
        data = json.loads(json_message)
        
        device_id = data.get("device_id", "N/A")
        sensor_type = data.get("sensor_type", "")
        value = float(data.get("value", 0.0))
        
        # Update specific sensor data
        if sensor_type == "temperature":
            temperature_data["value"] = value
            temperature_data["device_id"] = device_id
            temperature_data["color"] = "#F44336" if value > TEMPERATURE_THRESHOLD else "#4CAF50"
        elif sensor_type == "strain":
            strain_data["value"] = value
            strain_data["device_id"] = device_id
            strain_data["color"] = "#F44336" if value > STRAIN_THRESHOLD else "#4CAF50"
        elif sensor_type == "vibration":
            vibration_data["value"] = value
            vibration_data["device_id"] = device_id
            vibration_data["color"] = "#F44336" if value > VIBRATION_THRESHOLD else "#4CAF50"
        elif sensor_type == "ultrasonic":
            ultrasonic_data["value"] = value
            ultrasonic_data["device_id"] = device_id
            ultrasonic_data["color"] = "#F44336" if value < ULTRASONIC_THRESHOLD else "#4CAF50"
        else:
            print(f"GUI: Unknown sensor type: {sensor_type}")
        
        print(f"GUI: Updated {sensor_type} from {device_id}: {value}")
        
    except json.JSONDecodeError:
        print("GUI: Error parsing JSON message.")
    except Exception as e:
        print(f"GUI: Unexpected error: {e}")

def mqtt_thread():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(broker_address, port, 60)
    client.loop_forever()

if __name__ == "__main__":
    configure_screen()
    update_ui()
    update_circles()
    threading.Thread(target=mqtt_thread, daemon=True).start()
    root.mainloop()
       