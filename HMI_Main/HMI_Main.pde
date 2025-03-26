import processing.serial.*;
import controlP5.*;

Serial port;
int[] heartbeat_values;  // Store heartbeat values
int[] pressure_values;  // Store pressure values
int maxValues = 5000;  // Number of points displayed (horizontal resolution)
float yScale;
float ECG_scale = 0;

ControlP5 p5;


// Buttons and sliders
Slider ECG_slider;

PFont valueFont;
PFont unitFont;

void setup() 
{

  p5 = new ControlP5(this);

  String serial = Serial.list()[0];  // Change if needed
  port = new Serial(this, serial, 115200);
  heartbeat_values = new int[maxValues];  // Initialize array for heartbeat sensor values
  pressure_values = new int[maxValues];  // Initialize array for pressure sensor values
  yScale = height / 4096.0;  // Scale for 12-bit ADC


  
  // ECG_slider
  ECG_slider = p5.addSlider("ECG_slider")
                  .setPosition(width * 0.02, height * 0.34)
                  .setSize(int(width * 0.25), int(height * 0.03))
                  .setNumberOfTickMarks(10)
                  .snapToTickMarks(true)
                  .setRange(0, 0.15)
                  .setLabel("ECG Scale");
                  
  
}


void draw() 
{
  // Ceate fonts
  valueFont = createFont("Arial", width * 0.08);
  unitFont = createFont("Arial", width * 0.02);


  background(0);
  stroke(255);
  noFill();


  // Slider & button positions (for scaling purposes)
  ECG_slider.setPosition(width * 0.02, height * 0.34)
            .setSize(int(width * 0.25), int(height * 0.03));


  // Draw graph outlines
  stroke(255, 0, 0); // Red
  rect( width * 0.02, 
        height * 0.03, 
        width * 0.75, 
        height * 0.3
        );  

  stroke(153, 255, 255); // Light Blue
  rect( width * 0.02, 
        (height * 0.3 + height * 0.03) + height * 0.05, 
        width * 0.75, 
        height * 0.3
        );

  // End of graph outlines


  // Text
  textFont(valueFont);
  fill(255, 0, 0);
  String bpm = "65";
  text(bpm, width * 0.78, height * 0.25);

  textFont(unitFont);
  text("BPM", width * 0.78 + width * 0.15, height * 0.25);
  
  // Draw ECG graph
  beginShape();
  stroke(255, 0, 0); // Red
  for (int i = 0; i < heartbeat_values.length; i++) 
  {
    float x = map(i, 
                  0, 
                  heartbeat_values.length, 
                  width * 0.03, 
                  width * 0.02 + width * 0.74
                  );

    float y = map(heartbeat_values[i],  // ECG is upside down, flip it 180 degrees
                  2800, 
                  500, 
                  height * 0.03 + height * -ECG_scale, 
                  height * 0.27 + height * ECG_scale
                  );


    // Limit the y values to the graph area
    y = min(height * 0.3 + height * 0.03, y);
    y = max(height * 0.03, y);
    vertex(x, y);
  }
  endShape();


}

// Read Serial Data
void serialEvent(Serial port)
{
  String line = port.readStringUntil('\n');
  if (line != null) 
  {
    line = trim(line); // Trims whitespaces from beginning and end of string

    if (line.length() > 0) // Check if the line is not empty after the trim
    { 

      switch (line.charAt(0)) 
      {
        case 'h': // Heartbeat

          line = line.substring(1); // Remove the first character from the string  
          int val = int(line); // Extract the value from the string (remove first character)
          heartbeat_values = append(heartbeat_values, val);
          if (heartbeat_values.length > maxValues) 
          {
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
          println("UNKNOWN VALUE");
          break;
      }
    }
  }
}

public void ECG_slider(float theValue) {
  ECG_scale = theValue;
}

public void settings() {
 size(1920, 700, P2D);

}