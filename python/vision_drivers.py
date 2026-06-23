'''
 * ============================================================================
 * @file        vision_drivers.py
 *
 * @author      Marco Antônio Ranghetti
 * @github      github.com/mRangh
 * @email       marcoantonioranghetti@gmail.com
 * @academic    d2026008956@unifei.edu.br
 *
 * @version     1.0.0
 * @date        2026-06-22
 * @license     Apache License 2.0
 * ============================================================================
 '''

import cv2
import easyocr
import time
import random
import re
import hashlib
import io
import qrcode
import qrcode.constants
import numpy as np
from gate_client import GateClient

print('[PYTHON]: Starting Artificial Inteligence (EasyOCR).')
reader = easyocr.Reader(['en'], gpu=False)
print('[PYTHON]: AI ready to go.')

def cut_license_plate(img):
    print('[OPENCV]: Processing webcam image searching for license plates...')
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    blur = cv2.bilateralFilter(gray, 11, 17, 17)
    edged = cv2.Canny(blur, 30, 200)
    contours, _ = cv2.findContours(edged.copy(), cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[:10]
    plate_contour = None

    for c in contours:
        peri = cv2.arcLength(c, True)
        approx = cv2.approxPolyDP(c, 0.018 * peri, True)
    
        if len(approx) == 4:
            x, y, w, h = cv2.boundingRect(c)
            aspect_ratio = float(w) / h

            if aspect_ratio >= 2.2 and aspect_ratio <= 4.5:
                plate_contour = approx
                break
    
    if plate_contour is not None:
        x, y, w, h = cv2.boundingRect(plate_contour)

        margin = 5
        y_ini = max(0, y - margin)
        y_end = min(img.shape[0], y + h + margin)
        x_ini = max(0, x - margin)
        x_end = min(img.shape[1], x + w + margin)

        cropped_img = img[y_ini : y_end, x_ini : x_end]

        print('[OPENCV]: License plate found and cut applied.')
        return cropped_img
    else:
        print('[OPENCV_ERR]: License plate not found.')
        return None



def read_license_plate():
    frame_plate = None
    for attempt in range(10):
        print('[OPENCV]: Accessing webcam...')
        print(f'[OPENCV]: Reading attempt {attempt + 1}/10')
        cam = cv2.VideoCapture(0)

        if not cam.isOpened():
            print('[OPENCV]: Impossible to reach camera.')
            time.sleep(1)
            continue
        
        time.sleep(0.4)

        ret, frame = cam.read()
        cam.release()

        if not ret:
            print('[OPENCV_ERR]: Image capture failure.')
            time.sleep(0.5)
            continue
        
        print('[OPENCV]: Image captured.')

        frame_plate = cut_license_plate(frame)

        if frame_plate is not None:
            print(f'[OPENCV]: Plate located successfully on attempt {attempt + 1}!')
            cv2.imwrite('last_cropped_plate.jpg', frame_plate)
            break
        else:
            print("[OPENCV_WARN]: Plate not found, retring")
            time.sleep(0.3)

    if frame_plate is None:
        print('[OPENCV_ERR]: Failed to locate plate rectangle after all attempts.')
        return None

    print('[OCR]: Processing image text.')
    result = reader.readtext(
        frame_plate,
        detail=0,
        allowlist='ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789',
        )

    if result:
        raw_txt = ''.join(str(item) for item in result).upper().replace(' ', '')
        match = re.search(r'[A-Z]{3}[0-9][A-Z0-9][0-9]{2}', raw_txt)
        if match:
            plate_txt = match.group(0)
            return plate_txt
        else:
            return None


def generate_random_plate():
    letters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    numbers = '0123456789'
    plate = ''
    for i in range(3):
        plate += random.choice(letters)
    plate += random.choice(numbers)
    plate += random.choice(letters)
    for i in range(2):
        plate += random.choice(numbers)
    return plate



def hash_sha256(text):
    return hashlib.sha256(text.encode()).hexdigest()



def generate_qr_code(hash: str):
    qr = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=10,
        border=4,
    )

    qr.add_data(hash)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white").get_image()

    buffer = io.BytesIO()
    img.save(buffer, format="PNG")

    try:
        img_bytes = buffer.getvalue()
        nparr = np.frombuffer(img_bytes, np.uint8)
        qr_img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        if qr_img is not None:
            cv2.imshow("Ticket Estacionamento - ESCANEIE AQUI", qr_img)
            
            cv2.waitKey(0)
            # ATTENTION: On workbench test, .waitKey(0) works
            # but in real applications, it must be (1)
            cv2.destroyAllWindows()
        else:
            print("[QRCODE_ERR]: Could not read the generated image with OpenCV.")
    except Exception as e:
        print(f"[QRCODE_ERR]: Failed to display image via OpenCV: {e}")
    return buffer.getvalue()



def read_qr_code():
    print('[OPENCV]: Accessing webcam to read QR Code...')
    cam = cv2.VideoCapture(0)

    if not cam.isOpened():
        print('[OPENCV_ERR]: Impossible to reach camera for QR Code.')
        return None
    
    detector = cv2.QRCodeDetector()
    hash_detected = None

    for attempt in range(15):
        print(f'[OPENCV]: Scanning QR Code... Attempt {attempt + 1}/15')
        time.sleep(0.2)

        ret, frame = cam.read()
        if not ret:
            print('[OPENCV_ERR]: Frame capture failure.')
            continue

        data, bbox, _ = detector.detectAndDecode(frame)

        if bbox is not None and data:
            hash_detected = data
            print(f'[OPENCV_SUCCESS]: QR Code detected successfully!')
            break

    cam.release()

    if hash_detected:
        print(f"[OPENCV]: Raw text extracted from QR Code -> {hash_detected}")
        return hash_detected
    else:
        print("[OPENCV_WARN]: No valid QR Code could be read after all attempts.")
        return None



if __name__ == '__main__':
    print('[PYTHON]: Starting OPENCV and OCR test in 3 seconds')
    gate_client = GateClient()
    for i in range(3, 0, -1):
        print(f"{i}...")
        time.sleep(1)
    
    result_plate = read_license_plate()

    print("\n==================================================")
    if result_plate:
        print(f"[TESTING_SUCESS]: AI reading: {result_plate}")
    else:
        print("[TESTING_FAIL]: Could not read license plate.")
    print("==================================================")

    random_plate = generate_random_plate()
    print(f'[TESTING]: Generated random plate: {random_plate}')
