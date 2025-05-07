#!/usr/bin/env python3
"""
ESP32-CAM Stream Vision - Advanced Processing

This script provides more advanced computer vision capabilities for processing
the ESP32-CAM stream, including motion detection, object tracking, and color filtering.

Part of the ESP32-CAM Stream Vision project
https://github.com/yourusername/ESP32-CAM-Stream-Vision
"""

import cv2
import numpy as np
import time
import argparse
import sys
import signal
from collections import deque

class MotionDetector:
    """Class for detecting motion in video stream"""
    
    def __init__(self, history=20, threshold=25, detect_shadows=True, min_area=500):
        """Initialize motion detector"""
        self.bg_subtractor = cv2.createBackgroundSubtractorMOG2(
            history=history,
            varThreshold=threshold,
            detectShadows=detect_shadows
        )
        self.min_area = min_area
        self.motion_detected = False
        self.motion_timestamp = 0
        self.motion_duration = 0
        
    def detect(self, frame):
        """Detect motion in the frame"""
        # Apply background subtraction
        fg_mask = self.bg_subtractor.apply(frame)
        
        # Remove noise
        kernel = np.ones((5, 5), np.uint8)
        fg_mask = cv2.morphologyEx(fg_mask, cv2.MORPH_OPEN, kernel)
        fg_mask = cv2.morphologyEx(fg_mask, cv2.MORPH_CLOSE, kernel)
        
        # Find contours in the mask
        contours, _ = cv2.findContours(fg_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # Initialize motion flag
        motion_in_current_frame = False
        
        # Process each contour
        for contour in contours:
            if cv2.contourArea(contour) < self.min_area:
                continue
                
            # Get bounding box
            x, y, w, h = cv2.boundingRect(contour)
            
            # Draw rectangle around the motion
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
            
            # Set motion flag
            motion_in_current_frame = True
        
        # Update motion state
        current_time = time.time()
        if motion_in_current_frame:
            if not self.motion_detected:
                # Motion just started
                self.motion_detected = True
                self.motion_timestamp = current_time
            
            # Calculate duration
            self.motion_duration = current_time - self.motion_timestamp
        else:
            if self.motion_detected:
                # Motion just ended
                self.motion_detected = False
                self.motion_duration = 0
        
        # Add motion status text to frame
        status_text = "Motion Detected" if self.motion_detected else "No Motion"
        cv2.putText(frame, status_text, (10, 30), 
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255) if self.motion_detected else (0, 255, 0), 2)
        
        if self.motion_detected:
            cv2.putText(frame, f"Duration: {self.motion_duration:.1f}s", (10, 70), 
                       cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
        
        return frame, self.motion_detected, fg_mask


class ObjectTracker:
    """Class for tracking objects in video stream"""
    
    def __init__(self, tracker_type="KCF", trail_length=64):
        """Initialize object tracker"""
        self.tracker = None
        self.tracker_type = tracker_type
        self.tracking = False
        self.bbox = None
        self.trail_length = trail_length
        self.center_points = deque(maxlen=trail_length)
        
        # Available tracker types
        self.tracker_types = {
            "BOOSTING": cv2.legacy.TrackerBoosting_create,
            "MIL": cv2.legacy.TrackerMIL_create,
            "KCF": cv2.legacy.TrackerKCF_create,
            "TLD": cv2.legacy.TrackerTLD_create,
            "MEDIANFLOW": cv2.legacy.TrackerMedianFlow_create,
            "MOSSE": cv2.legacy.TrackerMOSSE_create,
            "CSRT": cv2.legacy.TrackerCSRT_create
        }
    
    def create_tracker(self):
        """Create a new tracker instance"""
        if self.tracker_type in self.tracker_types:
            self.tracker = self.tracker_types[self.tracker_type]()
        else:
            # Default to KCF tracker
            self.tracker = cv2.legacy.TrackerKCF_create()
    
    def start_tracking(self, frame, bbox):
        """Start tracking a new object"""
        self.create_tracker()
        self.bbox = bbox
        success = self.tracker.init(frame, bbox)
        if success:
            self.tracking = True
            self.center_points.clear()
            center_x = int(bbox[0] + bbox[2] / 2)
            center_y = int(bbox[1] + bbox[3] / 2)
            self.center_points.append((center_x, center_y))
        return success
    
    def update(self, frame):
        """Update tracker with new frame"""
        if not self.tracking:
            return frame, False, None
        
        # Update tracker
        success, bbox = self.tracker.update(frame)
        
        if success:
            # Tracking success
            self.bbox = bbox
            
            # Calculate center point
            center_x = int(bbox[0] + bbox[2] / 2)
            center_y = int(bbox[1] + bbox[3] / 2)
            self.center_points.append((center_x, center_y))
            
            # Draw bounding box
            p1 = (int(bbox[0]), int(bbox[1]))
            p2 = (int(bbox[0] + bbox[2]), int(bbox[1] + bbox[3]))
            cv2.rectangle(frame, p1, p2, (0, 255, 0), 2, 1)
            
            # Draw trail
            for i in range(1, len(self.center_points)):
                if self.center_points[i - 1] is None or self.center_points[i] is None:
                    continue
                    
                # Calculate color gradient based on position in the trail
                color_intensity = int(255 * (i / len(self.center_points)))
                color = (0, color_intensity, 255 - color_intensity)
                
                # Draw line segment
                cv2.line(frame, self.center_points[i - 1], self.center_points[i], color, 2)
            
            # Display tracking status
            cv2.putText(frame, "Tracking", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        else:
            # Tracking failure
            cv2.putText(frame, "Tracking Failed", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            self.tracking = False
        
        return frame, success, bbox
    
    def stop_tracking(self):
        """Stop tracking"""
        self.tracking = False
        self.bbox = None
        self.center_points.clear()


class ColorFilter:
    """Class for color filtering in video stream"""
    
    def __init__(self, color_name="red"):
        """Initialize color filter"""
        self.color_name = color_name
        self.color_ranges = {
            "red": [
                (np.array([0, 100, 100]), np.array([10, 255, 255])),
                (np.array([160, 100, 100]), np.array([180, 255, 255]))
            ],
            "green": [(np.array([40, 40, 40]), np.array([80, 255, 255]))],
            "blue": [(np.array([100, 40, 40]), np.array([140, 255, 255]))],
            "yellow": [(np.array([20, 100, 100]), np.array([40, 255, 255]))],
            "purple": [(np.array([140, 40, 40]), np.array([170, 255, 255]))],
            "orange": [(np.array([10, 100, 100]), np.array([25, 255, 255]))]
        }
        
    def set_color(self, color_name):
        """Set the color to filter"""
        if color_name in self.color_ranges:
            self.color_name = color_name
            return True
        return False
    
    def filter(self, frame):
        """Apply color filtering to the frame"""
        # Convert to HSV color space
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        
        # Create mask for the selected color
        mask = None
        
        if self.color_name in self.color_ranges:
            # Apply each range for the selected color
            for lower, upper in self.color_ranges[self.color_name]:
                if mask is None:
                    mask = cv2.inRange(hsv, lower, upper)
                else:
                    mask = mask | cv2.inRange(hsv, lower, upper)
        
        # Apply morphological operations to clean up the mask
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        
        # Find contours
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # Find the largest contour
        largest_contour = None
        max_area = 0
        
        for contour in contours:
            area = cv2.contourArea(contour)
            if area > 500 and area > max_area:
                max_area = area
                largest_contour = contour
        
        # Draw contours on original frame
        result = frame.copy()
        
        if largest_contour is not None:
            cv2.drawContours(result, [largest_contour], -1, (0, 255, 0), 2)
            
            # Get bounding rectangle
            x, y, w, h = cv2.boundingRect(largest_contour)
            cv2.rectangle(result, (x, y), (x + w, y + h), (0, 0, 255), 2)
            
            # Calculate center
            center_x = x + w // 2
            center_y = y + h // 2
            
            # Draw center point
            cv2.circle(result, (center_x, center_y), 5, (255, 0, 0), -1)
            
            # Add text
            cv2.putText(result, f"{self.color_name.capitalize()} object", (x, y - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        # Apply color mask to create filtered image
        filtered = cv2.bitwise_and(frame, frame, mask=mask)
        
        # Return various outputs
        return result, filtered, mask


def main():
    """Main function to run the advanced ESP32-CAM stream processor"""
    
    # Register signal handler for Ctrl+C
    signal.signal(signal.SIGINT, lambda sig, frame: sys.exit(0))
    
    # Create argument parser
    parser = argparse.ArgumentParser(description='Advanced processing for ESP32-CAM stream')
    parser.add_argument('--ip', type=str, default='192.168.4.1', 
                        help='IP address of the ESP32-CAM (default: 192.168.4.1 - AP mode)')
    parser.add_argument('--port', type=int, default=80, 
                        help='Port number (default: 80)')
    parser.add_argument('--path', type=str, default='/stream', 
                        help='Stream path (default: /stream)')
    parser.add_argument('--mode', type=str, default='motion',
                        choices=['motion', 'tracking', 'color'],
                        help='Processing mode (motion, tracking, color)')
    parser.add_argument('--color', type=str, default='red',
                        choices=['red', 'green', 'blue', 'yellow', 'purple', 'orange'],
                        help='Color to filter in color mode (default: red)')
    parser.add_argument('--tracker', type=str, default='KCF',
                        choices=['BOOSTING', 'MIL', 'KCF', 'TLD', 'MEDIANFLOW', 'MOSSE', 'CSRT'],
                        help='Tracker type to use in tracking mode (default: KCF)')
    parser.add_argument('--save', action='store_true',
                        help='Save processed video to file')
    parser.add_argument('--output', type=str, default='esp32_advanced_output.mp4',
                        help='Output video filename (default: esp32_advanced_output.mp4)')
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
    
    # Initialize video writer
    out = None
    if args.save:
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        out = cv2.VideoWriter(args.output, fourcc, 10.0, (frame_width, frame_height))
        print(f"Saving output to: {args.output}")
    
    # Initialize processing objects based on mode
    if args.mode == 'motion':
        processor = MotionDetector(min_area=500)
        window_name = 'ESP32-CAM Motion Detection'
    elif args.mode == 'tracking':
        processor = ObjectTracker(tracker_type=args.tracker)
        window_name = f'ESP32-CAM Object Tracking ({args.tracker})'
    else:  # color mode
        processor = ColorFilter(color_name=args.color)
        window_name = f'ESP32-CAM Color Filter ({args.color})'
    
    # Create windows
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    
    # Secondary windows for different modes
    if args.mode == 'motion':
        cv2.namedWindow('Motion Mask', cv2.WINDOW_NORMAL)
    elif args.mode == 'color':
        cv2.namedWindow('Color Filtered', cv2.WINDOW_NORMAL)
        cv2.namedWindow('Color Mask', cv2.WINDOW_NORMAL)
    
    # Set window sizes
    if args.resize > 0:
        aspect_ratio = frame_width / frame_height
        resize_height = int(args.resize / aspect_ratio)
        cv2.resizeWindow(window_name, args.resize, resize_height)
        
        if args.mode == 'motion':
            cv2.resizeWindow('Motion Mask', args.resize, resize_height)
        elif args.mode == 'color':
            cv2.resizeWindow('Color Filtered', args.resize, resize_height)
            cv2.resizeWindow('Color Mask', args.resize, resize_height)
    else:
        cv2.resizeWindow(window_name, 640, int(640 * frame_height / frame_width))
        
        if args.mode == 'motion':
            cv2.resizeWindow('Motion Mask', 320, int(320 * frame_height / frame_width))
        elif args.mode == 'color':
            cv2.resizeWindow('Color Filtered', 320, int(320 * frame_height / frame_width))
            cv2.resizeWindow('Color Mask', 320, int(320 * frame_height / frame_width))
    
    print(f"Stream started in {args.mode} mode. Controls:")
    
    if args.mode == 'tracking':
        print("  - Press 's' to select an object to track")
        print("  - Press 'r' to reset tracking")
    elif args.mode == 'color':
        print("  - Press keys 1-6 to change color:")
        print("    1: Red, 2: Green, 3: Blue, 4: Yellow, 5: Purple, 6: Orange")
    
    print("  - Press 'q' to exit")
    
    # Variables for tracking mode
    tracking_object = False
    selection_in_progress = False
    selection_roi = None
    
    # Track FPS
    fps_start_time = time.time()
    fps_frame_count = 0
    fps = 0
    
    try:
        while True:
            # Read frame from the video stream
            ret, frame = cap.read()
            
            if not ret:
                print("Error: Could not read frame. Attempting to reconnect...")
                time.sleep(1)
                cap.release()
                cap = cv2.VideoCapture(url)
                continue
            
            # Make a copy of the frame for processing
            display_frame = frame.copy()
            
            # Calculate FPS
            fps_frame_count += 1
            if fps_frame_count >= 10:
                end_time = time.time()
                fps = fps_frame_count / (end_time - fps_start_time)
                fps_start_time = time.time()
                fps_frame_count = 0
            
            # Process frame based on selected mode
            if args.mode == 'motion':
                processed_frame, motion_detected, motion_mask = processor.detect(display_frame)
                
                # Show the motion mask
                cv2.imshow('Motion Mask', motion_mask)
                
            elif args.mode == 'tracking':
                if tracking_object:
                    processed_frame, success, bbox = processor.update(display_frame)
                    if not success:
                        tracking_object = False
                else:
                    processed_frame = display_frame
                    if selection_in_progress and selection_roi:
                        # Draw selected region
                        x, y, w, h = selection_roi
                        cv2.rectangle(processed_frame, (x, y), (x + w, y + h), (255, 0, 0), 2)
                
            elif args.mode == 'color':
                processed_frame, filtered_frame, color_mask = processor.filter(display_frame)
                
                # Show the filtered image and mask
                cv2.imshow('Color Filtered', filtered_frame)
                cv2.imshow('Color Mask', color_mask)
            
            # Add FPS to the frame
            cv2.putText(processed_frame, f"FPS: {fps:.1f}", (10, frame_height - 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            
            # Save frame if requested
            if args.save and out is not None:
                out.write(processed_frame)
            
            # Display the processed frame
            cv2.imshow(window_name, processed_frame)
            
            # Handle keyboard input
            key = cv2.waitKey(1) & 0xFF
            
            if key == ord('q'):
                # Exit
                break
                
            elif args.mode == 'tracking' and key == ord('s'):
                # Start selection of object to track
                print("Select an object to track. Draw a rectangle with the mouse.")
                selection_roi = cv2.selectROI(window_name, frame, fromCenter=False, showCrosshair=True)
                if selection_roi[2] > 0 and selection_roi[3] > 0:
                    tracking_object = processor.start_tracking(frame, selection_roi)
                    print("Tracking started" if tracking_object else "Failed to initialize tracker")
                else:
                    print("Selection cancelled")
                
            elif args.mode == 'tracking' and key == ord('r'):
                # Reset tracking
                processor.stop_tracking()
                tracking_object = False
                print("Tracking reset")
                
            elif args.mode == 'color':
                # Change color filter
                color_keys = {
                    ord('1'): 'red',
                    ord('2'): 'green',
                    ord('3'): 'blue',
                    ord('4'): 'yellow',
                    ord('5'): 'purple',
                    ord('6'): 'orange'
                }
                
                if key in color_keys:
                    selected_color = color_keys[key]
                    processor.set_color(selected_color)
                    print(f"Color filter changed to: {selected_color}")
    
    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Release resources
        cap.release()
        if out is not None:
            out.release()
        cv2.destroyAllWindows()
        print("Stream closed")

if __name__ == "__main__":
    main()
