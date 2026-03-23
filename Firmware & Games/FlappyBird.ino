

 // *  FLAPPY BIRD — Game Guy (RP2350)
 // *  Display : SSD1306 128×64 OLED  
 //*  SDA     → D4
 //*  SCL     → D5
 //*  S1      → D0   (flap)
 //*  S2      → D1
 //*  S3      → D2
 //*  S4      → D6
 //*  A       → D7   (flap / confirm)
 //*  B       → D8   (flap / back)
 //*  Buzzer  → D3
 


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display 
#define SCREEN_W   128
#define SCREEN_H    64
#define OLED_ADDR  0x3C
#define OLED_RESET  -1

// ─Pins 
#define PIN_SDA   D4
#define PIN_SCL   D5
#define PIN_S1    D0    // flap
#define PIN_S2    D1
#define PIN_S3    D2
#define PIN_S4    D6
#define PIN_A     D7    // flap / confirm
#define PIN_B     D8    // flap
#define PIN_BUZ   D3

// Game tuning 
#define BIRD_X          22
#define BIRD_R           4      // half-size of bird sprite
#define GRAVITY        0.38f
#define FLAP_VEL      -3.8f
#define PIPE_W          10
#define PIPE_GAP        18      // vertical gap between pipes
#define PIPE_SPEED       2
#define NUM_PIPES        3
#define PIPE_SPACING    58      // pixels between successive pipe spawn X

// Tone frequencies 
#define SND_FLAP   900
#define SND_SCORE  1400
#define SND_HIT    180

//  Ground 
#define GROUND_Y  (SCREEN_H - 6)

// Objects 
struct Pipe {
  int  x;
  int  gapTop;   // y of top edge of the gap
  bool passed;
};

// Globals 
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

enum State { ST_TITLE, ST_PLAYING, ST_DEAD };
State    state       = ST_TITLE;

float    birdY       = 0;
float    birdVY      = 0;
int      score       = 0;
int      hiScore     = 0;
Pipe     pipes[NUM_PIPES];

// Debounce helpers
bool     prevFlap    = false;

// Helpers 
bool flapPressed() {
  return (digitalRead(PIN_S1) == LOW ||
          digitalRead(PIN_A)  == LOW ||
          digitalRead(PIN_B)  == LOW);
}

void beep(int freq, int dur) {
  tone(PIN_BUZ, freq, dur);
}

// Returns a random gap top, keeping the gap fully on screen
int randomGapTop() {
  return random(6, GROUND_Y - PIPE_GAP - 4);
}

// Spawn pipe #i as the i-th one after the rightmost existing pipe
void spawnPipe(int i) {
  // Find rightmost X among other pipes
  int maxX = SCREEN_W;
  for (int j = 0; j < NUM_PIPES; j++) {
    if (j != i && pipes[j].x > maxX) maxX = pipes[j].x;
  }
  pipes[i].x      = maxX + PIPE_SPACING;
  pipes[i].gapTop = randomGapTop();
  pipes[i].passed = false;
}

void initPipes() {
  for (int i = 0; i < NUM_PIPES; i++) {
    pipes[i].x      = SCREEN_W + i * PIPE_SPACING;
    pipes[i].gapTop = randomGapTop();
    pipes[i].passed = false;
  }
}

void initGame() {
  birdY  = SCREEN_H / 2.0f;
  birdVY = 0;
  score  = 0;
  initPipes();
}

//  Drawing
// 8×8 bird sprite (1 = white pixel, relative to centre)
void drawBird(int cx, int cy) {
  // Body
  oled.fillRect(cx - 4, cy - 3, 8, 6, WHITE);
  // Eye
  oled.drawPixel(cx + 1, cy - 1, BLACK);
  // Beak
  oled.drawPixel(cx + 4, cy,     WHITE);
  oled.drawPixel(cx + 4, cy + 1, WHITE);
  // Wing hint (darker slot)
  oled.drawLine(cx - 3, cy + 1, cx - 1, cy + 1, BLACK);
}

void drawPipe(const Pipe& p) {
  if (p.x + PIPE_W < 0 || p.x > SCREEN_W) return;
  // Top pipe
  oled.fillRect(p.x, 0, PIPE_W, p.gapTop, WHITE);
  // Top cap
  oled.fillRect(p.x - 1, p.gapTop - 4, PIPE_W + 2, 4, WHITE);
  // Bottom pipe
  int botY = p.gapTop + PIPE_GAP;
  // Bottom cap
  oled.fillRect(p.x - 1, botY, PIPE_W + 2, 4, WHITE);
  oled.fillRect(p.x, botY + 4, PIPE_W, GROUND_Y - (botY + 4), WHITE);
}

void drawGround() {
  oled.drawFastHLine(0, GROUND_Y,     SCREEN_W, WHITE);
  oled.drawFastHLine(0, GROUND_Y + 1, SCREEN_W, WHITE);
  // Simple grass ticks
  for (int x = 0; x < SCREEN_W; x += 8) {
    oled.drawPixel(x + 2, GROUND_Y + 2, WHITE);
    oled.drawPixel(x + 5, GROUND_Y + 3, WHITE);
  }
}

void drawHUD() {
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(2, 2);
  oled.print(score);
}

// Collision 
bool collidesWithPipe(const Pipe& p) {
  int bx1 = BIRD_X - BIRD_R + 1;
  int bx2 = BIRD_X + BIRD_R - 1;
  int by1 = (int)birdY - BIRD_R + 1;
  int by2 = (int)birdY + BIRD_R - 1;

  int px1 = p.x - 1;
  int px2 = p.x + PIPE_W + 1;

  if (bx2 < px1 || bx1 > px2) return false;   // no X overlap
  if (by1 > p.gapTop && by2 < p.gapTop + PIPE_GAP) return false; // in gap
  return true;
}

// Screens 
void drawTitle() {
  // Big title
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.setCursor(4, 4);
  oled.print("FLAPPY");
  oled.setCursor(22, 22);
  oled.print("BIRD");

  // Decorative bird
  drawBird(106, 18);

  // Blinking "press" hint (blink every ~500 ms based on millis)
  if ((millis() / 500) % 2 == 0) {
    oled.setTextSize(1);
    oled.setCursor(8, 46);
    oled.print("Press A / B / S1");
  }

  // Hi-score
  oled.setTextSize(1);
  oled.setCursor(8, 57);
  oled.print("BEST: ");
  oled.print(hiScore);
}

void drawDead() {
  // Dim background effect via text
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.setCursor(14, 4);
  oled.print("GAME");
  oled.setCursor(14, 22);
  oled.print("OVER!");

  oled.setTextSize(1);
  oled.setCursor(6, 44);
  oled.print("Score : ");
  oled.print(score);
  oled.setCursor(6, 54);
  oled.print("Best  : ");
  oled.print(hiScore);
}

// Arduino setup 
void setup() {
  // I2C 
  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();

  // OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // If OLED fails, blink the LED
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW);  delay(200);
    }
  }
  oled.clearDisplay();
  oled.display();

  // Buttons — internal pull-up, active LOW
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);
  pinMode(PIN_S3, INPUT_PULLUP);
  pinMode(PIN_S4, INPUT_PULLUP);
  pinMode(PIN_A,  INPUT_PULLUP);
  pinMode(PIN_B,  INPUT_PULLUP);

  // Buzzer
  pinMode(PIN_BUZ, OUTPUT);
  noTone(PIN_BUZ);

  // Seed RNG from floating ADC
  randomSeed(analogRead(A0) ^ analogRead(A1) ^ micros());

  // Startup jingle
  beep(600, 60); delay(80);
  beep(900, 60); delay(80);
  beep(1200, 120);

  state = ST_TITLE;
}

// ── Arduino loop ───────────────────────────────────────────
void loop() {
  static uint32_t lastFrame = 0;
  uint32_t now = millis();

  // Cap at ~40 fps (25 ms per frame)
  if (now - lastFrame < 25) return;
  lastFrame = now;

  bool curFlap = flapPressed();
  bool flapEdge = curFlap && !prevFlap;  // rising edge only

  oled.clearDisplay();

  // State machine 
  switch (state) {

    // TITLE 
    case ST_TITLE:
      drawTitle();
      if (flapEdge) {
        initGame();
        state = ST_PLAYING;
        beep(800, 50);
      }
      break;

    // PLAYING 
    case ST_PLAYING: {
      // Input
      if (flapEdge) {
        birdVY = FLAP_VEL;
        beep(SND_FLAP, 40);
      }

      // Physics
      birdVY += GRAVITY;
      birdY  += birdVY;

      // Ceiling
      if (birdY - BIRD_R < 0) {
        birdY  = BIRD_R;
        birdVY = 0;
      }

      // Hit ground
      if ((int)birdY + BIRD_R >= GROUND_Y) {
        birdY = GROUND_Y - BIRD_R;
        beep(SND_HIT, 400);
        if (score > hiScore) hiScore = score;
        state = ST_DEAD;
        break;
      }

      // Update & draw pipes
      for (int i = 0; i < NUM_PIPES; i++) {
        pipes[i].x -= PIPE_SPEED;

        // Recycle off-screen pipe
        if (pipes[i].x + PIPE_W < 0) {
          spawnPipe(i);
        }

        // Score point
        if (!pipes[i].passed && (pipes[i].x + PIPE_W) < BIRD_X) {
          pipes[i].passed = true;
          score++;
          beep(SND_SCORE, 60);
        }

        // Collision
        if (collidesWithPipe(pipes[i])) {
          beep(SND_HIT, 400);
          if (score > hiScore) hiScore = score;
          state = ST_DEAD;
          break;
        }

        drawPipe(pipes[i]);
      }

      if (state != ST_PLAYING) break;  // exit early on death

      drawGround();
      drawBird(BIRD_X, (int)birdY);
      drawHUD();
      break;
    }

    // GAME OVER 
    case ST_DEAD:
      drawDead();
      // Wait for button press to return to title
      if (flapEdge) {
        state = ST_TITLE;
        beep(600, 60); delay(80);
        beep(900, 60);
      }
      break;
  }

  oled.display();
  prevFlap = curFlap;
}
