import processing.serial.*;
import controlP5.*;

Serial port;
int[] heartbeat_values;  // Store heartbeat values
int[] pressure_values;  // Store pressure values
int maxValues = 10000;  // Number of points displayed (horizontal resolution)
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
  heartbeat_values = new int[maxValues];  // Initialize array for heartbeat sensor values
  pressure_values = new int[maxValues];  // Initialize array for pressure sensor values
  yScale = height / 4096.0;  // Scale for 12-bit ADC

  zoom_plus = p5.addButton("zoom_plus").setPosition(250, 100).setSize(100, 50).setLabel("Zoom +");
  zoom_min = p5.addButton("zoom_min").setPosition(100, 100).setSize(100, 50).setLabel("Zoom -");
}


void draw() {
  background(0);
  stroke(255);
  noFill();

  // Draw graph
  beginShape();
  for (int i = 0; i < heartbeat_values.length; i++) {
    float x = map(i, 0, heartbeat_values.length, 0, width);
    float y = height - heartbeat_values[i] * yScale;
    vertex(x, y);
  }
  endShape();

}

// Read Serial Data
void serialEvent(Serial port) {
  String line = port.readStringUntil('\n');
  if (line != null) {
    line = trim(line); // Trims whitespaces from beginning and end of string

    if (line.length() > 0) { // Check if the line is not empty after the trim

      switch (line[0]) {
        case 'h': // Heartbeat
          
          int val = int(line);
          heartbeat_values = append(heartbeat_values, val);
          if (values.length > maxValues) {
            heartbeat_values = subset(heartbeat_values, 1);  // Remove first value and shift all values to the left
          }

          break;

        case 'p': // Pressure
          break;

        case 't': // Temperature
          
          break;
        
        case 'm': // Moisture
          
          break;

        default:  // unknown / Garbage
          
          break;
      }
    }
  }
}

public void zoom_plus(int theValue) {
  println("ASDJAKSD");
}