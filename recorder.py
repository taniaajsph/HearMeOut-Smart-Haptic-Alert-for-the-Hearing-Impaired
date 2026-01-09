import serial
import wave

# Replace 'COM6' with your actual port
PORT = 'COM6' 
# CHANGED: Must match ESP32 speed!
BAUD = 460800 
FILENAME = "doorbell16.wav"
RECORD_SECONDS = 5
# CHANGED: Must match 8000 Hz
SAMPLE_RATE = 8000 

ser = serial.Serial(PORT, BAUD)
print(f"Connected to {PORT} at {BAUD} baud. Recording...")

frames = []
bytes_to_read = SAMPLE_RATE * 2 * RECORD_SECONDS
audio_data = ser.read(bytes_to_read)

with wave.open(FILENAME, 'wb') as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2) 
    wav_file.setframerate(SAMPLE_RATE)
    wav_file.writeframes(audio_data)

print(f"Saved: {FILENAME}")
ser.close()