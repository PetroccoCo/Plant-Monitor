#include <Arduino.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastCRC.h>
#include "wifi.h"
#include <ArduinoOTA.h>

bool printRTCData(ushort offset);
uint8_t getRTCData(ushort offset);
void writeRTCData(ushort offset);
ushort findOldestOffset();

struct MoistureData
{
  uint8_t crc8;
  ulong timeStamp;
  u_char moistureVal;
} rtcData;

WiFiServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

FastCRC8 CRC8;

ushort currentOffset;

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // give everything a second to settle
  delay(1000);
  WiFi.begin(SSID, PSK);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  server.begin();

  timeClient.begin();
  timeClient.update();

  ushort startingOffset = findOldestOffset();
  Serial.printf("Oldest offset is %u \n", startingOffset);

  writeRTCData(startingOffset);
  currentOffset = startingOffset;

  ArduinoOTA.setPort(3232);
  ArduinoOTA.begin();
  ArduinoOTA.setRebootOnSuccess(true);
  //Serial.println("Going into deep sleep for 60 minutes");
  //deep sleep is micro seconds, one second is 1e6
  //ESP.deepSleep(36e8);
}

/* iterate by 2 as our data struct is 8 bytes and offsets are at 4 byte boundries */
ushort findOldestOffset()
{
  ushort oldOffset = 0;
  ulong oldTime = timeClient.getEpochTime();
  for (short curOffset = 0; curOffset < 128; curOffset = curOffset + 2)
  {
    uint8_t crc8 = getRTCData(curOffset);
    if (crc8 == rtcData.crc8)
    {
      if (oldTime > rtcData.timeStamp)
      {
        oldTime = rtcData.timeStamp;
        oldOffset = curOffset;
      }
    }
    else
    {
      // invalid checksum just start here
      return curOffset;
    }
  }
  return oldOffset;
}

bool printRTCData(ushort offset)
{
  Serial.printf("Reading block %u \n", offset);
  if (ESP.rtcUserMemoryRead(offset, (uint32_t *)&rtcData, sizeof(rtcData)))
  {
    Serial.println("Read: ");
    uint8_t crcOfData = CRC8.smbus((uint8_t *)&rtcData.timeStamp, sizeof(rtcData.timeStamp));
    Serial.print("CRC8 of data: ");
    Serial.println(crcOfData, HEX);
    Serial.print("CRC8 read from RTC: ");
    Serial.println(rtcData.crc8, HEX);
    Serial.printf("Moisture was %u at time %lu \n", rtcData.moistureVal, rtcData.timeStamp);
    return rtcData.crc8 == crcOfData;
  }
  return false;
}

uint8_t getRTCData(ushort offset)
{
  if (ESP.rtcUserMemoryRead(offset, (uint32_t *)&rtcData, sizeof(rtcData)))
  {
    return CRC8.smbus((uint8_t *)&rtcData.timeStamp, sizeof(rtcData.timeStamp));
  }
  return 0;
}

void writeRTCData(ushort offset)
{
  // Generate new data set for the struct
  timeClient.update();
  rtcData.timeStamp = timeClient.getEpochTime();
  Serial.println(timeClient.getEpochTime());

  double analogValue = 0.0;
  //double analogVolts = 0.0;

  analogValue = analogRead(A0); // read the analog signal
  //analogVolts = (analogValue * 3.3) / 1024;

  // 900 is totally dry in air
  // 400 is sitting in water

  ushort chartValue = (1 - 1 / (500 / (analogValue - 400))) * 100;
  rtcData.moistureVal = chartValue;
  Serial.printf("Moisture value is: %u \n", chartValue);

  uint8_t crcOfData = CRC8.smbus((uint8_t *)&rtcData.timeStamp, sizeof(rtcData.timeStamp));
  Serial.print("CRC8 of data: ");
  Serial.println(crcOfData, HEX);
  rtcData.crc8 = crcOfData;

  if (ESP.rtcUserMemoryWrite(offset, (uint32_t *)&rtcData, sizeof(rtcData)))
  {
    Serial.println("RTC data written");
    Serial.println();
    printRTCData(offset);
  }
}

void loop() {
  
  ArduinoOTA.handle();

  ulong oneHourLater = 3600 + rtcData.timeStamp;
  if(timeClient.getEpochTime() > oneHourLater) {
    currentOffset = currentOffset + 2;  
    if(currentOffset > 128){
      currentOffset = 0;
    }
    writeRTCData(currentOffset);
  }

  // check to for any web server requests. ie - browser requesting a page from the webserver
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");

  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");

  client.println("<html>");
  client.println(" <head>");
  client.println("<meta http-equiv=\"refresh\" content=\"60\">");
  client.println(" <script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>");
  client.println("  <script type=\"text/javascript\">");
  client.println("    google.charts.load('current', {'packages':['gauge']});");
  client.println("    google.charts.setOnLoadCallback(drawChart);");
  client.println("   function drawChart() {");

  client.println("      var data = google.visualization.arrayToDataTable([ ");
  client.println("        ['Label', 'Value'], ");
  client.print("        ['Moisture',  ");
  client.print(rtcData.moistureVal);
  client.println(" ], ");
  client.println("       ]); ");
  // setup the google chart options here
  client.println("    var options = {");
  client.println("      width: 400, height: 120,");
  client.println("      redFrom: 0, redTo: 25,");
  client.println("      yellowFrom: 25, yellowTo: 75,");
  client.println("      greenFrom: 75, greenTo: 100,");
  client.println("       minorTicks: 5");
  client.println("    };");

  client.println("   var chart = new google.visualization.Gauge(document.getElementById('chart_div'));");

  client.println("  chart.draw(data, options);");

  client.println("  setInterval(function() {");
  client.print("  data.setValue(0, 1, ");
  client.print(rtcData.moistureVal);
  client.println("    );");
  client.println("    chart.draw(data, options);");
  client.println("    }, 13000);");


  client.println("  }");
  client.println(" </script>");

  client.println("  </head>");
  client.println("  <body>");

  client.print("<h1 style=\"size:12px;\">ESP8266 Soil Moisture</h1>");

  // show some data on the webpage and the guage
  client.println("<table><tr><td>");

  client.print("WiFi Signal Strength: ");
  client.println("<br><a href=\"/REFRESH\"\"><button>Refresh</button></a>");

  client.println("</td><td>");
  // below is the google chart html
  client.println("<div id=\"chart_div\" style=\"width: 300px; height: 120px;\"></div>");
  client.println("</td></tr></table>");

  client.println("<br /><table><tr><th>Timestamp</th><th>Moisture level</th><th>CRC8 in Mem</th><th>CRC8 Computed</th></tr>");
  for (short curOffset = 0; curOffset < 128; curOffset = curOffset + 2)
  {
    uint8_t crc8 = getRTCData(curOffset);
    time_t rawtime;
    rawtime = rtcData.timeStamp;
    client.printf("<tr><td>%s</td><td>%u</td><td>%X</td><td>%X</td></tr>\n", ctime(&rawtime), rtcData.moistureVal, rtcData.crc8, crc8);
  }
  client.println("</table>");

  client.println("<body>");
  client.println("</html>");
  delay(1);
  Serial.println("Client disonnected");
  Serial.println("");

}