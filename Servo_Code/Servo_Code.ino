//Red button pin
#define RED_PIN 4
//Blue button pin
#define BLUE_PIN 2
//Green button pin
#define GREEN_PIN 3
//Yellow button pin
#define YELLOW_PIN 5

#define Servo_1_1_pin 11
#define Servo_1_2_pin 10
#define Servo_2_1_pin 9
#define Servo_2_1_pin 8

#define RESET_PIN 6


#define INITIAL_VIDEO_DURATION 6000

#define Start "1/"
#define Question_Start "2<"
#define Question_End ">/"
#define Correct "3/"
#define Wrong "4/"
#define Duration_Start "5<"
#define Duration_End ">/"
#define End "6"

//source for sgn function: https://forum.arduino.cc/t/sgn-sign-signum-function-suggestions/602445
#define sgn(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))

const int QUIZ_SIZE = 15;
const int MOTOR_SPEED = 128;
const float MICROS_CONST = 1.0e6;

long globalTime = 0;

enum ButtonIdentifier {
  RED = RED_PIN,
  BLUE = BLUE_PIN,
  GREEN = GREEN_PIN,
  YELLOW = YELLOW_PIN,
  INVALID = 255
};

class Question {
  public:
    int8_t questionNum;
    ButtonIdentifier correctAnswer;
    long questionTime;
    int ptsLost;
    long timeSpent;

    Question()
      : questionNum(0), correctAnswer(RED), questionTime(0), ptsLost(0), timeSpent(0) {
    }

    Question(int8_t questionNum, ButtonIdentifier correctAnswer, long questionTime, int ptsLost) {
      this->questionNum = questionNum;
      this->correctAnswer = correctAnswer;
      this->questionTime = questionTime;
      this->ptsLost = ptsLost;
      this->timeSpent = 0;
    }

    bool isAnsRight(ButtonIdentifier ans) {
      return ans == correctAnswer;
    }

    int updatePoints(ButtonIdentifier ans, long startTime, long ansTime, int pts) {
      if (ans == correctAnswer) {
        pts += (ptsLost / 2);
      } else {
        pts -= ptsLost;
      }
      if ((ansTime - startTime) > questionTime) {
        pts -= ((float)(ansTime - startTime)) / MICROS_CONST;
      }
      return pts;
    }

    void askQuestion() {
      Serial.print(questionNum);
      Serial.print(";");
    }

    int checkResponce(ButtonIdentifier ans){
      if(ans == correctAnswer){
        return 1;
      }
      return -1;
    }
};

class MotorController {
  private:
    int pwma_in;
    int in1_pin;
    int in2_pin;

  public:
    MotorController(int pwma_in, int in1_pin, int in2_pin) {
      this->pwma_in = pwma_in;
      this->in1_pin = in1_pin;
      this->in2_pin = in2_pin;
    }

    void setMotor(int dir, int pwm_val) {
      analogWrite(pwma_in, pwm_val);
      Serial.print("dir: ");
      Serial.println(dir);
      if(dir == 1) {
        digitalWrite(in1_pin, LOW);
        digitalWrite(in2_pin, HIGH);
      } else if (dir == -1) {
        digitalWrite(in1_pin, HIGH);
        digitalWrite(in2_pin, LOW);
      } else {
        digitalWrite(in1_pin, LOW);
        digitalWrite(in2_pin, LOW);
      }
    }
};

class MotorTimeService {
  private:
    long motorTime;
    MotorController motorController1;
    MotorController motorController2;

  public:
    MotorTimeService(MotorController mc1, MotorController mc2)
      : motorTime(0), motorController1(mc1), motorController2(mc2)
    {}

    void moveMotor(long duration, int8_t dir) {
      motorController1.setMotor(dir, MOTOR_SPEED);
      motorController2.setMotor(dir, MOTOR_SPEED);
      if(dir)
      motorTime = (motorTime + (0.5625 * (sgn(dir) * duration)));
      delay(duration);
      motorController1.setMotor(dir, 0);
      motorController2.setMotor(dir, 0);
    }

    void resetMotor() {
      int8_t dir = -1 * sgn(motorTime);
      motorController1.setMotor(dir, MOTOR_SPEED);
      motorController2.setMotor(dir, MOTOR_SPEED);
      delay(abs(motorTime));
      motorTime = 0;
      motorController1.setMotor(dir, 0);
      motorController2.setMotor(dir, 0);
    }
};

class QuizService {
  private:
    int pts;
    Question questions[QUIZ_SIZE];
    MotorTimeService motorTimeSerice;
    int questionNum;
    bool questionsAsked[QUIZ_SIZE];
    bool qInProcess;

  public:
    int qpts;
    int8_t latestQID;
    QuizService(Question qArray[QUIZ_SIZE], MotorTimeService mts)
      : motorTimeSerice(mts), qpts(1000), pts(0), questionNum(0), latestQID(-1), qInProcess(false)
    {
      for (int i = 0; i < QUIZ_SIZE; i++) {
        questions[i] = qArray[i];
        questionsAsked[i] = false;
      }
    }

    void AskRandomQuestion() {
      int randId = random(0, QUIZ_SIZE);
      if (!questionsAsked[randId]) {
        questionsAsked[randId] = true;
        qInProcess = true;
        latestQID = randId;
      } else {
        AskRandomQuestion();
      }
    }

    int checkResponce(int id, ButtonIdentifier ans){
      return questions[id].checkResponce(ans);
    }

    bool applyConsequence(int responce){
      int dir;
      if(responce == 1){
        dir = 1;
      } else {
        dir = -1;
      }
      qInProcess = false;
      motorTimeSerice.moveMotor(questions[latestQID].ptsLost, dir);
      qpts += (questions[latestQID].ptsLost * dir);
      return (responce == 1);
    }

    bool isFinished(){
      bool finished = true;
      for (int i = 0; i < QUIZ_SIZE; i++) {
        if(questionsAsked[i] == false){
          finished = false;
        }
      }
      return finished && !qInProcess;
    }

    void resetGame() {
      motorTimeSerice.resetMotor();
      qpts = 1000;
      pts = 0;
      questionNum = 0;
      latestQID = -1;
      qInProcess = false;
      for (int i = 0; i < QUIZ_SIZE; i++) {
        questionsAsked[i] = false;
      }
    }

    long duration(int8_t id){
      return questions[id].questionTime;
    }

    bool isInProgress(){
      return qInProcess;
    }
};

class ButtonService{
  private:
    bool buttons[4];

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

    void readButtons() {
      for (int i = 0; i < 4; i++) {
        ButtonIdentifier pinEnum = idToIdentifier(i);
        buttons[i] = digitalRead(pinEnum);
      }
    }

    void resetButtons() {
      for (int i = 0; i < 4; i++) {
        buttons[i] = LOW; 
      }
    }

    int8_t buttonPressed() {
      for (int i = 0; i < 4; i++) {
        if (buttons[i]) {
          return idToIdentifier(i);
        }
      }
      return -1;
    }
};

MotorController motorController1(PWMA_IN_PIN, INA1_PIN, INA2_PIN);
MotorController motorController2(PWMB_IN_PIN, INB1_PIN, INB2_PIN);

MotorTimeService motorTimeService(motorController1, motorController2);
ButtonService buttonService;

Question qArray[15] = {
  Question(1,  RED,    1, 101),
  Question(2,  BLUE,   1, 102),
  Question(3,  GREEN,  1, 103),
  Question(4,  YELLOW, 1, 104),
  Question(5,  RED,    1, 105),
  Question(6,  BLUE,   1, 106),
  Question(7,  GREEN,  1, 107),
  Question(8,  YELLOW, 1, 108),
  Question(9,  RED,    1, 109),
  Question(10, BLUE,   1, 110),
  Question(11, GREEN,  1, 111),
  Question(12, YELLOW, 1, 112),
  Question(13, RED,    1, 113),
  Question(14, BLUE,   1, 114),
  Question(15, GREEN,  1, 115)
};

QuizService quizService(qArray, motorTimeService);

void setup() {
  Serial.begin(9600);

  pinMode(PWMA_IN_PIN, OUTPUT);
  pinMode(INA1_PIN, OUTPUT);
  pinMode(INA2_PIN, OUTPUT);

  pinMode(PWMB_IN_PIN, OUTPUT);
  pinMode(INB1_PIN, OUTPUT);
  pinMode(INB2_PIN, OUTPUT);

  pinMode(RED_PIN, INPUT_PULLUP);
  pinMode(BLUE_PIN, INPUT_PULLUP);
  pinMode(GREEN_PIN, INPUT_PULLUP);
  pinMode(YELLOW_PIN, INPUT_PULLUP);

  Serial.println(Start);
  delay(INITIAL_VIDEO_DURATION);
}

int8_t buttonPressedVar;
int8_t currentResponce;

void loop() {
  globalTime = micros();
  //Serial.print("InProgress: ");
  //Serial.println(quizService.isInProgress());
  if(quizService.isInProgress()){
    buttonService.readButtons();
    buttonPressedVar = buttonService.buttonPressed();
    //Serial.print("buttonPressedBar: ");
    //Serial.println(buttonPressedVar);
    if(buttonPressedVar != -1){
      currentResponce = quizService.checkResponce(quizService.latestQID, (ButtonIdentifier)buttonPressedVar);
      Serial.println(currentResponce);
    }
    if(currentResponce > 0){
      quizService.applyConsequence(currentResponce);
      Serial.println(Correct);
      currentResponce = -2;
      delay(2000);
    } else if (currentResponce == -1){
      quizService.applyConsequence(currentResponce);
      Serial.println(Wrong);
      currentResponce = -2;
      delay(2000);
    }
  } else {
    quizService.AskRandomQuestion();
    Serial.print(Question_Start);
    Serial.print(quizService.latestQID);
    Serial.println(Question_End);
    Serial.print(Duration_Start);
    Serial.print(quizService.duration(quizService.latestQID));
    Serial.println(Duration_End);
  }
  
  if(quizService.isFinished()){
    Serial.println(End);
    quizService.resetGame();
    buttonService.resetButtons();
  }

  delay(2);
}