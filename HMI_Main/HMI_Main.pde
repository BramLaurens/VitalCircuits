import processing.serial.*;
import controlP5.*;

Serial port;
int[] heartbeat_values;  // Store heartbeat values
int[] pressure_values;  // Store pressure values
int graph_max_length = 5000;  // Number of points displayed (horizontal resolution)
float yScale;
float ECG_scale = 0;
float pressure_scale = 0;

ControlP5 p5;


// Buttons and sliders
Slider ecg_slider;
Slider pressure_slider;

Button create_screenshot;
Button home_screen;
Button ecg_screen;
Button respiration_screen;

// Initialize fonts
PFont value_font;
PFont unit_font;
PFont label_font;
PFont title_font;

// Initialize font sizes
int value_font_size = 110;
int important_value_font_size = 110;
int unit_font_size = 110;
int label_font_size = 110;
int title_font_size = 110;

// Other variables
char current_screen = 0;

void setup() 
{

  p5 = new ControlP5(this);
  windowTitle("Vital Circuits - Stress monitor");

  // println(PFont.list()); // Prints all fonts.

  String serial = Serial.list()[0];  // Change if needed
  port = new Serial(this, serial, 115200);
  heartbeat_values = new int[graph_max_length];  // Initialize array for heartbeat sensor values
  pressure_values = new int[graph_max_length];  // Initialize array for pressure sensor values
  yScale = height / 4096.0;  // Scale for 12-bit ADC

  // label_font_size because this is the only font that can't dynamically change
  label_font_size = int(width * 0.01);

  // Fonts
  value_font = createFont("Arial", value_font_size, true);
  unit_font = createFont("Arial", unit_font_size, true);
  label_font = createFont("Arial", label_font_size, true);
  title_font = createFont("Arial Bold", title_font_size, true);

  // --- Sliders --- \\
  // ecg_slider
  ecg_slider = p5.addSlider("ecg_slider")
                  .setPosition(width * 0.02, height * 0.34)
                  .setSize(int(width * 0.25), int(height * 0.03))
                  .setNumberOfTickMarks(20)
                  .snapToTickMarks(true)
                  .setRange(0, 0.15)
                  .setLabel("ECG Scale")
                  .setFont(label_font);
                  
                  
  // pressure_slider
  pressure_slider = p5.addSlider("pressure_slider")
                      .setPosition(width * 0.02, (height * 0.3 + height * 0.03) + height * 0.36)
                      .setSize(int(width * 0.25), int(height * 0.03))
                      .setNumberOfTickMarks(20)
                      .snapToTickMarks(true)
                      .setRange(0, 0.35)
                      .setLabel("Pressure Scale")
                      .setFont(label_font);

  // --- Buttons --- \\
  // create_screenshot
  create_screenshot = p5.addButton("create_screenshot")
                  .setPosition(width * 0.92, 0)
                  .setSize(int(width * 0.08), int(height * 0.06))
                  .setLabel("Save Screen")
                  .setFont(label_font);

  // Navigation buttons
  // home_screen
  home_screen = p5.addButton("home_screen")
                  .setPosition(width * 0.8, height * 0.94)
                  .setSize(int(width * 0.05), int(height * 0.06))
                  .setLabel("Home")
                  .setFont(label_font);
  
  // ecg_screen
  ecg_screen = p5.addButton("ecg_screen")
                  .setPosition(width * 0.85, height * 0.94)
                  .setSize(int(width * 0.05), int(height * 0.06))
                  .setLabel("ECG")
                  .setFont(label_font);

  respiration_screen = p5.addButton("respiration_screen")
                  .setPosition(width * 0.9, height * 0.94)
                  .setSize(int(width * 0.1), int(height * 0.06))
                  .setLabel("Respiration")
                  .setFont(label_font);
}


void draw() 
{
  // Update values
  String bpm_value = "62";
  String respiration_rate_value = "25";
  String temperature_value = "36.5";
  String moisture_value = "14";

  // Update font size
  value_font_size = int(width * 0.07);
  important_value_font_size = int(width * 0.1);
  unit_font_size = int(width * 0.02);
  title_font_size = int(width * 0.01);

  background(0);
  stroke(255);


  // --- Title --- \\
  textFont(title_font);
  textSize(title_font_size);
  fill(255, 255, 255); // White
  text("Vital Circuits - Stress monitor", width * 0.021, height * 0.021);

  // Buttons
  create_screenshot.setPosition(width * 0.92, 0)
                   .setSize(int(width * 0.08), int(height * 0.06));

  home_screen.setPosition(width * 0.8, height * 0.94)
             .setSize(int(width * 0.05), int(height * 0.06));
  
  ecg_screen.setPosition(width * 0.85, height * 0.94)
            .setSize(int(width * 0.05), int(height * 0.06));

  respiration_screen.setPosition(width * 0.9, height * 0.94)
                    .setSize(int(width * 0.1), int(height * 0.06));


  if (current_screen == 0) 
  {
    noFill();

    // Show / hide control P5 elements
    ecg_slider.show();
    pressure_slider.show();
    create_screenshot.show();


    // Slider & button positions (for scaling purposes)
    ecg_slider.setPosition(width * 0.02, height * 0.34)
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
    // --- BPM --- \\
    fill(255, 0, 0); // Red

    // BPM value 
    textFont(value_font);
    textSize(important_value_font_size);
    text(bpm_value, width * 0.78, height * 0.25);

    // BPM unit
    textFont(unit_font);
    textSize(unit_font_size);
    text("BPM", width * 0.78, height * 0.3);
    noFill();

    // --- Pressure --- \\ 
    fill(153, 255, 255); // Light Blue

    // Pressure value
    textFont(value_font);
    textSize(value_font_size);
    text(respiration_rate_value, width * 0.78, height * 0.6);

    // Pressure unit
    textFont(unit_font);
    textSize(unit_font_size);
    text("Breaths p/m", width * 0.78, height * 0.65);
    noFill();


    // --- Temperature --- \\
    fill(255, 255, 0); // Yellow

    // Temperature Label
    textFont(unit_font);
    textSize(unit_font_size);
    text("Temperature", width * 0.02, height * 0.97);

    // Temperature value
    textFont(value_font);
    textSize(value_font_size);
    
    text(temperature_value, width * 0.02, height * 0.92);

    // Temperature unit
    textFont(unit_font);
    textSize(unit_font_size);
    text("°C", width * 0.02 + width * 0.14, height * 0.92);


    // --- Moisture --- \\
    fill(0, 255, 0); // Green

    // Moisture Label
    textFont(unit_font);
    textSize(unit_font_size);
    text("Skin moisture", width * 0.4, height * 0.97);	

    // Moisture value
    textFont(value_font);
    textSize(value_font_size);
    text(moisture_value, width * 0.4, height * 0.92);

    // Moisture unit
    textFont(unit_font);
    textSize(unit_font_size);
    text("%", width * 0.4 + width * 0.12, height * 0.92);


    // Draw ECG graph
    noFill();
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
                    40000, 
                    6000, 
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
    noFill();
    beginShape();
    stroke(153, 255, 255); // Light Blue
    for (int i = 0; i < pressure_values.length; i++) 
    {
      float x = map(i, 
                    0, 
                    pressure_values.length, 
                    width * 0.03, 
                    width * 0.02 + width * 0.74
                    );

      float y = map(pressure_values[i],
                    4096, 
                    0, 
                    (height * 0.38) - height * pressure_scale, 
                    (height * 0.68)
                    );

     y = min(height * 0.68, y);
     y = max(height * 0.38, y);
    vertex(x, y);

    /*
    rect( width * 0.02, 
          (height * 0.3 + height * 0.03) + height * 0.05, 
          width * 0.75, 
          height * 0.3
          );
    */

    }
    endShape();

  }

  else if (current_screen == 1)
  {
    noFill();
    // Show / hide control P5 elements
    pressure_slider.hide();
    ecg_slider.show();

    ecg_slider.setPosition(width * 0.02, height * 0.92)
              .setSize(int(width * 0.25), int(height * 0.03));


    // Text
    // --- BPM --- \\
    fill(255, 0, 0); // Red

    // BPM value 
    textFont(value_font);
    textSize(important_value_font_size);
   
    text(bpm_value, width * 0.81, height * 0.25);

    // BPM unit
    textFont(unit_font);
    textSize(unit_font_size);
    text("BPM", width * 0.81, height * 0.3);
    noFill();


    // Draw graph outline
    stroke(255, 0, 0); // Red
    rect( width * 0.02, 
          height * 0.03, 
          width * 0.78, 
          height * 0.87
          );  

     // Draw ECG graph
    noFill();
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
      y = min(height * 0.87 + height * 0.03, y);
      y = max(height * 0.03, y);
      vertex(x, y);
    }
    endShape();


    // Text
  
  } 

  else if (current_screen == 2)
  {
    noFill();
    // Show / hide control P5 elements   
    ecg_slider.hide();
    pressure_slider.show();

    pressure_slider.setPosition(width * 0.02, height * 0.92)
                   .setSize(int(width * 0.25), int(height * 0.03));


    // Text
    // --- BPM --- \\
    fill(153, 255, 255); // Light Blue

    // BPM value 
    textFont(value_font);
    textSize(important_value_font_size);
    text(respiration_rate_value, width * 0.81, height * 0.25);

    // BPM unit
    textFont(unit_font);
    textSize(unit_font_size);
    text("Breaths p/m", width * 0.81, height * 0.3);
    noFill();


    // Draw graph outline
    stroke(153, 255, 255); // Light Blue
    rect( width * 0.02, 
          height * 0.03, 
          width * 0.78, 
          height * 0.87
          );  

    // Draw respiration graph
    noFill();
    beginShape();
    stroke(153, 255, 255); // Light Blue
    for (int i = 0; i < pressure_values.length; i++) 
    {
      float x = map(i, 
                    0, 
                    pressure_values.length, 
                    width * 0.03, 
                    width * 0.02 + width * 0.74
                    );

      float y = map(pressure_values[i],  // ECG is upside down, flip it 180 degrees
                    0, 
                    4096, 
                    (height * 0.03) + height * -pressure_scale, 
                    (height * 0.27) + height * pressure_scale
                    );


      // Limit the y values to the graph area
      y = min(height * 0.87 + height * 0.03, y);
      y = max(height * 0.03, y);
      vertex(x, y);


    }
  }
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
          if (heartbeat_values.length > graph_max_length) 
          {
            heartbeat_values = subset(heartbeat_values, 1);  // Remove first value and shift all values to the left
          }

          break;

        case 'p': // Pressure
        
          line = line.substring(1); // Remove the first character from the string  
          int val_p = int(line); // Extract the value from the string (remove first character)
          pressure_values = append(pressure_values, val_p);
          if (pressure_values.length > graph_max_length) 
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

public void ecg_slider(float theValue) 
{
  ECG_scale = theValue;
}


public void pressure_slider(float theValue) 
{
  pressure_scale = theValue;
}

public void create_screenshot() 
{
  String filename = String.format("%d_%d_%d_t%s-%s-%s_VC_Stress monitor.png", year(), month(), day(), nf(hour(), 2), nf(minute(), 2), nf(second(), 2));

  save(filename);
}


// Screens \\ 
public void home_screen()
{
  current_screen = 0;
}

public void ecg_screen()
{
  current_screen = 1;
}

public void respiration_screen()
{
  current_screen = 2;
}

public void settings() 
{
  size(1920, 700, P2D);
}
