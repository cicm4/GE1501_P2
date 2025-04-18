#include <Servo.h>

// -------------------- PIN DEFINITIONS --------------------
// Digital pin numbers for each button:
#define RED_PIN 5
#define BLUE_PIN 3
#define GREEN_PIN 4
#define YELLOW_PIN 2

// Servo pins
#define SERVO_1_1 9
#define SERVO_1_2 10
#define SERVO_2_2 11

// Reset pin:
#define RESET_PIN 2

// -------------------- PROTOCOL MESSAGES --------------------
#define BAUD_RATE 9600
#define INITIAL_VIDEO_DURATION 1
#define QUIZ_LENGTH 5

// Simple protocol strings for printing over Serial:
#define Main "0/"
#define Start "1/"
#define Question_Start "2<"
#define Question_End ">/"
#define Correct "3/"
#define Wrong "4/"
#define Duration_Start "5<"
#define Duration_End ">/"
#define End_Header "6<"
#define End_Return ">/"
#define Data_Header "7<"
#define Data_Separator "><"
#define Data_Return ">/"


#define LOOP_RATE 2 // Loop rate in ms

// -------------------- PTS CONTROLS --------------------
#define PTS_LOSS_RATE 0.995 // Rate at which points are lost for time
#define RESPONCE_DELAY 2000 // Delay after a response is given in ms

// -------------------- RESPONCE ASSOSIATIONS --------------------
#define BUTTON_PRESSED 0
#define BUTTON_NOT_PRESSED -1

#define CORRECT_THREASHOLD 0
#define INCORRECT_RESPONCE -1
#define NO_RESPONCE -2

#define MOTOR_SPEED 90

#define RESET_DELAY 4000


// -------------------- HELPER MACROS --------------------
// sgn function: returns -1, 0, or 1 based on sign of x
// Source: https://forum.arduino.cc/t/sgn-sign-signum-function-suggestions/602445
#define sgn(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

// -------------------- GLOBAL CONSTANTS --------------------
const int QUIZ_SIZE = 15;         // Maximum number of questions
const int NUMBER_OF_Q = 5;
const float MICROS_CONST = 1.0e6; // For time-based scoring (micros to seconds)

const int SERVO_1_1_DIR = -1; // Direction for servo 1
const int SERVO_1_2_DIR = 1; // Direction for servo 2
const int SERVO_2_2_DIR = -1; // Direction for servo 3

int DIRS[3] = {SERVO_1_1_DIR, SERVO_1_2_DIR, SERVO_2_2_DIR};

// Global time variable used for measuring intervals, etc.
long globalTime = 0;

// -------------------- ENUMS & TYPES --------------------
enum ButtonIdentifier
{
  RED = RED_PIN,
  BLUE = BLUE_PIN,
  GREEN = GREEN_PIN,
  YELLOW = YELLOW_PIN,
  INVALID = 255
};

// -------------------- QUESTION CLASS --------------------
class Question
{
public:
  int8_t questionNum; // ID number or index of the question
  ButtonIdentifier correctAnswer;
  long questionTime; // Allowed time (in microseconds, presumably)
  int ptsLost;       // Points lost if answered incorrectly
  long timeSpent;    // (Currently unused) tracks how long was spent

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
    this->questionNum = questionNum;
    this->correctAnswer = correctAnswer;
    this->questionTime = questionTime;
    this->ptsLost = ptsLost;
    this->timeSpent = 0;
  }

  // Check if a given button press matches the correct answer
  bool isAnsRight(ButtonIdentifier ans)
  {
    return ans == correctAnswer;
  }

  // Update points based on time taken and question time
  // decreases points if time taken exceeds questionTime
  // uses reference to int to modify the points directly
  int updatePoints(long startTime, long currentTime, int pts)
  {
    if (currentTime - startTime > questionTime){
      return int(PTS_LOSS_RATE * pts);
    } else return pts;
  }

  // Print question number via Serial
  void askQuestion()
  {
    Serial.print(questionNum);
    Serial.print(";");
  }

  // Return 1 if correct, -1 if wrong
  int checkResponce(ButtonIdentifier ans)
  {
    if (ans == correctAnswer)
    {
      return 1;
    }
    return -1;
  }
};

// -------------------- SERVO TIME SERVICE --------------------
// For controlling servos instead of DC motors
// Utilizes the Servo library to control servo positions
class ServoTimeService 
{
private:
  long motorTime; // Tracks net servo move
  Servo &servo1;
  Servo &servo2;
  Servo &servo2c;


public:
  ServoTimeService(Servo &s1, Servo &s2, Servo &s2c)
      : motorTime(0), servo1(s1), servo2(s2), servo2c(s2c)
  {}

  void extractData(){
    Serial.print(motorTime);
    Serial.print(Data_Separator);
  }

  // Very rough "servo movement" logic
  void moveMotor(long duration, int8_t dir, int servoDir[3])
  {
    //360 servos take speed as a parameter instead of angle, yet its being
    //called an angle due to the library
    int theta_1 = (dir * MOTOR_SPEED * servoDir[0]) + 90;
    int theta_2 = (dir * MOTOR_SPEED * servoDir[1])+ 90;
    int theta_3 = (dir * MOTOR_SPEED * servoDir[2])+ 90;
    
    servo1.write(theta_1);
    servo2.write(theta_2);
    servo2c.write(theta_3);

    if (dir != 0)
    {
      motorTime += (duration * dir);
    }
    delay(12.5 * duration);
    servo1.write(90);
    servo2.write(90);
  }

  void resetMotor(int servoDir[3])
  {
    int8_t dir = -1 * sgn(motorTime);
    int theta = (dir * MOTOR_SPEED) + 90;
    servo1.write(theta * servoDir[0]);
    servo2.write(theta * servoDir[1]);
    servo2c.write(theta * servoDir[2]);
    delay(12.5 * abs(motorTime));
    motorTime = 0;
    servo1.write(90);
    servo2.write(90);
  }
};

// -------------------- QUIZ SERVICE --------------------
// Manages random questions, answers, and consequences
class QuizService
{
private:
  int pts;
  Question questions[QUIZ_SIZE];
  long questionTime[QUIZ_SIZE];
  bool questionsAsked[QUIZ_SIZE];
  bool qInProcess;

  ServoTimeService &servoTimeService; // Reference to servo time service
  int count;
  int questionNum;

public:
  int qpts; // Another points variable
  int8_t latestQID;
  int dirs[3]; // Directions for servos (if using servos)
  long currentQuestionStartTime;
  long absInitTime;

  QuizService(Question qArray[QUIZ_SIZE], ServoTimeService &sts,
              int servoDirs[3])
      : servoTimeService(sts), qpts(1000), pts(0), questionNum(0),
        latestQID(-1), qInProcess(false), dirs{servoDirs[0], servoDirs[1], servoDirs[2]}, count(0), absInitTime(globalTime)
  {
    for (int i = 0; i < QUIZ_SIZE; i++)
    {
      questions[i] = qArray[i];
      questionsAsked[i] = false;
      questionTime[i] = 0;
    }
  }

  void extractData(){
    Serial.print(Data_Header);
    Serial.print(QUIZ_SIZE);
    Serial.print(Data_Separator);
    Serial.print(QUIZ_LENGTH);
    Serial.print(Data_Separator);

    Serial.print("|");
    //print out question data
    for (int i = 0; i < QUIZ_SIZE; i++)
    {
      Serial.print(questionsAsked[i]);
      Serial.print("|");
    }
    Serial.print("|");
    Serial.print(Data_Separator);

    //print out question time
    Serial.print("|");
    for (int i = 0; i < QUIZ_SIZE; i++)
    {
      Serial.print(questionTime[i]);
      Serial.print("|");
    }
    Serial.print("|");

    Serial.print(Data_Separator);

    //print out time taken
    long dt = globalTime - absInitTime;
    Serial.print(dt);
    Serial.print(Data_Separator);

    //print out qpts
    Serial.print(qpts);
    Serial.print(Data_Separator);
    
    //print out servo data
    servoTimeService.extractData();

    //print out data return
    Serial.println(Data_Return);
  }

  // Randomly pick a question that has not been asked
  void AskRandomQuestion()
  {
    int randId = random(0, QUIZ_SIZE);
    if (!questionsAsked[randId])
    {
      questionsAsked[randId] = true;
      qInProcess = true;
      latestQID = randId;
      currentQuestionStartTime = globalTime;
      pts = questions[randId].ptsLost;
    }
    else
    {
      // If that question was already asked, pick another
      AskRandomQuestion();
    }
  }

  // Check user’s response for the question ID:
  int checkResponce(int id, ButtonIdentifier ans)
  {
    return questions[id].checkResponce(ans);
  }

  // Update points based on the time taken to answer
  void updatePts(long startTime)
  {
    pts = questions[latestQID].updatePoints(startTime, globalTime, pts);

  }

  // Apply movement or scoring depending on correct/wrong
  bool applyConsequence(int response)
  {
    count++;
    int dir;
    if (response == 1)
    {
      dir = 1; // correct
      servoTimeService.moveMotor(pts, dir, dirs);

    }
    else
    {
      dir = -1; // incorrect
    }

    questionTime[latestQID] = globalTime - currentQuestionStartTime;

    qInProcess = false;
    // Move motor/servo for 'ptsLost' milliseconds


    Serial.print("Count: ");
    Serial.println(count);

    if (response == 1)
    {
      qpts += (pts * dir);
    } else
    {
      qpts -= 200;
    }
    // Adjust quiz scoring
    return (response == 1);
  }

  // Check if # of questions asked is greater
  bool isFinished()
  {
    return (count >= NUMBER_OF_Q);
  }

  // Reset entire game
  void resetGame()
  {
    servoTimeService.resetMotor(dirs);
    qpts = 1000;
    pts = 0;
    questionNum = 0;
    latestQID = -1;
    qInProcess = false;
    count = 0;

    // Mark all questions as unasked
    for (int i = 0; i < QUIZ_SIZE; i++)
    {
      questionsAsked[i] = false;
      questions[i].timeSpent = 0;
    }
  }

  // Return time limit for question with ID = id
  long duration(int8_t id)
  {
    return questions[id].questionTime;
  }

  // InProgress check
  bool isInProgress()
  {
    return qInProcess;
  }
};

// -------------------- BUTTON SERVICE --------------------
// Manages reading buttons from digital pins
class ButtonService {
private:
  bool buttons[4]; // Each element corresponds to a color button

  // Helper function to convert index -> enum
  ButtonIdentifier idToIdentifier(int8_t id ){
    switch (id)
    {
    case 0:
      return RED;
    case 1:
      return BLUE;
    case 2:
      return GREEN;
    case 3:
      return YELLOW;
    default:
      return INVALID;
    }
  }

public:
  ButtonService()
  {
    for (int i = 0; i < 4; i++)
    {
      buttons[i] = LOW;
    }
  }

  // Read the current state of each button
  void readButtons()
  {
    for (int i = 0; i < 4; i++)
    {
      ButtonIdentifier pinEnum = idToIdentifier(i);
      buttons[i] = !digitalRead(pinEnum);
    }
    /*
    int rand = random(0, 3000);
    if (rand > 2998){
      int otherRand = random(0, 3);
      buttons[otherRand] = HIGH;
    }
    */
  }

  // Reset stored button states
  void resetButtons()
  {
    for (int i = 0; i < 4; i++)
    {
      buttons[i] = LOW;
    }
  }

  // Return which button was pressed (if any)
  int8_t buttonPressed()
  {
    for (int i = 0; i < 4; i++)
    {
      if (buttons[i])
      {
        return idToIdentifier(i);
      }
    }
    return -1; // no button pressed
  }
};

// -------------------- GLOBAL INSTANTIATIONS --------------------
// Initialize button service
ButtonService buttonService;

// NOTE:
// A - RED
// B - GREEN
// C - BLUE
// D - yellow
//  Sample array of questions
Question qArray[QUIZ_SIZE] = {
    Question(1, GREEN, 8000, 200),  // B
    Question(2, RED, 8000, 200),    // A
    Question(3, BLUE, 8000, 200),   // C
    Question(4, GREEN, 8000, 200),  // B
    Question(5, YELLOW, 8000, 200), // D
    Question(6, GREEN, 8000, 200),  // B
    Question(7, RED, 8000, 200),    // A
    Question(8, GREEN, 8000, 200),  // B
    Question(9, YELLOW, 8000, 200), // D
    Question(10, GREEN, 8000, 200), // B
    Question(11, BLUE, 8000, 200),  // C
    Question(12, RED, 8000, 200),   // A
    Question(13, RED, 8000, 200),   // A
    Question(14, GREEN, 8000, 200), // B
    Question(15, BLUE, 8000, 200)   // C
};

bool quizBegan = false;
bool quizReset = false;

Servo servo_1_1;
Servo servo_1_2;
Servo servo_2_2;

ServoTimeService servoTimeService(servo_1_1, servo_1_2, servo_2_2);

QuizService quizService(qArray, servoTimeService, DIRS);

static void resetGame(){
  quizReset = true;
}

void setup() {
  Serial.begin(BAUD_RATE);

  // Attach servo objects to servo pins
  servo_1_1.attach(SERVO_1_1);
  servo_1_2.attach(SERVO_1_2);
  servo_2_2.attach(SERVO_2_2);

  digitalWrite(RED_PIN, HIGH);
  digitalWrite(BLUE_PIN, HIGH);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(YELLOW_PIN, HIGH);

  // Set pin modes for button inputs (internal pull-up)
  pinMode(RED_PIN, INPUT_PULLUP);
  pinMode(BLUE_PIN, INPUT_PULLUP);
  pinMode(GREEN_PIN, INPUT_PULLUP);
  pinMode(YELLOW_PIN, INPUT_PULLUP);

  // attach interrupt to reset pin
  pinMode(RESET_PIN, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(RESET_PIN), []()
                  //{resetGame();}, FALLING);

  quizBegan = false; // Initialize quiz state

  Serial.println(Main);
}

// Track which button and response are pressed
int8_t buttonPressedVar = BUTTON_NOT_PRESSED;
int8_t currentResponse = NO_RESPONCE;

void loop() {
  if (!quizBegan)
  {
    randomSeed(millis());
    buttonService.readButtons();
    buttonPressedVar = buttonService.buttonPressed();
    if (buttonPressedVar != BUTTON_NOT_PRESSED)
    {
      quizBegan = true;
      Serial.println(Start);
      delay(INITIAL_VIDEO_DURATION);
      delay(1000);
    }
  }
  else
  {
    // Record time for potential scoring or timing
    globalTime = millis();

    // If a question is currently active:
    if (quizService.isInProgress()) {
      if (quizService.isFinished()) {
      Serial.print(End_Header);
      Serial.print(quizService.qpts);
      Serial.println(End_Return);
      quizReset = false;
      quizBegan = false;
      quizService.extractData();

      // Reset so we can play again
      quizService.resetGame();
      buttonService.resetButtons();

      delay(RESET_DELAY);
      return;
    }
      // Read input from user
      buttonService.readButtons();
      buttonPressedVar = buttonService.buttonPressed();

      if (buttonPressedVar != BUTTON_NOT_PRESSED)
      {
        // Check correctness of that response
        currentResponse = quizService.checkResponce(quizService.latestQID, (ButtonIdentifier)buttonPressedVar);
        Serial.println(currentResponse);
      }
      // If correct
      if (currentResponse > CORRECT_THREASHOLD)
      {
        Serial.println(Correct);
        quizService.applyConsequence(currentResponse);
        currentResponse = NO_RESPONCE;
        delay(RESPONCE_DELAY);
      }
      // If wrong
      else if (currentResponse == INCORRECT_RESPONCE)
      {
        
        Serial.println(Wrong);
        quizService.applyConsequence(currentResponse);
        currentResponse = NO_RESPONCE;
        delay(RESPONCE_DELAY);
      }
      else
      {
        // If no button pressed, update current question time
        quizService.updatePts(quizService.currentQuestionStartTime);
      }
    }
    else
    {
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
  }

  delay(LOOP_RATE); // minimal delay
}