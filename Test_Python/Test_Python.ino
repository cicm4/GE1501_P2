/*
 * Quiz Application Test Script
 * 
 * This Arduino sketch simulates the quiz controller by sending
 * serial messages to test the PC client application.
 * 
 * Commands:
 * 1/ - Start video
 * 2<n>/ - Show question n
 * 3/ - Show correct answer
 * 4/ - Show wrong answer
 * 5<time>/ - Set question time in ms
 * 6 - End quiz
 */

void setup() {
  // Initialize serial communication at 9600 baud
  Serial.begin(9600);
  delay(2000); // Wait for serial connection to establish
  
  Serial.println("Arduino Quiz Simulator Starting...");
  delay(1000);
}

void loop() {
  // Start the quiz with intro video
  Serial.println("Starting quiz simulation...");
  Serial.println("1/");
  delay(7000); // Wait 5 seconds to simulate video playing
  
  // Example: Three questions with varying responses
  
  // Question 1 - Correct answer
  Serial.println("Starting Question 1...");
  Serial.println("5<10000>/"); // 10 second timer
  delay(10);
  Serial.println("2<0>/"); // Question 0 (displays as Q1.png)
  delay(10000); // Wait for 8 seconds (simulating thinking time)
  Serial.println("3/"); // Correct answer
  delay(2000);
  
  // Question 2 - Wrong answer
  Serial.println("Starting Question 2...");
  Serial.println("5<15000>/"); // 15 second timer
  delay(10);
  Serial.println("2<1>/"); // Question 1 (displays as Q2.png)
  delay(15000); // Wait for 10 seconds (simulating thinking time)
  Serial.println("4/"); // Wrong answer
  delay(3000);
  
  // Question 3 - Correct answer
  Serial.println("Starting Question 3...");
  Serial.println("5<20000>/"); // 20 second timer
  delay(1000);
  Serial.println("2<2>/"); // Question 2 (displays as Q3.png)
  delay(12000); // Wait for 12 seconds (simulating thinking time)
  Serial.println("3/"); // Correct answer
  delay(2000);
  
  // End quiz
  Serial.println("Ending quiz...");
  Serial.println("6");
  
  // Wait before restarting the entire sequence
  delay(10000);
  Serial.println("Restarting quiz simulation cycle...");
}