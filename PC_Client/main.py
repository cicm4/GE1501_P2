import serial.tools.list_ports as port_list
import os
from serial import Serial
import re
import pygame
import time

# Initialize pygame for UI
pygame.init()

# Global variables
currentQuestion = 0
inQuestion = False
started = False
currentQTime = 0
maxQuestionTime = 0

# Create window
def createWindow(width, height):
    screen = pygame.display.set_mode((width, height))
    pygame.display.set_caption("Quiz Application")
    return screen

# Display an image
def displayImage(image_path):
    try:
        image = pygame.image.load(image_path)
        image = pygame.transform.scale(image, (screen_width, screen_height))
        screen.blit(image, (0, 0))
        pygame.display.flip()
        print(f"Displaying image: {image_path}")
    except Exception as e:
        print(f"Error loading image {image_path}: {e}")

# Display a video
def displayVideo(video_path):
    try:
        os.system(f'start {video_path}')  # Simple approach for Windows
        print(f"Playing video: {video_path}")
    except Exception as e:
        print(f"Error playing video {video_path}: {e}")

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
                    displayVideo("Main/video.mp4")
                    started = True

                elif line.startswith("2<") and line.endswith(">/"):
                    match = re.match(r"2<(\d+)>/$", line)
                    if match:
                        currentQuestion = int(match.group(1))
                        displayImage(f"Q{currentQuestion+1}.png")
                        inQuestion = True
                    
                elif line == "3/":
                    displayImage("Correct.png")
                    inQuestion = False

                elif line.startswith("5<") and line.endswith(">/"):
                    match = re.match(r"5<(\d+)>/$", line)
                    if match:
                        currentQTime = int(match.group(1))
                        maxQuestionTime = currentQTime
                        print(f"Question time: {currentQTime}ms")

                elif line == "4/":
                    displayImage("Wrong.png")
                    time.sleep(1)  # Wait 1 second
                    displayImage(f"Q{currentQuestion+1}_CorrectAns.png")
                    inQuestion = False

                elif line == "6":
                    displayImage("End.png")
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
ser.close()
pygame.quit()