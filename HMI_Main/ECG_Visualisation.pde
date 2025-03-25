import processing.serial.*;
import controlP5.*;

Serial port;
int[] values;  // Store sensor values
int maxValues = 10000;  // Number of points displayed
float yScale;


ControlP5 p5;

// Buttons
Button zoom_plus;
Button zoom_min;

void setup() {
  size(2000, 1000);  

  p5 = new ControlP5(this);
  // String serial = Serial.list()[0];  // Change if needed
  // port = new Serial(this, serial, 115200);
  values = new int[maxValues];  // Initialize array
  yScale = height / 4096.0;  // Scale for 10-bit ADC

  zoom_plus = p5.addButton("Zoom +").setPosition(250, 100).setSize(100, 50);
  zoom_min = p5.addButton("Zoom -").setPosition(100, 100).setSize(100, 50);
}


void draw() {
  background(0);
  stroke(255);
  noFill();

  // Draw graph
  beginShape();
  for (int i = 0; i < values.length; i++) {
    float x = map(i, 0, values.length, 0, width);
    float y = height - values[i] * yScale;
    vertex(x, y);
  }
  endShape();
}

// Read Serial Data
void serialEvent(Serial port) {
  String line = port.readStringUntil('\n');
  if (line != null) {
    line = trim(line);
    if (line.length() > 0) {
      int val = int(line);
      values = append(values, val);
      if (values.length > maxValues) {
        values = subset(values, 1);  // Keep array size constant
      }
    }
  }
}
