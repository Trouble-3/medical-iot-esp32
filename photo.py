import serial
import cv2
import numpy as np
import os
import time

# --- НАСТРОЙКИ ---
PORT = 'COM6'
BAUD = 115200
# Назови лекарство перед началом съемки
MEDICINE_NAME = "background" 

# Создаем папки
save_path = f"dataset/{MEDICINE_NAME}"
if not os.path.exists(save_path):
    os.makedirs(save_path)

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Сборщик данных запущен! Порт: {PORT}")
    print(f"Сохраняю в: {save_path}")
    print("Нажми кнопку на ESP32, чтобы сделать кадр...")
except Exception as e:
    print(f"Ошибка: {e}")
    exit()

img_counter = 0

while True:
    line = ser.readline().decode(errors='ignore').strip()
    
    if line == "START_IMAGE":
        try:
            size_line = ser.readline().decode(errors='ignore').strip()
            image_size = int(size_line)
            
            raw_data = ser.read(image_size)
            nparr = np.frombuffer(raw_data, np.uint8)
            img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if img is not None:
                img_counter += 1
                filename = f"{save_path}/{MEDICINE_NAME}_{int(time.time())}.jpg"
                cv2.imwrite(filename, img)
                
                print(f"✅ Сохранено фото №{img_counter}: {filename}")
                
                # Отправляем на ESP32 подтверждение, чтобы ты видел успех
                ser.write(f"OK! Foto #{img_counter}\n".encode('utf-8'))
                
                # Показываем, что сняли
                cv2.imshow("Dataset Collector", img)
                cv2.waitKey(1)
        except Exception as e:
            print(f"Ошибка при приеме: {e}")

ser.close()
