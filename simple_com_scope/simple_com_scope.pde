import processing.serial.*;

Serial port;
float[] values;
int graphWidth = 800;
int graphHeight = 400;
float yScale = 1.0; // Change this to scale the Y-axis

void setup() {
  // This must be the first line in setup()
  size(800, 400);
  
  // Now it's safe to print the available serial ports
  println(Serial.list());

  // Adjust the port index to your ESP32
  port = new Serial(this, Serial.list()[2], 115200);
  port.bufferUntil('\n');

  values = new float[graphWidth * 2];
}

void draw() {
  background(0);

  // Draw the graph
  stroke(0, 255, 0);
  noFill();
  beginShape();
  for (int i = 0; i < values.length; i++) {
    float x = map(i, 0, values.length, 0, width);
    //float y = map(values[i], 0, 1023, height - 10, (height / 2) + 10); // Assuming 1`0-bit ADC input
    float y = map(values[i] * yScale, 0, 1023, height - 10, 10);
    vertex(x, y);
  }
  endShape();
}

void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    inString = trim(inString);
    try {
      float val = float(inString);
      shiftArray(values, val);
    } catch (NumberFormatException e) {
      println("Invalid number: " + inString);
    }
  }
}

void shiftArray(float[] arr, float newVal) {
  for (int i = 1; i < arr.length; i++) {
    arr[i - 1] = arr[i];
  }
  arr[arr.length - 1] = newVal;
}


void keyPressed() {
  if (key == '=') {
    yScale += 0.1; // Zoom in
  }
  if (key == '-') {
    yScale -= 0.1; // Zoom out
  }
}
