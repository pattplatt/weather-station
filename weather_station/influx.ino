#define DEVICE "ESP8266_1"
#include <ESP8266WiFi.h>
#include <PolledTimeout.h>
#include <AALeC.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "72_OeERcVjvtvvgeLEINprI5yi3RMv860Vrc1viQCgqjEYNhyieCMx7xg6zRMdls5D_myugtXaIQr0ptNDMZfw=="
#define INFLUXDB_ORG "fca6a4168d956284"
#define INFLUXDB_BUCKET "timeseriesTIPSV2"

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
InfluxDBClient clientInflux(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensor_temp("temperatur_sensor");
Point sensor_hum("humidity_sensor");

//function that connects to influx and initiates points to write
void startInflux() {
  Serial.print("Connecting to wifi");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println(" Influx is running");
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  sensor_temp.addTag("sensorID", DEVICE);
  sensor_temp.addTag("sensorSSID", WiFi.SSID());
  sensor_hum.addTag("sensorID", DEVICE);
  sensor_hum.addTag("sensorSSID", WiFi.SSID());
  Serial.print(DEVICE);
  Serial.println(WiFi.SSID());

  if (clientInflux.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(clientInflux.getServerUrl());

  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(clientInflux.getLastErrorMessage());
  }
}
String sensorName;
//function that sends query to influx and returns result
String queryInflux(String *time,String *field){
  if(*field == "temperature"){
    sensorName = "temperatur_sensor";
  }
  else{
    sensorName = "humidity_sensor";
  }
    String query = "from(bucket: \"timeseriesTIPSV2\") |> range(start:-"+*time+"d) |> filter(fn: (r) => r._measurement == \""+sensorName+"\"and r._field == \"" + *field + "\"";
    query += "and r.sensorID == \"ESP8266_1\")";
    query += "|> aggregateWindow(every: 24h, fn: max)";
    String resultQuery;
    
    FluxQueryResult result = clientInflux.query(query);
    Serial.println("query: " + query);
    // Iterate over rows. Even there is just one row, next() must be called at least once.
    while (result.next()) {
      FluxDateTime timeFlux = result.getValueByName("_time").getDateTime();
      String timeStr = timeFlux.format("%F");
      // Get converted value for flux result column '_value' where there is RSSI value
      long value = result.getValueByName("_value").getLong();
      resultQuery += timeStr + "| " + *field + ":  ";
      resultQuery += String(value) + " \n";
    }
     Serial.println("result " + resultQuery);
    // Check if there was an error
    if(result.getError() != "") {
      return result.getError();
      Serial.println(result.getError());
    }
    else{
      return resultQuery;
    }
}

//function to save data on influx
void writeData(int *temp, int *hum){
  // Clear fields for reusing the point. Tags will remain the same as set above.
  sensor_temp.clearFields();
  sensor_hum.clearFields();
  sensor_temp.addField("rssi", WiFi.RSSI());
  sensor_hum.addField("rssi", WiFi.RSSI());
  sensor_temp.addField("temperature", *temp);
  sensor_hum.addField("humidity", *hum);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(sensor_temp.toLineProtocol());
  Serial.print("Writing: ");
  Serial.println(sensor_hum.toLineProtocol());

  //print on aalec led screen
  aalec.print_line(1, "Temperatur: " + String(*temp) + "°C");
  aalec.print_line(2, "Feuchte: " + String(*hum) + "%");

  // Write points
  if (!clientInflux.writePoint(sensor_temp)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(clientInflux.getLastErrorMessage());
  }

  if (!clientInflux.writePoint(sensor_hum)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(clientInflux.getLastErrorMessage());
  }
}