'''
Avery 1.0 
server.py - local device connected to the ESP32's AP
Developer: Aleksandre Meskhi
Date: 20 March 2025
'''

import cv2
import numpy as np
import torch
import time
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap
import requests
from PIL import Image
import io
import argparse
import socket
import json
import threading
import websocket
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, 
                    format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Default ESP32 IP
ESP32_IP = '192.168.1.100'
SEND_DATA_URL = f"http://{ESP32_IP}/send_data"
WEBSOCKET_URL = f"ws://{ESP32_IP}:81"

# Global WebSocket connection
ws = None
ws_connected = False

# Check if CUDA is available
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
logger.info(f"Using device: {device}")

def on_ws_open(ws):
    """Callback when WebSocket connection opens"""
    global ws_connected
    ws_connected = True
    logger.info("WebSocket connection established")

def on_ws_close(ws, close_status_code, close_msg):
    """Callback when WebSocket connection closes"""
    global ws_connected
    ws_connected = False
    logger.info(f"WebSocket connection closed: {close_status_code}, {close_msg}")

def on_ws_error(ws, error):
    """Callback for WebSocket errors"""
    global ws_connected
    ws_connected = False
    logger.error(f"WebSocket error: {error}")

def on_ws_message(ws, message):
    """Callback for WebSocket messages"""
    logger.debug(f"Received WebSocket message: {message}")

def connect_websocket(ip_address):
    """Connect to the ESP32 WebSocket server"""
    global ws, ws_connected
    
    websocket_url = f"ws://{ip_address}:81"
    logger.info(f"Connecting to WebSocket at {websocket_url}")
    
    # Create WebSocket connection
    ws = websocket.WebSocketApp(
        websocket_url,
        on_open=on_ws_open,
        on_message=on_ws_message,
        on_error=on_ws_error,
        on_close=on_ws_close
    )
    
    # Start WebSocket in a daemon thread
    wst = threading.Thread(target=ws.run_forever)
    wst.daemon = True
    wst.start()
    
    # Wait for connection to establish
    timeout = 5
    start_time = time.time()
    while not ws_connected and time.time() - start_time < timeout:
        time.sleep(0.1)
    
    if ws_connected:
        logger.info("WebSocket connected successfully")
        return True
    else:
        logger.warning("WebSocket connection failed or timed out")
        return False

class ESP32Camera:
    def __init__(self, ip_address, timeout=2, max_retries=2, retry_delay=0.5, use_websocket=True):
        """
        Initialize the ESP32 camera client with improved error handling
        
        Args:
            ip_address: IP address of the ESP32 camera
            timeout: HTTP request timeout in seconds
            max_retries: Maximum number of retries for connection
            retry_delay: Delay between retries in seconds
            use_websocket: Whether to use WebSocket for data communication
        """
        self.ip_address = ip_address
        self.camera_url = f"http://{ip_address}/capture"
        self.stream_url = f"http://{ip_address}/stream"
        self.timeout = timeout
        self.max_retries = max_retries
        self.retry_delay = retry_delay
        self.use_websocket = use_websocket
        
        # Test connection
        self.test_connection(ip_address)
        
        # Connect WebSocket for faster data communication
        if use_websocket:
            connect_websocket(ip_address)
    
    def test_connection(self, ip_address):
        """Test connection to ESP32 camera and print diagnostic information"""
        logger.info(f"Testing connection to ESP32 at {ip_address}...")
        
        # Try to ping the ESP32
        try:
            # Create a socket connection to test if the host is reachable
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2)
            result = sock.connect_ex((ip_address, 80))
            if result == 0:
                logger.info(f"✅ Port 80 is open on {ip_address}")
            else:
                logger.warning(f"❌ Port 80 is not accessible on {ip_address}")
            sock.close()
        except Exception as e:
            logger.error(f"❌ Network test failed: {e}")
        
        # Try HTTP connection
        try:
            response = requests.get(f"http://{ip_address}", timeout=2)
            logger.info(f"✅ HTTP connection successful (status code: {response.status_code})")
        except requests.exceptions.RequestException as e:
            logger.error(f"❌ HTTP connection failed: {e}")
            
        logger.info("Connection test complete.")
    
    def read(self):
        """Read a frame from the ESP32 camera with optimized performance"""
        for attempt in range(self.max_retries):
            try:
                # Use streaming mode for better performance
                response = requests.get(self.camera_url, timeout=self.timeout, stream=True)
                
                if response.status_code == 200:
                    # Convert image bytes to OpenCV format
                    image = Image.open(io.BytesIO(response.content))
                    frame = cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)
                    
                    # Rotate frame 90 degrees counterclockwise to correct ESP32 rotation
                    # frame = cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
                    
                    return True, frame
                else:
                    logger.warning(f"Error: HTTP status code {response.status_code}")
            except requests.exceptions.Timeout:
                logger.warning(f"Timeout error on attempt {attempt+1}/{self.max_retries}")
            except requests.exceptions.ConnectionError:
                logger.warning(f"Connection error on attempt {attempt+1}/{self.max_retries}")
            except Exception as e:
                logger.error(f"Error reading from ESP32 on attempt {attempt+1}/{self.max_retries}: {e}")
            
            # Wait before retrying
            if attempt < self.max_retries - 1:
                time.sleep(self.retry_delay)
        
        # All attempts failed
        logger.error("Failed to read from ESP32 camera after multiple attempts")
        return False, None
    
    def release(self):
        """Release resources"""
        pass
    
    def isOpened(self):
        """Check if camera is opened"""
        return True
    
    def set(self, *args):
        """Placeholder for compatibility with OpenCV's VideoCapture"""
        pass


# Function to send data to ESP32 via WebSocket or HTTP
def send_data_to_esp32(data_array, esp32_ip=ESP32_IP, max_retries=2):
    """Send data array to ESP32 with optimized performance"""
    global ws, ws_connected
    
    # Try WebSocket first if connected (much faster)
    if ws and ws_connected:
        try:
            # Create JSON payload
            payload = json.dumps({"data": data_array})
            ws.send(payload)
            return True
        except Exception as e:
            logger.error(f"WebSocket send error: {e}")
            # Fall back to HTTP if WebSocket fails
    
    # Fall back to HTTP method (legacy)
    url = f"http://{esp32_ip}/send_data"
    
    for attempt in range(max_retries):
        try:
            # Create JSON payload
            payload = {"data": data_array}
            
            # Send POST request
            response = requests.post(
                url, 
                data=json.dumps(payload),
                headers={"Content-Type": "application/json"},
                timeout=1  # Shorter timeout for better performance
            )
            
            if response.status_code == 200:
                return True
            else:
                logger.warning(f"Failed to send data: HTTP {response.status_code}")
        except requests.exceptions.Timeout:
            logger.warning(f"Timeout error on send attempt {attempt+1}/{max_retries}")
        except requests.exceptions.ConnectionError:
            logger.warning(f"Connection error on send attempt {attempt+1}/{max_retries}")
        except Exception as e:
            logger.error(f"Error sending data on attempt {attempt+1}/{max_retries}: {e}")
        
        # Wait before retrying
        if attempt < max_retries - 1:
            time.sleep(0.2)  # Shorter delay for faster retries
    
    # All attempts failed
    logger.error("Failed to send data to ESP32 after multiple attempts")
    return False


class MonocularDepthEstimator:
    def __init__(self, camera_source, model_type="MiDaS_small", frame_skip=2):
        """
        Initialize the monocular depth estimation system
        
        Args:
            camera_source: Camera object (cv2.VideoCapture or ESP32Camera)
            model_type: MiDaS model type - "MiDaS_small" (faster) or "DPT_Large" (more accurate)
            frame_skip: Number of frames to skip between sending motor intensity updates
        """
        # Set camera
        self.cap = camera_source
        self.frame_skip = frame_skip
        
        # Load MiDaS model for depth estimation
        logger.info("Loading MiDaS depth estimation model...")
        
        # Select the model based on type
        if model_type == "DPT_Large":
            model_name = "DPT_Large"
        else:
            model_name = "MiDaS_small"
            
        # Load model from torch hub
        self.midas = torch.hub.load("intel-isl/MiDaS", model_name)
        self.midas.to(device)
        self.midas.eval()
        
        # Load transforms specific to the model
        midas_transforms = torch.hub.load("intel-isl/MiDaS", "transforms")
        
        if model_name == "DPT_Large":
            self.transform = midas_transforms.dpt_transform
        else:
            self.transform = midas_transforms.small_transform
            
        # Create custom colormap (red to white)
        self.create_colormap()
        
        logger.info("Model loaded and ready!")
        
    def create_colormap(self):
        """Create a custom colormap from red (close) to white (far)"""
        colors = [(1, 0, 0), (1, 0.5, 0.5), (1, 0.7, 0.7), (1, 1, 1)]  # Red to White
        self.depth_colormap = LinearSegmentedColormap.from_list("red_to_white", colors)
        
    def process_frame(self, frame, downsample_factor=2):
        """
        Process a single frame to generate a depth map with optimized performance
        
        Args:
            frame: Input BGR frame from camera
            downsample_factor: Factor to downsample input for faster processing
        
        Returns:
            Original frame, depth map, and colored depth map
        """
        # Resize to smaller resolution for faster processing
        if downsample_factor > 1:
            h, w = frame.shape[:2]
            small_frame = cv2.resize(frame, (w // downsample_factor, h // downsample_factor))
        else:
            small_frame = frame
        
        # Convert BGR to RGB
        img = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)
        
        # Apply input transforms
        input_batch = self.transform(img).to(device)
        
        # Prediction
        with torch.no_grad():
            prediction = self.midas(input_batch)
            
            # Interpolate to original small frame size
            prediction = torch.nn.functional.interpolate(
                prediction.unsqueeze(1),
                size=img.shape[:2],
                mode="bicubic",
                align_corners=False,
            ).squeeze()
        
        # Convert to numpy array
        depth_map = prediction.cpu().numpy()
        
        # Normalize depth map to 0-1 range for visualization
        depth_min = depth_map.min()
        depth_max = depth_map.max()
        normalized_depth = (depth_map - depth_min) / (depth_max - depth_min)
        
        # Upsample depth map to original size if downsampled
        if downsample_factor > 1:
            normalized_depth = cv2.resize(normalized_depth, (w, h))
        
        # Apply colormap (red to white)
        colored_depth = (self.depth_colormap(normalized_depth) * 255).astype(np.uint8)
        colored_depth = cv2.cvtColor(colored_depth, cv2.COLOR_RGB2BGR)
        
        return frame, normalized_depth, colored_depth
        
    def run(self):
        """Main loop to capture frames and display depth estimation"""
        cv2.namedWindow('Original', cv2.WINDOW_NORMAL)
        cv2.namedWindow('Depth Map', cv2.WINDOW_NORMAL)
        
        frame_count = 0
        max_failures = 5
        consecutive_failures = 0
        motorIntensities = [0, 0, 0, 0, 0, 0, 0]
        
        # Performance tracking
        fps_list = []
        processing_times = []
        
        try:
            while True:
                start_loop = time.time()
                
                # Capture frame
                ret, frame = self.cap.read()
                
                if not ret:
                    consecutive_failures += 1
                    logger.warning(f"Failed to capture frame ({consecutive_failures}/{max_failures})")
                    
                    if consecutive_failures >= max_failures:
                        logger.error("Too many consecutive failures. Exiting.")
                        break
                    
                    time.sleep(0.5)  # Wait before retrying
                    continue
                
                # Reset failure counter on success
                consecutive_failures = 0
                frame_count += 1
                
                # Process frame
                start_time = time.time()
                original, depth_map, colored_depth = self.process_frame(frame, downsample_factor=2)
                processing_time = time.time() - start_time
                processing_times.append(processing_time)
                
                # Calculate instantaneous and average FPS
                fps = 1.0 / processing_time if processing_time > 0 else 0
                fps_list.append(fps)
                if len(fps_list) > 10:
                    fps_list.pop(0)
                avg_fps = sum(fps_list) / len(fps_list)
                
                # Calculate motor intensities (only on every frame_skip frames)
                if frame_count % self.frame_skip == 0:
                    # Divide the depth map into 7 vertical sections
                    height, width = depth_map.shape
                    section_width = width // 7
                    
                    # Use vectorized operations for better performance
                    for motorIndex in range(7):
                        start_col = motorIndex * section_width
                        end_col = (motorIndex + 1) * section_width if motorIndex < 6 else width
                        
                        # Extract the section and calculate average depth
                        section = depth_map[:, start_col:end_col]
                        motorIntensities[motorIndex] = float(np.mean(section))
                    
                    # Apply intensity transformation (pow 3 for non-linear response)
                    for i in range(7):
                        motorIntensities[i] = pow(motorIntensities[i], 3)
                    
                    # Send motor intensities to ESP32
                    send_data_to_esp32(motorIntensities, ESP32_IP)
                
                # Add performance info to the frame
                cv2.putText(
                    original, 
                    f"FPS: {avg_fps:.1f}", 
                    (20, 40), 
                    cv2.FONT_HERSHEY_SIMPLEX, 
                    1, 
                    (0, 255, 0), 
                    2
                )
                
                cv2.putText(
                    colored_depth,
                    f"Motors: {', '.join(f'{x:.2f}' for x in motorIntensities)}",
                    (20, 40), 
                    cv2.FONT_HERSHEY_SIMPLEX, 
                    1, 
                    (0, 255, 0), 
                    2
                )  
                
                # Display frames
                cv2.imshow('Original', original)
                cv2.imshow('Depth Map', colored_depth)
                
                # Calculate loop time
                loop_time = time.time() - start_loop
                if frame_count % 30 == 0:  # Log performance every 30 frames
                    avg_proc_time = sum(processing_times) / len(processing_times)
                    logger.info(f"Performance: Avg FPS: {avg_fps:.1f}, Process time: {avg_proc_time:.3f}s, Total loop: {loop_time:.3f}s")
                        
                # Check for key press to exit
                key = cv2.waitKey(1) & 0xFF
                if key == 27 or key == ord('q'):  # ESC or 'q' key
                    logger.info("User requested exit")
                    break
                
        except KeyboardInterrupt:
            logger.info("Keyboard interrupt detected. Exiting.")
        except Exception as e:
            logger.error(f"Error in main loop: {e}")
        finally:
            # Clean up resources
            self.cap.release()
            cv2.destroyAllWindows()
            logger.info("Resources released. Exiting.")


def main():
    """Main entry point for the application"""
    parser = argparse.ArgumentParser(description='Monocular Depth Estimation with ESP32 Camera')
    
    parser.add_argument('--esp32-ip', type=str, default='192.168.1.100',
                      help='IP address of the ESP32 camera')
    parser.add_argument('--video-source', type=int, default=None,
                      help='Camera index for local camera (default: None, uses ESP32)')
    parser.add_argument('--model', type=str, default='MiDaS_small', choices=['MiDaS_small', 'DPT_Large'],
                      help='Model type for depth estimation (default: MiDaS_small)')
    parser.add_argument('--frame-skip', type=int, default=2,
                      help='Number of frames to skip between motor updates (default: 2)')
    
    args = parser.parse_args()
    
    # Update global ESP32 IP
    global ESP32_IP
    ESP32_IP = args.esp32_ip
    
    # Select camera source
    if args.video_source is not None:
        # Use local camera
        logger.info(f"Using local camera index: {args.video_source}")
        camera = cv2.VideoCapture(args.video_source)
        camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    else:
        # Use ESP32 camera
        logger.info(f"Using ESP32 camera at {ESP32_IP}")
        camera = ESP32Camera(ESP32_IP, use_websocket=True)
    
    # Create and run the depth estimator
    depth_estimator = MonocularDepthEstimator(
        camera_source=camera,
        model_type=args.model,
        frame_skip=args.frame_skip
    )
    
    # Run the main processing loop
    depth_estimator.run()


if __name__ == "__main__":
    main()

'''
Avery 1.0 
server.py - local device connected to the ESP32's AP
Developer: Aleksandre Meskhi
Date: 20 March 2025
'''
