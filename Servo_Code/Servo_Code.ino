#include <Servo.h>

// -------------------- PIN DEFINITIONS --------------------
// Digital pin numbers for each button:
#define RED_PIN    4
#define BLUE_PIN   7
#define GREEN_PIN  3
#define YELLOW_PIN 5

// Servo pins (corrected to have 4 unique pins):
#define SERVO_1_1 11
#define SERVO_1_2 10
#define SERVO_2_1 9
#define SERVO_2_2 6

// Reset pin:
#define RESET_PIN  2

// -------------------- PROTOCOL MESSAGES --------------------
#define BAUD_RATE 9600
#define INITIAL_VIDEO_DURATION 6000
#define QUIZ_LENGTH 5

// Simple protocol strings for printing over Serial:
#define Start          "1/"
#define Question_Start "2<"
#define Question_End   ">/"
#define Correct        "3/"
#define Wrong          "4/"
#define Duration_Start "5<"
#define Duration_End   ">/"
#define End            "6"

#define LOOP_RATE 2 // Loop rate in ms

// -------------------- PTS CONTROLS --------------------
#define PTS_LOSS_RATE 0.975 // Rate at which points are lost for time
#define RESPONCE_DELAY 2000 // Delay after a response is given in ms

// -------------------- RESPONCE ASSOSIATIONS --------------------
#define BUTTON_PRESSED 0
#define BUTTON_NOT_PRESSED -1

#define CORRECT_THREASHOLD 0
#define INCORRECT_RESPONCE -1
#define NO_RESPONCE -2

/*//--------------------- MOTOR CONTROLLER PINS --------------------
#define PWMA_IN_PIN  8
;#define INA1_PIN     12
;#define INA2_PIN     13
#define PWMB_IN_PIN  4
#define INB1_PIN     5
#define INB2_PIN     6
*/

// -------------------- HELPER MACROS --------------------
// sgn function: returns -1, 0, or 1 based on sign of x
// Source: https://forum.arduino.cc/t/sgn-sign-signum-function-suggestions/602445
#define sgn(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

// -------------------- GLOBAL CONSTANTS --------------------
const int QUIZ_SIZE   = 15;   // Maximum number of questions
const int MOTOR_SPEED = 128;  // Generic PWM speed for motors (0-255)
const float MICROS_CONST = 1.0e6; // For time-based scoring (micros to seconds)

const int SERVO_1_1_DIR = 1; // Direction for servo 1
const int SERVO_1_2_DIR = 1; // Direction for servo 2
const int SERVO_2_1_DIR = 1; // Direction for servo 3
const int SERVO_2_2_DIR = 1; // Direction for servo 4

const int DIRS[4] = {SERVO_1_1_DIR, SERVO_1_2_DIR, SERVO_2_1_DIR, SERVO_2_2_DIR};

// Global time variable used for measuring intervals, etc.
long globalTime = 0;

// -------------------- ENUMS & TYPES --------------------
enum ButtonIdentifier {
  RED = RED_PIN,
  BLUE = BLUE_PIN,
  GREEN = GREEN_PIN,
  YELLOW = YELLOW_PIN,
  INVALID = 255
};

// -------------------- QUESTION CLASS --------------------
class Question {
  public:
    int8_t questionNum;        // ID number or index of the question
    ButtonIdentifier correctAnswer;
    long questionTime;         // Allowed time (in microseconds, presumably)
    int ptsLost;               // Points lost if answered incorrectly
    long timeSpent;            // (Currently unused) tracks how long was spent

    // Default constructor
    Question()
      : questionNum(0), correctAnswer(RED), questionTime(0),
        ptsLost(0), timeSpent(0)
    {
    }

    // Overloaded constructor
    Question(int8_t questionNum, ButtonIdentifier correctAnswer,
             long questionTime, int ptsLost)
    {
      this->questionNum   = questionNum;
      this->correctAnswer = correctAnswer;
      this->questionTime  = questionTime;
      this->ptsLost       = ptsLost;
      this->timeSpent     = 0;
    }

    // Check if a given button press matches the correct answer
    bool isAnsRight(ButtonIdentifier ans) {
      return ans == correctAnswer;
    }

    // Update points based on time taken and question time
    // decreases points if time taken exceeds questionTime
    // uses reference to int to modify the points directly
    void updatePoints(long startTime, long currentTime, int &pts) {
      (currentTime - startTime > questionTime) ? int(PTS_LOSS_RATE * pts) : pts;
    }

    // Print question number via Serial
    void askQuestion() {
      Serial.print(questionNum);
      Serial.print(";");
    }

    // Return 1 if correct, -1 if wrong
    int checkResponce(ButtonIdentifier ans){
      if(ans == correctAnswer){
        return 1;
      }
      return -1;
    }
};

// -------------------- MOTOR CONTROLLER CLASS --------------------
// For controlling a single DC motor driver channel
/*
class MotorController {
  private:
    int pwma_in;  // pin for PWM
    int in1_pin;  // forward?
    int in2_pin;  // reverse?

  public:
    // Constructor
    MotorController(int pwma_in, int in1_pin, int in2_pin) {
      this->pwma_in = pwma_in;
      this->in1_pin = in1_pin;
      this->in2_pin = in2_pin;
    }

    // Set the direction and PWM speed of the motor
    void setMotor(int dir, int pwm_val) {
      analogWrite(pwma_in, pwm_val);
      Serial.print("dir: ");
      Serial.println(dir);

      if(dir == 1) {
        // Forward
        digitalWrite(in1_pin, LOW);
        digitalWrite(in2_pin, HIGH);
      } else if (dir == -1) {
        // Reverse
        digitalWrite(in1_pin, HIGH);
        digitalWrite(in2_pin, LOW);
      } else {
        // Stop
        digitalWrite(in1_pin, LOW);
        digitalWrite(in2_pin, LOW);
      }
    }
};


// -------------------- DC MOTOR TIME SERVICE --------------------
// Tracks how long a DC motor has moved in one direction
class MotorTimeService {
  private:
    long motorTime;                 // Tracks net time in one direction
    MotorController motorController1;
    MotorController motorController2;

  public:
    // Constructor
    MotorTimeService(MotorController mc1, MotorController mc2)
      : motorTime(0), motorController1(mc1), motorController2(mc2)
    {}

    // Move motors for 'duration' ms in direction 'dir' (+1 or -1)
    void moveMotor(long duration, int8_t dir) {
      motorController1.setMotor(dir, MOTOR_SPEED);
      motorController2.setMotor(dir, MOTOR_SPEED);

      if(dir != 0) {
        motorTime += duration * dir; // accumulate direction & time
      }
      delay(duration);

      // Stop motors
      motorController1.setMotor(0, 0);
      motorController2.setMotor(0, 0);
    }

    // Move motors back so net movement is zero
    void resetMotor() {
      int8_t dir = -1 * sgn(motorTime);
      motorController1.setMotor(dir, MOTOR_SPEED);
      motorController2.setMotor(dir, MOTOR_SPEED);
      delay(abs(motorTime));

      motorTime = 0; // reset net movement

      motorController1.setMotor(0, 0);
      motorController2.setMotor(0, 0);
    }
};

*/

// -------------------- SERVO TIME SERVICE --------------------
// For controlling servos instead of DC motors
// Utilizes the Servo library to control servo positions
class ServoTimeService {
  private:
    long motorTime;  // Tracks net servo move
    Servo servo1;
    Servo servo2;
    Servo servo3;
    Servo servo4;

  public:
    // FIX: The constructor name below should match the class name (ServoTimeService).
    // Changed it from "MotorTimeService" to "ServoTimeService".
    ServoTimeService(Servo s1, Servo s2, Servo s3, Servo s4)
      : motorTime(0), servo1(s1), servo2(s2), servo3(s3), servo4(s4)
    {}

    // Very rough "servo movement" logic
    void moveMotor(long duration, int8_t dir, int8_t servoDir[4]) {
      // Writing 'dir * MOTOR_SPEED' to a servo normally doesn't work.
      // Typically, you write an angle 0-180.  This code is just an example.
      servo1.write(dir * MOTOR_SPEED * servoDir[0]);
      servo2.write(dir * MOTOR_SPEED * servoDir[1]);
      servo3.write(dir * MOTOR_SPEED * servoDir[2]);
      servo4.write(dir * MOTOR_SPEED * servoDir[3]);

      if(dir != 0) {
        motorTime += duration;
      }
      delay(duration);
    }

    void resetMotor() {
      int8_t dir = -1 * sgn(motorTime);
      // Typically, you'd write angles (e.g. 90 = center)
      servo1.write(90);
      servo2.write(90);
      delay(abs(motorTime));
      motorTime = 0;
    }
};

// -------------------- QUIZ SERVICE --------------------
// Manages random questions, answers, and consequences
class QuizService {
  private:
    int pts;                // Possibly unused, keep for scoring logic
    Question questions[QUIZ_SIZE];
    bool questionsAsked[QUIZ_SIZE];
    bool qInProcess;
    long currentQuestionStartTime; // Time for current question

    ServoTimeService &servoTimeService; // Reference to servo time service
    int count
    int questionNum;

  public:
    int qpts;                 // Another points variable
    int8_t latestQID;
    int dirs[4]; // Directions for servos (if using servos)

    // FIX: Changed second param to MotorTimeService if you plan to pass that in
    QuizService(Question qArray[QUIZ_SIZE], ServoTimeService &sts, 
                int8_t servoDirs[4])
      : motorTimeService(sts), qpts(1000), pts(0), questionNum(0),
        latestQID(-1), qInProcess(false), dirs{servoDirs[0], servoDirs[1], servoDirs[2], servoDirs[3]}
    {
      for (int i = 0; i < QUIZ_SIZE; i++) {
        questions[i]      = qArray[i];
        questionsAsked[i] = false;
      }
    }

    // Randomly pick a question that has not been asked
    void AskRandomQuestion() {
      int randId = random(0, QUIZ_SIZE);
      if (!questionsAsked[randId]) {
        questionsAsked[randId] = true;
        qInProcess = true;
        latestQID = randId;
        currentQuestionStartTime = globalTime;
        pts = questions[randId].ptsLost;
      } else {
        // If that question was already asked, pick another
        AskRandomQuestion();
      }
    }

    // Check user’s response for the question ID:
    int checkResponce(int id, ButtonIdentifier ans){
      return questions[id].checkResponce(ans);
    }

    // Update points based on the time taken to answer
    void updatePts(long startTime)
    {
      questions[id].updatePoints(startTime, globalTime, pts);
    }

    // Apply movement or scoring depending on correct/wrong
    bool applyConsequence(int response){
      int dir;
      if(response == 1){
        dir = 1;   // correct
      } else {
        dir = -1;  // incorrect
      }

      qInProcess = false;

      // Move motor/servo for 'ptsLost' milliseconds
      servoTimeService.moveMotor(questions[latestQID].ptsLost, dir);

      // Adjust quiz scoring
      qpts += (questions[latestQID].ptsLost * dir);
      return (response == 1);
    }

    // Check if all questions are answered
    bool isFinished(){
      return (count == QUIZ_SIZE) && !qInProcess;
    }

    // Reset entire game
    void resetGame() {
      servoTimeService.resetMotor();
      qpts        = 1000;
      pts         = 0;
      questionNum = 0;
      latestQID   = -1;
      qInProcess  = false;

      // Mark all questions as unasked
      for (int i = 0; i < QUIZ_SIZE; i++) {
        questionsAsked[i] = false;
      }
    }

    // Return time limit for question with ID = id
    long duration(int8_t id){
      return questions[id].questionTime;
    }

    // InProgress check
    bool isInProgress(){
      return qInProcess;
    }
};

// -------------------- BUTTON SERVICE --------------------
// Manages reading buttons from digital pins
class ButtonService {
  private:
    bool buttons[4];  // Each element corresponds to a color button

    // Helper function to convert index -> enum
    ButtonIdentifier idToIdentifier(int8_t id){
      switch(id){
        case 0: return RED;
        case 1: return BLUE;
        case 2: return GREEN;
        case 3: return YELLOW;
        default: return INVALID;
      }
    }

  public:
    ButtonService() {
      for (int i = 0; i < 4; i++) {
        buttons[i] = LOW;
      }
    }

    // Read the current state of each button
    void readButtons() {
      for (int i = 0; i < 4; i++) {
        ButtonIdentifier pinEnum = idToIdentifier(i);
        buttons[i] = digitalRead(pinEnum);
      }
    }

    // Reset stored button states
    void resetButtons() {
      for (int i = 0; i < 4; i++) {
        buttons[i] = LOW; 
      }
    }

    // Return which button was pressed (if any)
    int8_t buttonPressed() {
      for (int i = 0; i < 4; i++) {
        if (buttons[i]) {
          return idToIdentifier(i);
        }
      }
      return -1; // no button pressed
    }
};

// -------------------- GLOBAL INSTANTIATIONS --------------------
// Initialize button service
ButtonService buttonService;


//NOTE:
//A - RED
//B - GREEN
//C - BLUE
//D - yellow
// Sample array of questions
Question qArray[QUIZ_SIZE] = {
  Question(1,  GREEN, 150, 200), //B
  Question(2,  RED, 150, 200), //A
  Question(3,  BLUE, 150, 200), //C
  Question(4,  GREEN, 150, 200), //B
  Question(5,  YELLOW, 150, 200), //D
  Question(6,  GREEN, 150, 200), //B
  Question(7,  RED,  150, 200), //A
  Question(8,  GREEN, 150, 200), //B
  Question(9,  YELLOW, 150, 200), //D
  Question(10, GREEN, 150, 200), //B
  Question(11, BLUE, 150, 200), //C
  Question(12, RED, 150, 200), //A
  Question(13, RED, 150, 200), //A
  Question(14, GREEN, 150, 200), //B
  Question(15, BLUE, 150, 200) //C
};

// If you want servo-based logic, instantiate them here:
Servo servo_1_1;
Servo servo_1_2;
Servo servo_2_1;
Servo servo_2_2;

void setup() {
  Serial.begin(BAUD_RATE);

  // Attach servo objects to servo pins
  servo_1_1.attach(SERVO_1_1);
  servo_1_2.attach(SERVO_1_2);
  servo_2_1.attach(SERVO_2_1);
  servo_2_2.attach(SERVO_2_2);

  QuizService quizService(qArray, ServoTimeService(servo_1_1, servo_1_2, servo_2_1, servo_2_2), dirs);

  // Set pin modes for motor control pins
  pinMode(PWMA_IN_PIN, OUTPUT);
  pinMode(INA1_PIN,    OUTPUT);
  pinMode(INA2_PIN,    OUTPUT);
  pinMode(PWMB_IN_PIN, OUTPUT);
  pinMode(INB1_PIN,    OUTPUT);
  pinMode(INB2_PIN,    OUTPUT);

  // Set pin modes for button inputs (internal pull-up)
  pinMode(RED_PIN,    INPUT_PULLUP);
  pinMode(BLUE_PIN,   INPUT_PULLUP);
  pinMode(GREEN_PIN,  INPUT_PULLUP);
  pinMode(YELLOW_PIN, INPUT_PULLUP);

  // attach interrupt to reset pin
  pinMode(RESET_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), []() {
    quizService.resetGame();
  }, FALLING);

  // Announce start, then wait for the "intro video" period
  Serial.println(Start);
  delay(INITIAL_VIDEO_DURATION);
}

// Track which button and response are pressed
int8_t buttonPressedVar = BUTTON_NOT_PRESSED;
int8_t currentResponse  = NO_RESPONCE;

void loop() {
  // Record time for potential scoring or timing
  globalTime = micros();

  // If a question is currently active:
  if(quizService.isInProgress()){
    // Read input from user
    buttonService.readButtons();
    buttonPressedVar = buttonService.buttonPressed();

    if(buttonPressedVar != BUTTON_NOT_PRESSED){
      // Check correctness of that response
      currentResponse = quizService.checkResponce(quizService.latestQID, (ButtonIdentifier)buttonPressedVar);
      Serial.println(currentResponse);
    }
    // If correct
    if(currentResponse > CORRECT_THREASHOLD){
      quizService.applyConsequence(currentResponse);
      Serial.println(Correct);
      currentResponse = NO_RESPONCE;
      delay(RESPONCE_DELAY);
    }
    // If wrong
    else if (currentResponse == INCORRECT_RESPONCE){
      quizService.applyConsequence(currentResponse);
      Serial.println(Wrong);
      currentResponse = NO_RESPONCE;
      delay(RESPONCE_DELAY);
    } else {
      // If no button pressed, update current question time
      quizService.updatePts(quizService.currentQuestionStartTime);
    }
  }
  else {
    // Pick a random new question
    quizService.AskRandomQuestion();

    // Print the question ID and time limit
    Serial.print(Question_Start);
    Serial.print(quizService.latestQID);
    Serial.println(Question_End);

    Serial.print(Duration_Start);
    Serial.print(quizService.duration(quizService.latestQID));
    Serial.println(Duration_End);
  }
  
  // Check if the quiz is finished
  if(quizService.isFinished()){
    Serial.println(End);
    // Reset so we can play again
    quizService.resetGame();
    buttonService.resetButtons();
  }

  delay(LOOP_RATE); // minimal delay
}