import processing.serial.*;
import controlP5.*;

Serial port;
int[] heartbeat_values;  // Store heartbeat values
int[] pressure_values;  // Store pressure values
int maxValues = 5000;  // Number of points displayed (horizontal resolution)
float yScale;
float ECG_scale = 0;
float pressure_scale = 0;

ControlP5 p5;


// Buttons and sliders
Slider ECG_slider;
Slider pressure_slider;

PFont valueFont;
PFont unitFont;
PFont labelFont;

void setup() 
{

  p5 = new ControlP5(this);

  // println(PFont.list()); // Prints all fonts.

  String serial = Serial.list()[0];  // Change if needed
  port = new Serial(this, serial, 115200);
  heartbeat_values = new int[maxValues];  // Initialize array for heartbeat sensor values
  pressure_values = new int[maxValues];  // Initialize array for pressure sensor values
  yScale = height / 4096.0;  // Scale for 12-bit ADC

  // Fonts
  valueFont = createFont("Arial", width * 0.07, true);
  unitFont = createFont("Arial", width * 0.02, true);
  labelFont = createFont("Arial", width * 0.01, true);


  // ECG_slider
  ECG_slider = p5.addSlider("ECG_slider")
                  .setPosition(width * 0.02, height * 0.34)
                  .setSize(int(width * 0.25), int(height * 0.03))
                  .setNumberOfTickMarks(10)
                  .snapToTickMarks(true)
                  .setRange(0, 0.15)
                  .setLabel("ECG Scale")
                  .setFont(labelFont);
                  
                  
  // pressure_slider
  pressure_slider = p5.addSlider("pressure_slider")
                      .setPosition(width * 0.02, (height * 0.3 + height * 0.03) + height * 0.36)
                      .setSize(int(width * 0.25), int(height * 0.03))
                      .setNumberOfTickMarks(10)
                      .snapToTickMarks(true)
                      .setRange(0, 0.15)
                      .setLabel("Pressure Scale")
                      .setFont(labelFont);
  
}


void draw() 
{

  background(0);
  stroke(255);
  noFill();


  // Slider & button positions (for scaling purposes)
  ECG_slider.setPosition(width * 0.02, height * 0.34)
            .setSize(int(width * 0.25), int(height * 0.03));


  pressure_slider.setPosition(width * 0.02, height * 0.33 + height * 0.36)
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
  
  // BPM value
  textFont(valueFont);
  textSize(width * 0.07);
  fill(255, 0, 0); // Red
  String bpm = "62";
  text(bpm, width * 0.78, height * 0.25);

  // BPM unit
  textFont(unitFont);
  textSize(width * 0.02);
  text("BPM", width * 0.78, height * 0.3);
  noFill();


  // Pressure 
  textFont(valueFont);
  textSize(width * 0.07);
  fill(153, 255, 255); // Light Blue
  String respiration_rate = "25";
  text(respiration_rate, width * 0.78, height * 0.6);

  // Respiration rate unit
  textFont(unitFont);
  textSize(width * 0.02);
  text("Breaths p/m", width * 0.78, height * 0.65);
  noFill();
  
  

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
                  (height * 0.03) + height * -ECG_scale, 
                  (height * 0.27) + height * ECG_scale
                  );


    // Limit the y values to the graph area
    y = min(height * 0.3 + height * 0.03, y);
    y = max(height * 0.03, y);
    vertex(x, y);
  }
  endShape();


  // Draw Pressure graph
  
  beginShape();
  stroke(153, 255, 255); // Light Blue
  for (int i = 0; i < pressure_values.length; i++) 
  {
    println(pressure_values[i]);
    float x = map(i, 
                  0, 
                  pressure_values.length, 
                  width * 0.03, 
                  width * 0.02 + width * 0.74
                  );

    float y = map(pressure_values[i],
                  0, 
                  4096, 
                  (height * 0.03) + height * -pressure_scale, 
                  (height * 0.27) + height * pressure_scale
                  );


  
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
          int val_h = int(line); // Extract the value from the string (remove first character)
          heartbeat_values = append(heartbeat_values, val_h);
          if (heartbeat_values.length > maxValues) 
          {
            heartbeat_values = subset(heartbeat_values, 1);  // Remove first value and shift all values to the left
          }

          break;

        case 'p': // Pressure
        
          line = line.substring(1); // Remove the first character from the string  
          int val_p = int(line); // Extract the value from the string (remove first character)
          pressure_values = append(pressure_values, val_p);
          if (pressure_values.length > maxValues) 
          {
            pressure_values = subset(pressure_values, 1);  // Remove first value and shift all values to the left
          }

          

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


public void pressure_slider(float theValue) {
  pressure_scale = theValue;
}


public void settings() {
 size(1920, 700, P2D);

}
