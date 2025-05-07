#!/usr/bin/env python3
"""
ESP32-CAM Stream Vision - OpenCV Client

This script captures and processes video from an ESP32-CAM stream.
It provides various processing options and can be used in computer vision projects.

Part of the ESP32-CAM Stream Vision project
https://github.com/yourusername/ESP32-CAM-Stream-Vision
"""

import cv2
import numpy as np
import time
import argparse
import sys
import signal

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    print('\nStream viewing interrupted by user')
    sys.exit(0)

def process_frame(frame, mode='default'):
    """
    Process the frame with OpenCV.
    Different modes offer different processing techniques.
    
    Args:
        frame: The input frame from the camera
        mode: Processing mode (default, edges, gray, threshold, contours)
        
    Returns:
        processed_frame: The processed frame
    """
    if mode == 'edges':
        # Convert to grayscale and detect edges
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blurred, 50, 150)
        return cv2.cvtColor(edges, cv2.COLOR_GRAY2BGR)
    
    elif mode == 'gray':
        # Simple grayscale conversion
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        return cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    
    elif mode == 'threshold':
        # Apply adaptive thresholding
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        thresh = cv2.adaptiveThreshold(blurred, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                      cv2.THRESH_BINARY, 11, 2)
        return cv2.cvtColor(thresh, cv2.COLOR_GRAY2BGR)
    
    elif mode == 'contours':
        # Find and draw contours
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blurred, 50, 150)
        
        # Find contours
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # Draw contours
        contour_img = np.zeros_like(frame)
        cv2.drawContours(contour_img, contours, -1, (0, 255, 0), 2)
        return contour_img
    
    elif mode == 'combined':
        # Show original, edges, and contours side-by-side
        original = frame.copy()
        
        # Create edges
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blurred, 50, 150)
        edges_colored = cv2.cvtColor(edges, cv2.COLOR_GRAY2BGR)
        
        # Create contours
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        contour_img = np.zeros_like(frame)
        cv2.drawContours(contour_img, contours, -1, (0, 255, 0), 2)
        
        # Combine images horizontally
        result = np.hstack((original, edges_colored, contour_img))
        return result
    
    else:  # default mode
        # Return original frame
        return frame

def main():
    """Main function to run the ESP32-CAM stream processor"""
    
    # Register signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)
    
    # Create argument parser
    parser = argparse.ArgumentParser(description='Access ESP32-CAM stream with OpenCV')
    parser.add_argument('--ip', type=str, default='192.168.4.1', 
                        help='IP address of the ESP32-CAM (default: 192.168.4.1 - AP mode)')
    parser.add_argument('--port', type=int, default=80, 
                        help='Port number (default: 80)')
    parser.add_argument('--path', type=str, default='/stream', 
                        help='Stream path (default: /stream)')
    parser.add_argument('--mode', type=str, default='default',
                        choices=['default', 'edges', 'gray', 'threshold', 'contours', 'combined'],
                        help='Processing mode (default, edges, gray, threshold, contours, combined)')
    parser.add_argument('--save', action='store_true',
                        help='Save processed video to file')
    parser.add_argument('--output', type=str, default='esp32_output.mp4',
                        help='Output video filename (default: esp32_output.mp4)')
    parser.add_argument('--resize', type=int, default=0,
                        help='Resize output window to this width (keeps aspect ratio, 0 for no resize)')
    
    args = parser.parse_args()
    
    # Construct the URL for the stream
    url = f'http://{args.ip}:{args.port}{args.path}'
    print(f"Connecting to stream at: {url}")
    print(f"Processing mode: {args.mode}")
    
    # Open the video stream
    cap = cv2.VideoCapture(url)
    
    if not cap.isOpened():
        print("Error: Could not open stream. Check the IP address and make sure the ESP32-CAM is running.")
        return
    
    # Get video properties
    frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    
    # Video writer for saving
    out = None
    if args.save:
        # Define the codec and create VideoWriter object
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        
        # Adjust output size based on processing mode
        if args.mode == 'combined':
            out_width = frame_width * 3  # Three images side by side
            out_height = frame_height
        else:
            out_width = frame_width
            out_height = frame_height
            
        out = cv2.VideoWriter(args.output, fourcc, 10.0, (out_width, out_height))
        print(f"Saving output to: {args.output}")
    
    # Create window with adjustable size
    window_name = f'ESP32-CAM Stream ({args.mode} mode)'
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    
    # Set window size if requested
    if args.resize > 0:
        # Calculate height to maintain aspect ratio
        if args.mode == 'combined':
            aspect_ratio = (frame_width * 3) / frame_height
        else:
            aspect_ratio = frame_width / frame_height
        
        resize_height = int(args.resize / aspect_ratio)
        cv2.resizeWindow(window_name, args.resize, resize_height)
    elif args.mode == 'combined':
        # Default size for combined mode
        cv2.resizeWindow(window_name, 1280, int(1280 * frame_height / (frame_width * 3)))
    else:
        # Default size for other modes
        cv2.resizeWindow(window_name, 640, int(640 * frame_height / frame_width))
    
    print("Stream started. Press 'q' to exit.")
    
    try:
        # Track FPS
        fps_start_time = time.time()
        fps_frame_count = 0
        fps = 0
        
        while True:
            # Read frame from the video stream
            ret, frame = cap.read()
            
            if not ret:
                print("Error: Could not read frame. Attempting to reconnect...")
                # Wait a bit before trying to reconnect
                time.sleep(1)
                cap.release()
                cap = cv2.VideoCapture(url)
                continue
            
            # Process the frame
            processed_frame = process_frame(frame, args.mode)
            
            # Calculate FPS
            fps_frame_count += 1
            if fps_frame_count >= 10:
                end_time = time.time()
                fps = fps_frame_count / (end_time - fps_start_time)
                fps_start_time = time.time()
                fps_frame_count = 0
            
            # Add FPS text to frame
            cv2.putText(processed_frame, f"FPS: {fps:.1f}", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            
            # Save frame if requested
            if args.save and out is not None:
                out.write(processed_frame)
            
            # Display the frame
            cv2.imshow(window_name, processed_frame)
            
            # Press 'q' to exit
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
    
    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Release the VideoCapture and close windows
        cap.release()
        if out is not None:
            out.release()
        cv2.destroyAllWindows()
        print("Stream closed")

if __name__ == "__main__":
    main()
