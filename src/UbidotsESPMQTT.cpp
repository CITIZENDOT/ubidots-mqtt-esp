/*
Copyright (c) 2016 Ubidots.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Original Maker: Mateo Velez - Metavix for Ubidots Inc
Modified and Maintened by: Jose Garcia - Ubidots Inc

*/

#include "UbidotsESPMQTT.h"

Ubidots::Ubidots(char *token)
{
  String _deviceMac = WiFi.macAddress();
  _clientName = new char[_deviceMac.length() + 1];
  strcpy(_clientName, _deviceMac.c_str());
  initialize(token, _clientName);
}

Ubidots::Ubidots(char *token, char *clientName) { initialize(token, clientName); }

void Ubidots::begin(void (*callback)(char *, uint8_t *, unsigned int))
{
  this->callback = callback;
  _client.setServer(_server, MQTT_PORT);
  _client.setCallback(callback);
}

bool Ubidots::add(char *variableLabel, float value) { return add(variableLabel, value, "NULL", "NULL"); }

bool Ubidots::add(char *variableLabel, float value, char *context)
{
  return add(variableLabel, value, context, "NULL");
}

bool Ubidots::add(char *variableLabel, float value, char *context, char *timestamp)
{
  (val + currentValue)->_variableLabel = variableLabel;
  (val + currentValue)->_value = value;
  (val + currentValue)->_context = context;
  (val + currentValue)->_timestamp = timestamp;
  currentValue++;
  if (currentValue > MAX_VALUES)
  {
    Serial.println(F("You are sending more than the maximum of consecutive variables"));
    currentValue = MAX_VALUES;
  }
  return true;
}

bool Ubidots::connected() { return _client.connected(); }

void Ubidots::initialize(char *token, char *clientName)
{
  _server = SERVER;
  _token = token;
  currentValue = 0;
  val = (Value *)malloc(MAX_VALUES * sizeof(Value));
  _clientName = clientName;
}

bool Ubidots::loop()
{
  if (!_client.connected())
  {
    reconnect();
  }
  return _client.loop();
}

void Ubidots::reconnect()
{
  while (!_client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (_client.connect(_clientName, _token, NULL))
    {
      Serial.println("connected");
      break;
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(_client.state());
      Serial.println(" try again in 3 seconds");
      delay(3000);
    }
  }
}

void Ubidots::setDebug(bool debug) { _debug = debug; }

void Ubidots::ubidotsSetBroker(char *broker)
{
  if (_debug)
  {
    Serial.println("Broker set for Business Account");
  }
  _server = broker;
}

bool Ubidots::ubidotsSubscribe(char *deviceLabel, char *variableLabel)
{
  char topic[150];
  sprintf(topic, "%s%s/%s/lv", FIRST_PART_TOPIC, deviceLabel, variableLabel);
  if (!_client.connected())
  {
    reconnect();
  }
  if (_debug)
  {
    Serial.println("Subscribed to: ");
    Serial.println(topic);
  }
  return _client.subscribe(topic);
}

bool Ubidots::ubidotsPublish(char *deviceLabel)
{
  char topic[150];
  char payload[500];
  String str;
  sprintf(topic, "%s%s", FIRST_PART_TOPIC, deviceLabel);
  sprintf(payload, "{");
  for (int i = 0; i <= currentValue;)
  {
    str = String((val + i)->_value, 2);
    sprintf(payload, "%s\"%s\": [{\"value\": %s", payload, (val + i)->_variableLabel, str.c_str());
    if ((val + i)->_timestamp != "NULL")
    {
      sprintf(payload, "%s, \"timestamp\": %s", payload, (val + i)->_timestamp);
    }
    if ((val + i)->_context != "NULL")
    {
      sprintf(payload, "%s, \"context\": {%s}", payload, (val + i)->_context);
    }
    i++;
    if (i >= currentValue)
    {
      sprintf(payload, "%s}]}", payload);
      break;
    }
    else
    {
      sprintf(payload, "%s}], ", payload);
    }
  }
  if (_debug)
  {
    Serial.println("publishing to TOPIC: ");
    Serial.println(topic);
    Serial.print("JSON dict: ");
    Serial.println(payload);
  }
  currentValue = 0;
  return _client.publish(topic, payload, 512);
}

bool Ubidots::wifiConnection(char *ssid, char *pass)
{
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
}

bool Ubidots::sendMail(
    String smtpServer,
    int PORT,
    String fromEmailBase64,
    String fromPasswordBase64,
    String fromEmail,
    String toEmail,
    String subject,
    String message)
{
  String fromType1 = "MAIL From: " + fromEmail;
  String fromType2 = "From: " + fromEmail;
  String toType1 = "RCPT To: " + toEmail;
  String toType2 = "To: " + toEmail;
  String formattedSubject = "Subject: " + subject + "\r\n";

  if (espClient.connect(smtpServer, PORT) == 1)
  {
    Serial.println(F("connected"));
  }
  else
  {
    Serial.println(F("connection failed"));
    return 0;
  }
  if (!emailResp())
    return 0;

  Serial.println(F("Sending EHLO"));
  espClient.println("EHLO friend");
  if (!emailResp())
    return 0;

  Serial.println(F("Sending auth login"));
  espClient.println("AUTH LOGIN");
  if (!emailResp())
    return 0;

  Serial.println(F("Sending User"));
  espClient.println(fromEmailBase64);
  if (!emailResp())
    return 0;

  Serial.println(F("Sending Password"));
  espClient.println(fromPasswordBase64);
  if (!emailResp())
    return 0;

  Serial.println(F("Sending From"));

  espClient.println(fromType1);
  if (!emailResp())
    return 0;

  Serial.println(F("Sending To"));

  espClient.println(toType1);
  if (!emailResp())
    return 0;

  Serial.println(F("Sending DATA"));
  espClient.println("DATA");
  if (!emailResp())
    return 0;

  Serial.println(F("Sending email"));
  espClient.println(toType2);
  espClient.println(fromType2);
  espClient.println(formattedSubject);
  espClient.println(message);
  espClient.println(".");
  if (!emailResp())
    return 0;

  Serial.println(F("Sending QUIT"));
  espClient.println(F("QUIT"));
  if (!emailResp())
    return 0;

  Serial.println(F("disconnected"));
  return 1;
}

byte Ubidots::emailResp()
{
  byte responseCode;
  byte readByte;
  int loopCount = 0;

  while (!espClient.available())
  {
    delay(1);
    loopCount++;

    if (loopCount > 20000)
    {
      espClient.stop();
      Serial.println(F("\r\nTimeout"));
      return 0;
    }
  }

  responseCode = espClient.peek();
  while (espClient.available())
  {
    readByte = espClient.read();
    Serial.write(readByte);
  }

  if (responseCode >= '4')
  {
    return 0;
  }
  return 1;
}
