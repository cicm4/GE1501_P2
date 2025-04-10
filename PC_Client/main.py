import os
import serial.tools.list_ports as port_list
from serial import Serial
import re
import pygame
import time
import cv2
import numpy as np
from threading import Thread
import csv
import datetime

# CSV Initialization
csv_file = open('quiz_data_log.csv', mode='w', newline='')
csv_writer = csv.writer(csv_file)

# CSV header row (example columns based on your Arduino data output)
csv_writer.writerow(['Timestamp', 'Quiz_Size', 'Number_of_Questions', 'Questions_Asked', 'Total_Time', 'Quiz_Points', 'Motor_Time'])



pygame.init()

# GLOBALS
currentQuestion = 0
inQuestion = False
started = False
currentQTime = 0
maxQuestionTime = 0
playing_video = False
video_thread = None
countQuestions = 0
number_of_questions = 5
quiz_points = 0

#############################
# Fetch Windows display info
#############################
info_object = pygame.display.Info()
SCREEN_WIDTH = info_object.current_w
SCREEN_HEIGHT = info_object.current_h

def createWindow(width, height):
    """
    Create the main Pygame window at the desired resolution.
    """
    screen = pygame.display.set_mode((width, height), pygame.FULLSCREEN)
    pygame.display.set_caption("Quiz Application - Windows Edition")
    return screen

# Initialize the Pygame window
screen = createWindow(SCREEN_WIDTH, SCREEN_HEIGHT)

print("Pygame sees window width:", SCREEN_WIDTH)
print("Pygame sees window height:", SCREEN_HEIGHT)

clock = pygame.time.Clock()

def scale_to_fit(image, max_width, max_height):
    """
    Scales 'image' so that it fits entirely within (max_width x max_height)
    while preserving the image's aspect ratio.
    """
    img_width, img_height = image.get_size()
    img_ratio = img_width / img_height
    screen_ratio = max_width / max_height

    if img_ratio > screen_ratio:
        # Image is relatively wider than the target
        new_width = max_width
        new_height = int(new_width / img_ratio)
    else:
        # Image is relatively taller or same ratio
        new_height = max_height
        new_width = int(new_height * img_ratio)

    return pygame.transform.smoothscale(image, (new_width, new_height))

def displayImage(image_path):
    """
    Loads and displays the image at 'image_path' (JPG or jpg) 
    scaled to fit the current window without cutting off.
    """
    try:
        # Resolve .JPG or .jpg extension
        base_path = os.path.splitext(image_path)[0]
        image_path_jpg = f"{base_path}.JPG"
        if not os.path.exists(image_path_jpg):
            image_path_jpg = f"{base_path}.jpg"
            if not os.path.exists(image_path_jpg):
                print(f"Looking for file at: {image_path_jpg}")
                print(f"Current directory: {os.getcwd()}")
                raise FileNotFoundError(f"No file found at {image_path_jpg}")

        # Load the image
        image = pygame.image.load(image_path_jpg)

        # Get the current window size from Pygame
        window_width, window_height = screen.get_size()

        # Scale the image to fit completely within the window
        scaled_image = scale_to_fit(image, window_width, window_height)

        # Center the scaled image
        new_width, new_height = scaled_image.get_size()
        x_offset = (window_width - new_width) // 2
        y_offset = (window_height - new_height) // 2

        # Fill screen and draw the image
        screen.fill((0, 0, 0))
        screen.blit(scaled_image, (x_offset, y_offset))
        pygame.display.flip()
        print(f"Displaying image: {image_path_jpg}")

    except Exception as e:
        print(f"Error loading image {image_path}: {e}")

def play_video_thread(video_path):
    """
    Plays video frames on the Pygame screen in a loop, 
    scaling them to fit while maintaining aspect ratio,
    and using pygame.time.Clock to keep a stable FPS.
    """
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
        if fps <= 0:
            fps = 30  # fallback if fps is 0 or can't be determined

        # Use a clock to manage playback speed
        video_clock = pygame.time.Clock()

        while cap.isOpened() and playing_video:
            ret, frame = cap.read()
            if not ret:
                break  # Reached end of video

            # Convert frame BGR -> RGB for pygame
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

            # Scale frame to fit the current window size
            window_width, window_height = screen.get_size()
            frame_height, frame_width = frame.shape[:2]

            # Calculate scale factor
            img_ratio = frame_width / frame_height
            screen_ratio = window_width / window_height

            if img_ratio > screen_ratio:
                # Constrain by width
                scaled_width = window_width
                scaled_height = int(scaled_width / img_ratio)
            else:
                # Constrain by height
                scaled_height = window_height
                scaled_width = int(scaled_height * img_ratio)

            # Resize using cv2
            frame_resized = cv2.resize(frame, (scaled_width, scaled_height), interpolation=cv2.INTER_AREA)
            # Convert to pygame surface
            frame_surface = pygame.surfarray.make_surface(frame_resized.swapaxes(0, 1))

            # Center the frame
            x_offset = (window_width - scaled_width) // 2
            y_offset = (window_height - scaled_height) // 2

            # Draw on screen
            screen.fill((0, 0, 0))
            screen.blit(frame_surface, (x_offset, y_offset))
            pygame.display.flip()

            # Tick the clock to limit to the video's fps
            video_clock.tick(fps)

            # Check pygame events (escape or quit)
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    playing_video = False

        cap.release()
        playing_video = False

    except Exception as e:
        print(f"Error playing video {video_path}: {e}")
        playing_video = False

def displayVideo(video_path):
    """
    Starts a new thread to play video. Stops any currently playing video first.
    """
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

def overlay(percent):
    """
    Draw a semi-transparent progress bar at the bottom of the screen.
    """
    window_width, window_height = screen.get_size()
    bar_height = 30
    bar_width = int(window_width * percent)

    # Create a transparent surface
    overlay_surface = pygame.Surface((window_width, bar_height), pygame.SRCALPHA)

    # Draw a semi-transparent black background
    pygame.draw.rect(overlay_surface, (0, 0, 0, 128), (0, 0, window_width, bar_height))

    # Draw progress portion (green)
    pygame.draw.rect(overlay_surface, (0, 255, 0, 200), (0, 0, bar_width, bar_height))

    # Blit the overlay at the bottom of the screen
    screen.blit(overlay_surface, (0, window_height - bar_height))
    pygame.display.flip()

################
# Main Program #
################

baud = 9600
current_dir = os.getcwd()
print(f"Current working directory: {current_dir}")

# Check if the assets folder is found; if not, try going one directory up
if not os.path.exists(os.path.join("P2M5", "PC_Client", "Assets")):
    if os.path.exists("PC_Client"):
        os.chdir("..")
        print(f"Changed working directory to: {os.getcwd()}")

# Find available serial ports
ports = list(port_list.comports())
if not ports:
    print("No serial ports found!")
    exit(1)

# Connect to the last port in the list
port = ports[-1].device
print(f"Connecting to port: {port}")
ser = Serial(port, baud, timeout=1)


start_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Intro.jpg")
displayImage(start_path)

running = True
while running:
    # Handle pygame-level events
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    try:
        # Check for incoming serial data
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').strip()
            if line:
                print(f"Received: {line}")

                # Start Video
                if line == "1/":
                #    video_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "video.mp4")
                #    displayVideo(video_path)
                    started = True

                # Load Question
                elif line.startswith("2<") and line.endswith(">/"):
                    countQuestions += 1
                    if countQuestions <= number_of_questions:
                        match = re.match(r"2<(\d+)>/$", line)
                        if match:
                            currentQuestion = int(match.group(1))
                            question_path = os.path.join("P2M5", "PC_Client", "Assets", "Questions", f"Q{currentQuestion+1}")
                            displayImage(question_path)
                            inQuestion = True

                # Show Correct
                elif line == "3/":
                    correct_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Correct")
                    displayImage(correct_path)
                    inQuestion = False

                # Time Data
                elif line.startswith("5<") and line.endswith(">/"):
                    match = re.match(r"5<(\d+)>/$", line)
                    if match:
                        currentQTime = int(match.group(1))
                        maxQuestionTime = currentQTime
                        print(f"Question time: {currentQTime}ms")

                # Show Wrong + correct answer
                elif line == "4/":
                    wrong_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Wrong")
                    displayImage(wrong_path)
                    time.sleep(1)  # 1-second delay
                    correct_ans_path = os.path.join("P2M5", "PC_Client", "Assets", "Responces", f"Q{currentQuestion+1}_CorrectAns")
                    displayImage(correct_ans_path)
                    inQuestion = False

                # End
                # End with points
                elif line.startswith("6<") and line.endswith(">/"):
                    match = re.match(r"6<(\d+)>/$", line)
                    if match:
                        quiz_points = int(match.group(1))
                        print(f"Quiz points: {quiz_points}")
                    end_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "End")
                    displayImage(end_path)
                    started = False
                    countQuestions = 0
                    inQuestion = False
                
                # Final data and custom ending based on points
                elif line.startswith("7<") and line.endswith(">/"):
                    # First show the "Later" image
                    later_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Later")
                    displayImage(later_path)
                    time.sleep(2)  # Pause to view the transition image

                    # Determine which ending to show based on points
                    if 991 <= quiz_points <= 999:
                        # Secret ending for special point range
                        ending_path = os.path.join("P2M5", "PC_Client", "Assets", "Endings", "Secret")
                    elif quiz_points > 1200:
                        # Amazing ending for >1200 points
                        ending_path = os.path.join("P2M5", "PC_Client", "Assets", "Endings", "Amazing")
                    elif 800 <= quiz_points <= 1200:
                        # Good ending for 800-1200 points
                        ending_path = os.path.join("P2M5", "PC_Client", "Assets", "Endings", "Good")
                    elif 400 <= quiz_points < 800:
                        # Bad ending for 400-800 points
                        ending_path = os.path.join("P2M5", "PC_Client", "Assets", "Endings", "Bad")
                    else:  # quiz_points < 400
                        # Worst ending for <400 points
                        ending_path = os.path.join("P2M5", "PC_Client", "Assets", "Endings", "Worst")

                    # Show the selected ending based on points
                    displayImage(ending_path)
                    time.sleep(3)  # Display the ending for 3 seconds

                    # Show the end screen like in command 6
                    end_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "End")
                    displayImage(end_path)
                    time.sleep(3)  # Display the end screen for 3 seconds
                    
                    # Return to intro screen
                    intro_path = os.path.join("P2M5", "PC_Client", "Assets", "Main", "Intro.jpg")
                    displayImage(intro_path)
                    
                    # Process data for logging as before
                    data_str = line[2:-2]
                    data_parts = data_str.split('><')
                    quiz_size = int(data_parts[0])
                    number_of_questions = int(data_parts[1])
                    questions_asked = data_parts[2].split('|')
                    question_time = data_parts[3].split('|')
                    total_time = int(data_parts[4])
                    motor_time = int(data_parts[6])  # Data points still in same positions
                    
                    # Log the data with the quiz_points we extracted earlier
                    csv_writer.writerow([
                        datetime.datetime.now().isoformat(),
                        quiz_size,
                        number_of_questions,
                        questions_asked,
                        question_time,
                        total_time,
                        quiz_points,  # Use our stored points value
                        motor_time
                    ])
                    
                    print(f"Logged data: {quiz_size}, {number_of_questions}, {questions_asked}, {total_time}, {quiz_points}, {motor_time}")
                    
                    # Reset game state variables
                    started = False
                    countQuestions = 0
                    inQuestion = False
                    
                    # Ensure data is written to disk immediately
                    csv_file.flush()
                    
                    # Close CSV file after logging is complete
                    try:
                        csv_file.close()
                        print("CSV file closed successfully")
                    except Exception as e:
                        print(f"Error closing CSV file: {e}")

                    

        # Update progress bar if in a question
        if inQuestion and currentQTime > 0:
            percent = currentQTime / maxQuestionTime
            overlay(percent)
            currentQTime -= clock.get_time()  # subtract elapsed time (ms) since last tick

        # Limit main loop to 30 FPS
        clock.tick(30)

    except Exception as e:
        print(f"Error: {e}")

# Clean up on exit
playing_video = False
if video_thread and video_thread.is_alive():
    video_thread.join(timeout=1.0)
ser.close()
pygame.quit()
