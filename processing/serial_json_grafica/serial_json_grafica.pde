import processing.serial.*;
import grafica.*;

Serial port;
JSONObject json;
String val;

GPlot plot;
int maxPoints = 100;
int curPoint = 0;
GPointsArray points = new GPointsArray(maxPoints);

void setup()
{
  size(500, 350);
  background(150);

  println("Available serial ports:");
  println(Serial.list());

  port = new Serial(this, Serial.list()[0], 9600);
  //port = new Serial(this, "COM1", 9600);
  //port.bufferUntil('\n');

  // plot
  plot = new GPlot(this, 25, 25);
}

void draw()
{
  while (port.available() > 0) {
    String data = port.readStringUntil('\n');

    if (data != null) {
      try {
        json = JSONObject.parse(data);
        println(json);

        String deviceName = json.getString("device");
        float deviceVersion = json.getFloat("version");

        // bmp
        JSONObject bmp = json.getJSONObject("bmp");
        float bmpPressure = bmp.getFloat("pressure");
        float bmpTemperature = bmp.getFloat("temperature");
        float bmpAltitude = bmp.getFloat("altitude");

        // dht
        JSONObject dht = json.getJSONObject("dht");
        float dhtTemperature = dht.getFloat("temperature");
        int dhtHumidity = dht.getInt("humidity");

        // outside
        JSONObject outside = json.getJSONObject("outside");
        float outsideTemp = outside.getFloat("temperature");

        // plot
        plot.setTitleText(deviceName + " v" + str(deviceVersion) + " - outside temperature");
        plot.getXAxis().setAxisLabelText("x - time");
        plot.getYAxis().setAxisLabelText("y - temp in 'C");
        addPoint(outsideTemp);
        plot.setPoints(points);
        plot.defaultDraw();
      } 
      catch (Exception e) {
        e.printStackTrace();
      }
    }
  }
}

void addPoint(float val)
{
  curPoint++;
  points.add(curPoint, val);
}
