#include <LiquidCrystal.h>

const int queueSize = 32;
const double timeCorrection = 1 - 2.0D / (3 * 360); // correct 10 secs per hour

class Queue {
  public:
  unsigned long *buf;
  unsigned int startq = 0, endq = 0, countq = 0, qsize = 0;
  Queue(int qsize){ 
    this -> qsize = qsize ;
    startq = countq = qsize = 0;
    endq = qsize-1;
    buf = (unsigned long *)malloc(qsize*(sizeof(unsigned long))); 
  }
  void push(unsigned long element) {
    endq = (endq + 1) % qsize;
    buf[endq] = element;
    if(countq == qsize)
      startq = (startq+1) % qsize;
    else
      countq++;
  }
  unsigned long sizeq(void) {
    return countq;
  }
  unsigned long getStart() {
    if(countq == 0) return 0L;
    return buf[startq];
  }
  unsigned long getEnd() {
    if(countq == 0) return 0L;
    return buf[endq];
  }
  unsigned long timeSpan(void) {
    if(countq == 0) return 0L;
    return buf[endq] - buf[startq];
  }
};

Queue queue(queueSize);

LiquidCrystal lcd(12, 11, 5, 4, 3, 6);

unsigned long lastSerialUpdate = 0;
int sensorStatus = 0;
int started = 0;
int paused = 0;
unsigned long steps = 0;
unsigned long startedMillis = 0;
unsigned long pausedMillis = 0;
unsigned long pauseStart = 0;
unsigned long pauseEnd = 0;
double weight = 107.0d;

void serialUpdate(String s1, String s2) {
  if(millis() - lastSerialUpdate < 5000) return;
  Serial.print(s1);
  Serial.println(s2);
}

double calories(double weight, int stepIntervals) {
  double g = 9.81d;                // meters per second squared
  double muscleEfficiency = 0.2d;  // muscle's work effected (in calories) vs calories burned ratio
  double stepHeight = 2 * 0.27d;   // device's step height in meters * 2 steps per rotation
  double joulesToCalories = 4.184d * 1000;  // We want kcal, or otherwise known as "Calories"
  return weight * g * stepHeight * stepIntervals / (muscleEfficiency * joulesToCalories);
}

double avgCalories(double weight, int stepIntervals, unsigned long t) {
  double timeElapsed = ((double)t) / 1000 ;
  double timeElapsedInHours = timeElapsed / 3600 ;
  return calories(weight, stepIntervals) / timeElapsedInHours ; 
}

double avgInstCalories(double weight, Queue q) {
  unsigned long timeElapsed = (unsigned long)((q.getEnd() - q.getStart()) * timeCorrection) ;
  return avgCalories(weight, q.sizeq() - 1, timeElapsed);
}

void isr(void) {
  if (paused) {
    pauseEnd = millis();
    pausedMillis += pauseEnd - pauseStart;
  }
  paused = 0;
  if (queue.sizeq() > 0 && millis() - queue.getEnd() < 50) return;
  sensorStatus = ! digitalRead(2);
  if (sensorStatus) {
    Serial.println("Sensor activated.");
  } else {
    Serial.println("Sensor deactivated.");
  }
  if(sensorStatus && ! paused) {
    if(! started) startedMillis = millis();
    started = 1;
    queue.push(millis());
    steps++;
  }
}

void setup() {
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), isr, CHANGE);
  Serial.begin(9600);
  lcd.begin(20, 4);
  Serial.println("Stairclimber calorie counter running...");
}

void printSteps() {
  char s[100];
  lcd.setCursor(0,0);
  sprintf(s, "%4d", 2 * steps);
  lcd.print(s);  
  serialUpdate("Steps: ", s);
}

void printTime() {
  char s[100];
  double  totalTimeElapsed = millis() - (double)startedMillis - pausedMillis;
  totalTimeElapsed *= timeCorrection;
  unsigned int minutes = (unsigned int)(totalTimeElapsed / (60L * 1000));
  unsigned int seconds = (unsigned long)totalTimeElapsed % (60L * 1000) / 1000;
  lcd.setCursor(10,0);
  sprintf(s, "%03u:%02u", minutes, seconds);
  lcd.print(s);
  serialUpdate("Time: ", s);
}

double printTotalCalories() {
  char s[100];
  lcd.setCursor(5,0);
  double totalCalories = calories(weight, steps - 1);
  dtostrf(totalCalories, 4, 0, s);
  lcd.print(s);
  serialUpdate("Total calories: ", s);
  return totalCalories;
}

void printAvgCalories() {
  char s[100];
  unsigned long timeElapsed = queue.getEnd() - startedMillis - pausedMillis;
  timeElapsed = (unsigned long)(timeElapsed * timeCorrection);
  lcd.setCursor(0,1);
  dtostrf(avgCalories(weight, steps - 1, timeElapsed), 4, 0, s);
  serialUpdate("Avg calories per hour: ", s);
  lcd.print(s);
}

double printInstCalories() {
  char s[100];
  lcd.setCursor(6,1);
  double instCalories = avgInstCalories(weight, queue);
  dtostrf(instCalories, 4, 0, s);
  lcd.print(s);
  serialUpdate("Instant calories per hour: ", s);
  return instCalories;
}

void printProjectedCaloriesAt30minMultiple(double totalCalories, double instCalories) {
  char s[100];
  unsigned long timeElapsed = queue.getEnd() - startedMillis - pausedMillis;
  unsigned long halfHour = 30L * 60 * 1000 ;
  timeElapsed *= timeCorrection;
  unsigned long timeRemaining = (timeElapsed / halfHour + 1) * halfHour - timeElapsed;
  double projectedCals = totalCalories + instCalories * timeRemaining / (3600L * 1000) ;
  lcd.setCursor(12, 1);
  dtostrf(projectedCals, 4, 0, s);
  lcd.print(s);
  serialUpdate("Projected calories at next 30min boundary: ", s);
}

void loop() {
  if (millis() - queue.getEnd() > 5000) {
    if (! paused) pauseStart = millis();
    paused = 1 ;
    return ;
  }
  printSteps();
  
  if (started && ! paused) {
    printTime();
  }

  if(queue.sizeq() > 2) {
    double totalCalories = printTotalCalories();  
    printAvgCalories();
    double instCalories = printInstCalories();
    printProjectedCaloriesAt30minMultiple(totalCalories, instCalories);
  }
}
