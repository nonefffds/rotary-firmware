#include <Wire.h>

// Wemos D1 Mini wiring
//   D1 (GPIO5) -> BMI160 SCL
//   D2 (GPIO4) -> BMI160 SDA
#define SDA_PIN D2
#define SCL_PIN D1

#define BMI160_ADDR_0 0x68
#define BMI160_ADDR_1 0x69
#define BMI160_CHIP_ID 0xD1

#define REG_CHIP_ID 0x00
#define REG_ACC_X_L 0x12
#define REG_ACC_RANGE 0x41
#define REG_CMD     0x7E
#define CMD_ACC_NORMAL 0x11

#define G_PER_LSB 16384.0f
#define SECTOR_HALF 30.0f
#define HEARTBEAT_MS 2000UL

static int8_t dev_addr = 0;

int read_reg(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(dev_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  Wire.requestFrom((uint8_t)dev_addr, len);
  uint8_t i = 0;
  while (Wire.available() && i < len) buf[i++] = Wire.read();
  return i;
}

int write_reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(dev_addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission();
}

bool init_bmi160() {
  for (int8_t a : {BMI160_ADDR_0, BMI160_ADDR_1}) {
    dev_addr = a;
    Wire.beginTransmission(dev_addr);
    Wire.write(REG_CHIP_ID);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)dev_addr, (uint8_t)1);
    if (Wire.available() && Wire.read() == BMI160_CHIP_ID) {
      write_reg(REG_CMD, CMD_ACC_NORMAL);  // accel to normal mode
      delay(100);
      write_reg(REG_ACC_RANGE, 0x00);      // accel range = +-2g (16384 LSB/g)
      delay(20);
      return true;
    }
    dev_addr = 0;
  }
  return false;
}

bool read_accel_g(float &ax, float &ay, float &az) {
  uint8_t b[6];
  if (read_reg(REG_ACC_X_L, b, 6) != 6) return false;
  int16_t x = (int16_t)((b[1] << 8) | b[0]);
  int16_t y = (int16_t)((b[3] << 8) | b[2]);
  int16_t z = (int16_t)((b[5] << 8) | b[4]);
  ax = x / G_PER_LSB;
  ay = y / G_PER_LSB;
  az = z / G_PER_LSB;
  return true;
}

// Angle of the monitor's "up" direction in the sensor XY plane.
//   upright  (landscape)        ->   0 deg   -> state 0  (dmdo 0)
//   +90 deg  (front-clockwise)  -> +90 deg   -> state 1  (dmdo 1)
//   +-180 deg (upside down)     -> +-180 deg -> state 2  (dmdo 2)
//   -90 deg  (front-ccw)        -> -90 deg   -> state 3  (dmdo 3)
int angle_to_state(float deg) {
  if (deg >= -SECTOR_HALF && deg <= SECTOR_HALF) return 0;
  if (deg >= 90.0f - SECTOR_HALF && deg <= 90.0f + SECTOR_HALF) return 1;
  if (deg >= 180.0f - SECTOR_HALF || deg <= -180.0f + SECTOR_HALF) return 2;
  if (deg >= -90.0f - SECTOR_HALF && deg <= -90.0f + SECTOR_HALF) return 3;
  return -1;  // inside the dead band -> keep previous state
}

int state = -1;
unsigned long last_beat = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(50);
  Serial.println("READY");

  if (!init_bmi160()) {
    Serial.println("ERR BMI160 not found (check wiring: D1->SCL, D2->SDA, VCC/GND)");
    while (1) delay(1000);
  }
  Serial.print("OK BMI160 addr=0x");
  Serial.println(dev_addr, HEX);
}

void loop() {
  float ax, ay, az;
  if (!read_accel_g(ax, ay, az)) {
    delay(50);
    return;
  }

  // average a few samples (angle averaging via sin/cos avoids the +/-180 wrap)
  static float sin_acc = 0.0f, cos_acc = 0.0f;
  static int n = 0;
  float a = atan2(ax, ay);
  sin_acc += sin(a);
  cos_acc += cos(a);
  n++;
  if (n < 8) {
    delay(20);
    return;
  }
  float angle = degrees(atan2(sin_acc, cos_acc));
  sin_acc = 0.0f; cos_acc = 0.0f; n = 0;

  // live angle report for the sensor test view:  A=<deg> <ax> <ay> <az>
  Serial.print("A=");
  Serial.print(angle, 1);
  Serial.print(" ");
  Serial.print(ax, 3);
  Serial.print(" ");
  Serial.print(ay, 3);
  Serial.print(" ");
  Serial.println(az, 3);

  int new_state = angle_to_state(angle);
  if (new_state >= 0 && new_state != state) {
    state = new_state;
    Serial.print("ROT=");
    Serial.println(state);
    digitalWrite(LED_BUILTIN, LOW);   // LED on (active low) -> blink on change
    delay(60);
    digitalWrite(LED_BUILTIN, HIGH);
  }

  unsigned long now = millis();
  if (state >= 0 && now - last_beat > HEARTBEAT_MS) {
    last_beat = now;
    Serial.print("ROT=");
    Serial.println(state);
  }
  delay(20);
}
