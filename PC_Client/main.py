import serial.tools.list_ports as port_list
import os
from serial import Serial
import re
import pygame
import time
import cv2
import numpy as np
from threading import Thread

# Initialize pygame for UI
pygame.init()

# Global variables
currentQuestion = 0
inQuestion = False
started = False
currentQTime = 0
maxQuestionTime = 0
playing_video = False
video_thread = None

# Create window
def createWindow(width, height):
    screen = pygame.display.set_mode((width, height))
    pygame.display.set_caption("Quiz Application")
    return screen

# Display an image with proper aspect ratio
def displayImage(image_path):
    try:
        # Directly use JPG extension since we know all images are JPG
        base_path = os.path.splitext(image_path)[0]
        image_path_jpg = f"{base_path}.JPG"
        
        if not os.path.exists(image_path_jpg):
            # Try lowercase extension as fallback
            image_path_jpg = f"{base_path}.jpg"
            if not os.path.exists(image_path_jpg):
                print(f"Looking for file at: {image_path_jpg}")
                print(f"Current directory: {os.getcwd()}")
                raise FileNotFoundError(f"No file found at {image_path_jpg}")
        
        # Load the image        
        image = pygame.image.load(image_path_jpg)
        img_width, img_height = image.get_size()
        
        # Calculate aspect ratio
        img_ratio = img_width / img_height
        screen_ratio = screen_width / screen_height
        
        # Scale while maintaining aspect ratio
        if screen_ratio > img_ratio:
            # Screen is wider, constrain by height
            new_height = screen_height
            new_width = int(new_height * img_ratio)
        else:
            # Screen is taller, constrain by width
            new_width = screen_width
            new_height = int(new_width / img_ratio)
        
        # Scale image
        scaled_image = pygame.transform.smoothscale(image, (new_width, new_height))
        
        # Center the image on screen
        x_offset = (screen_width - new_width) // 2
        y_offset = (screen_height - new_height) // 2
        
        # Fill screen with black
        screen.fill((0, 0, 0))
        
        # Blit scaled image
        screen.blit(scaled_image, (x_offset, y_offset))
        pygame.display.flip()
        print(f"Displaying image: {image_path_jpg}")
    except Exception as e:
        print(f"Error loading image {image_path}: {e}")

# Function to play video in a separate thread
def play_video_thread(video_path):
    global playing_video
    
    try:
        if not os.path.exists(video_path):
            print(f"Video file not found: {video_path}")
            intro_image = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Intro")
            displayImage(intro_image)
            playing_video = False
            return
            
        # Open the video file
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            print("Error opening video file")
            playing_video = False
            return
        
        # Get video properties
        fps = cap.get(cv2.CAP_PROP_FPS)
        if fps == 0:
            fps = 30  # fallback if fps cannot be determined
        
        frame_time = 1000 / fps  # time per frame in milliseconds
        
        # Main video playback loop
        while cap.isOpened() and playing_video:
            start_time = pygame.time.get_ticks()
            
            ret, frame = cap.read()
            if not ret:
                break  # End of video
                
            # Convert frame from BGR to RGB
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            
            # Get frame dimensions
            frame_height, frame_width = frame.shape[0], frame.shape[1]
            
            # Calculate aspect ratio
            frame_ratio = frame_width / frame_height
            screen_ratio = screen_width / screen_height
            
            # Scale while maintaining aspect ratio
            if screen_ratio > frame_ratio:
                # Screen is wider, constrain by height
                new_height = screen_height
                new_width = int(new_height * frame_ratio)
            else:
                # Screen is taller, constrain by width
                new_width = screen_width
                new_height = int(new_width / frame_ratio)
                
            # Resize frame
            frame = cv2.resize(frame, (new_width, new_height), interpolation=cv2.INTER_AREA)
            
            # Convert frame to pygame surface
            frame_surface = pygame.surfarray.make_surface(frame.swapaxes(0, 1))
            
            # Center on screen
            x_offset = (screen_width - new_width) // 2
            y_offset = (screen_height - new_height) // 2
            
            # Fill screen with black
            screen.fill((0, 0, 0))
            
            # Blit frame
            screen.blit(frame_surface, (x_offset, y_offset))
            pygame.display.flip()
            
            # Control frame rate
            elapsed = pygame.time.get_ticks() - start_time
            delay = max(0, frame_time - elapsed)
            pygame.time.wait(int(delay))
            
            # Check pygame events to allow quitting
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    playing_video = False
                    
        cap.release()
        playing_video = False
            
    except Exception as e:
        print(f"Error playing video {video_path}: {e}")
        playing_video = False

# Display a video in the pygame window
def displayVideo(video_path):
    global playing_video, video_thread
    
    # Stop any currently playing video
    if playing_video and video_thread is not None:
        playing_video = False
        if video_thread.is_alive():
            video_thread.join(timeout=1.0)
    
    playing_video = True
    video_thread = Thread(target=play_video_thread, args=(video_path,))
    video_thread.daemon = True
    video_thread.start()

# Draw a progress bar overlay
def overlay(percent):
    # Draw a semi-transparent progress bar at the bottom of the screen
    bar_height = 30
    bar_width = int(screen_width * percent)
    
    # Create a transparent surface
    overlay_surface = pygame.Surface((screen_width, bar_height), pygame.SRCALPHA)
    
    # Draw background (semi-transparent)
    pygame.draw.rect(overlay_surface, (0, 0, 0, 128), (0, 0, screen_width, bar_height))
    
    # Draw progress bar
    pygame.draw.rect(overlay_surface, (0, 255, 0, 200), (0, 0, bar_width, bar_height))
    
    # Blit the overlay at the bottom of the screen
    screen.blit(overlay_surface, (0, screen_height - bar_height))
    pygame.display.flip()

# Main program
baud = 9600
screen_width = 2880
screen_height = 1800

# Get current working directory for debugging
current_dir = os.getcwd()
print(f"Current working directory: {current_dir}")

# Check if assets folder exists
if not os.path.exists(os.path.join("P2M5", "PC_Client", "Assets")):
    # Check if we need to go up one directory
    if os.path.exists("PC_Client"):
        os.chdir("..")
        print(f"Changed working directory to: {os.getcwd()}")

screen = createWindow(screen_width, screen_height)
clock = pygame.time.Clock()

# Find available serial ports
ports = list(port_list.comports())
if not ports:
    print("No serial ports found!")
    exit(1)

# Connect to the last port in the list
port = ports[-1].device
print(f"Connecting to port: {port}")
ser = Serial(port, baud, timeout=1)

running = True
while running:
    # Handle pygame events
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
    
    try:
        # Read from serial port
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').strip()
            if line:
                print(f"Received: {line}")

                if line == "1/":
                    # Try to find either video or intro image
                    video_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "video.mp4")
                    displayVideo(video_path)
                    started = True

                elif line.startswith("2<") and line.endswith(">/"):
                    match = re.match(r"2<(\d+)>/$", line)
                    if match:
                        currentQuestion = int(match.group(1))
                        question_path = os.path.join("P2M5", "PC_Client", "Assets", "Questions", f"Q{currentQuestion+1}")
                        displayImage(question_path)
                        inQuestion = True
                    
                elif line == "3/":
                    correct_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Correct")
                    displayImage(correct_path)
                    inQuestion = False

                elif line.startswith("5<") and line.endswith(">/"):
                    match = re.match(r"5<(\d+)>/$", line)
                    if match:
                        currentQTime = int(match.group(1))
                        maxQuestionTime = currentQTime
                        print(f"Question time: {currentQTime}ms")

                elif line == "4/":
                    wrong_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Wrong")
                    displayImage(wrong_path)
                    time.sleep(1)  # Wait 1 second
                    correct_ans_path = os.path.join("P2M5", "PC_Client", "Assets", "Responces", f"Q{currentQuestion+1}_CorrectAns")
                    displayImage(correct_ans_path)
                    inQuestion = False

                elif line == "6":
                    end_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "End")
                    displayImage(end_path)
                    started = False
                    inQuestion = False
        
        # Update progress bar if in a question
        if inQuestion and currentQTime > 0:
            percent = currentQTime / maxQuestionTime
            overlay(percent)
            currentQTime -= clock.get_time()  # Subtract elapsed time since last frame
            
        # Cap the frame rate
        clock.tick(30)
        
    except Exception as e:
        print(f"Error: {e}")

# Clean up
playing_video = False
if video_thread and video_thread.is_alive():
    video_thread.join(timeout=1.0)
ser.close()
pygame.quit()