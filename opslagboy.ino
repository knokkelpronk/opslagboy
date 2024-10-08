#include <Wire.h>

// STATUSLEDS
#include <FastLED.h>  // library voor statusleds
#define LED_PIN 13
#define LED_TYPE WS2811
#define LED_NUM 5
#define LED_BRIGHTNESS 255
#define TIJD_KNIPPER 100

// DISPLAY
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#define TFT_DC 10
#define TFT_CS 8
#define TFT_MOSI 11
#define TFT_CLK 12
#define TFT_RST 7
#define TFT_MISO 9
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

//voor uninterupbaar knipperen leds
unsigned long tijd_toen = 0;
CRGB leds[LED_NUM];  // led array nodig voor library

// voor opbouw frame
#define START 0x01        // start of com
#define END 0x04          // end of com
#define SOURCE 0x74       // co ev2a 5 = 1110100
#define DESTINATION 0x24  // cm ev2a 1 = 00100100

// onderdelen functiecode
#define FCODE_01_ONTV 0xC3   // fc 01 ontvang = 11000011
#define FCODE_01_STUUR 0xC0  // fc 01 zend VERANDEREN
#define FCODE_06_STAP1 0xD7  // = 11010111
#define FCODE_06_STAP2 0xD4  // = 11010100
#define FCODE_12 0xEC        // fc 12 ontvang = 0b11101111
#define FCODE_12_DATA 0x6    // zesje

//voor radiohead
#include <RH_ASK.h>
#include <SPI.h>
#define PIN_RX 3
#define PIN_TX 2
RH_ASK rf_driver(2000, PIN_RX, PIN_TX);  // wat is 0?

// exra logica
byte retransmit_teller = 0;
byte lastMessage[50] = { 0 };
uint8_t lastMessageSize = 0;
char schakel = 0;

void statusledRoodContinu() 
{
  // zet elke led op rood 
  for (int i = 0; i < LED_NUM; i++) 
  {
    leds[i] = CRGB::Red;
    FastLED.show();
  }
}

void statusledRoodKnipper() 
{
  // zet elke led op rood knipper. uninterrupteerbaar
  unsigned long tijd_nu = millis();
  if (tijd_nu - tijd_toen >= TIJD_KNIPPER) 
  {
    tijd_toen = tijd_nu;
    if (leds[1] == CRGB::Red) {
      for (int i = 0; i < LED_NUM; i++) 
      {
        leds[i] = CRGB::Black;
        FastLED.show();
      }

    } else 
    {
      for (int i = 0; i < LED_NUM; i++) 
      {
        leds[i] = CRGB::Red;
        FastLED.show();
      }
    }
  }
}

void statusledGeelContinu() 
{
  //  zet elke led op geel
  for (int i = 0; i < LED_NUM; i++) 
  {
    leds[i] = CRGB::Yellow;
    FastLED.show();
  }
}

void statusledGroenContinu() 
{
  // zet elke led op groen
  for (int i = 0; i < LED_NUM; i++) 
  {
    leds[i] = CRGB::Green;
    FastLED.show();
  }
}

void setup() 
{

  Serial.begin(9600); // start serial monitor
  statusledRoodContinu(); 
  tft.fillScreen(ILI9341_BLACK); // reset scherm
  tft.begin(0x9341); // init scherm
  tft.setRotation(3);
  tft.setCursor(20, 50);
  tft.setTextSize(5);
  tft.setTextColor(RED);
  tft.println("OPSTARTEN FLUX CON- DENSATOR ...");
  delay(300);

  // maak tekst aan per module. Wordt later ingevuld
  tft.fillRect(0, 0, 160, 120, YELLOW);
  tft.setCursor(20, 5);
  tft.setTextSize(2);
  tft.setTextColor(GREEN);
  tft.println("Module 1");
  tft.fillRect(160, 0, 160, 120, MAGENTA);
  tft.setCursor(200, 5);
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.println("Module 2");
  tft.fillRect(0, 120, 160, 120, GREEN);
  tft.setCursor(20, 145 - 20);
  tft.setTextSize(2);
  tft.setTextColor(BLUE);
  tft.println("Module 3");
  tft.fillRect(160, 120, 160, 120, BLUE);
  tft.setCursor(170, 145 - 20);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.println("Module 4");

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_NUM);  
  FastLED.setBrightness(255);
  if (rf_driver.init()) 
  {
    Serial.println("rf driver init OK");
  } else 
  {
    Serial.println("rf driver init FAIL");
  }
  delay(400);
}

void reset() 
{
  // maak bericht leeg
  for (int i = 0; i < 50; i++) 
  {
    lastMessage[i] = 0;
  }
  retransmit_teller = 0;
  statusledRoodContinu();
}

void stuurFunctie() 
{ // stuur opgeslagen bericht
  statusledGeelContinu();
  rf_driver.send(lastMessage, lastMessageSize);
  rf_driver.waitPacketSent();
}

void retransmitSend() 
{ 
  retransmit_teller++;
  if (retransmit_teller >= 100) 
  {
    Serial.println("ERROR retransmit > 100");
    statusledRoodKnipper();
  }

  union functiecode1 
  {
    struct 
    {
      uint8_t start;
      uint8_t packetlength;
      uint8_t fc;
      uint8_t source;
      uint8_t destination;
      uint8_t end;
      uint8_t lrc;
    } br;
    uint8_t string[7];
  };

  // opbouw bericht
  uint8_t msglen = sizeof(functiecode1);
  union functiecode1 bericht;
  bericht.br.start = START;
  bericht.br.packetlength = 4;
  bericht.br.fc = FCODE_01_STUUR;
  bericht.br.source = SOURCE;
  bericht.br.destination = DESTINATION;
  bericht.br.end = END;

  ///////////////////////////////////////////////////////////

  //

  //             Code gekopieerd van: Devon

  //

  ///////////////////////////////////////////////////////////
  // make lrc
  uint8_t lrc = 0;
  for (uint8_t i = 0; i < msglen - 1; i++) 
  {
    bericht.br.lrc ^= bericht.string[i];  // xor maken
  }

  lastMessageSize = msglen;
  for (int i = 0; i < msglen; i++) 
  {
    lastMessage[i] = bericht.string[i];
  }
  //////////////////////////////////////////////////////////

  stuurFunctie();
}

void ackSend() 
{  // functiecode 12
  union functiecode12 
  {
    struct 
    {
      uint8_t start;
      uint8_t packetlength;
      uint8_t fc;
      uint8_t source;
      uint8_t destination;
      uint8_t data;
      uint8_t end;
      uint8_t lrc;
    } br;
    uint8_t string[8];
  };

  uint8_t msglen = sizeof(functiecode12);
  union functiecode12 bericht;
  bericht.br.start = START;
  bericht.br.packetlength = 5;
  bericht.br.fc = FCODE_12;
  bericht.br.source = SOURCE;
  bericht.br.destination = DESTINATION;
  bericht.br.data = FCODE_12_DATA;
  bericht.br.end = END;

  ///////////////////////////////////////////////////////////

  //

  //             Code gekopieerd van: Devon

  //

  ///////////////////////////////////////////////////////////

  // make lrc
  uint8_t lrc = 0;
  for (uint8_t i = 0; i < msglen - 1; i++) 
  {
    bericht.br.lrc ^= bericht.string[i];  // xor maken
  }

  lastMessageSize = msglen;
  for (int i = 0; i < msglen; i++) 
  {
    lastMessage[i] = bericht.string[i];
  }
  ///////////////////////////////////////////////////////////
  stuurFunctie();
}

void regenboogXD() 
{
  while (true) 
  {
    statusledRoodContinu();
    delay(100);
    statusledGeelContinu();
    delay(100);
    statusledGroenContinu();
    delay(100);
  }
}

void loop() 
{
  statusledRoodContinu();
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (rf_driver.recv(buf, &buflen)) 
  {
    statusledGroenContinu();

    //opbouw frame. kijkt alleen naar de headerinfo
    uint8_t start = buf[0];
    uint8_t packagelength = buf[1];
    uint8_t function_code = buf[2];
    uint8_t source = buf[3];
    uint8_t destination = buf[4];
    uint8_t lrc = buf[buflen - 1];

    Serial.print("HEX DATA:");
    for (char i = 0; i < buflen; i++)  // Changed strlen(buf) to buflen
    {
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // kijk of bericht voor mij is
    if (destination == SOURCE) 
    {
      Serial.println("opslaan die data!");

      uint8_t lrc_check = 0;
      for (char i = 0; i < buflen - 1; i++) 
      {
        lrc_check ^= buf[i];
      }

      if (lrc != lrc_check) 
      {
        Serial.println("LRC niet goed");
        retransmitSend();
        statusledRoodKnipper(); // knippert zo niet
        return;
      } else 
      {
        retransmit_teller = 0;
        Serial.println("LRC goed");
      }

      char function = buf[2];
      Serial.print("Function code: ");
      Serial.println(function, HEX);

      switch (function_code) 
      {
        case FCODE_06_STAP1:  // fuctiecode 6
          delay(500);
          ackSend();
          break;

        case FCODE_06_STAP2:  // functiecode 6
          ackSend();          // stuur acknowledge

          uint8_t sourceid_mm = buf[4 + 0];
          uint32_t average_temp = buf[4 + 1];
          uint32_t average_light = buf[4 + 5];
          uint32_t average_humidity = buf[4 + 9];
          uint32_t max_temp = buf[4 + 13];
          uint32_t max_light = buf[17 + 4];
          uint32_t max_humidity = buf[21 + 4];
          uint32_t min_temp = buf[25 + 4];
          uint32_t min_light = buf[29 + 4];
          uint32_t min_humidity = buf[33 + 4];

          switch (schakel) 
          {
            default:
              // module 1
              tft.setCursor(30, 30);
              tft.setTextSize(2);
              tft.setTextColor(BLACK);
              tft.println(average_temp);
              tft.setCursor(30, 50);
              tft.setTextSize(2);
              tft.setTextColor(RED);
              tft.println(average_light);
              tft.setCursor(30, 70);
              tft.setTextSize(2);
              tft.setTextColor(BLACK);
              tft.println(average_humidity);
              tft.setTextSize(2);
              tft.setTextColor(BLACK);
              tft.setCursor(30 - 20, 90);
              tft.setTextSize(1);
              tft.setTextColor(RED);
              tft.println("temp, ligt, humidity");
              schakel = 1;
              break;
            case 1:

              // module 2
              tft.setCursor(210, 25);
              tft.setTextSize(2);
              tft.setTextColor(RED);
              tft.println(average_temp);
              tft.setCursor(210, 25 + 20);
              tft.setTextSize(2);
              tft.setTextColor(RED);
              tft.println(average_light);
              tft.setCursor(210, 25 + 40);
              tft.setTextSize(2);
              tft.setTextColor(RED);
              tft.println(average_humidity);
              tft.setCursor(210 - 20, 25 + 60);
              tft.setTextSize(1);
              tft.setTextColor(RED);
              tft.println("temp, ligt, humidity");
              schakel = 2;
              break;

            case 2:
              // module 3
              tft.setCursor(50, 200);
              tft.setTextSize(2);
              tft.setTextColor(YELLOW);
              tft.println(average_temp);
              tft.setCursor(50, 200 - 20);
              tft.setTextSize(2);
              tft.setTextColor(CYAN);
              tft.println(average_light);
              tft.setCursor(50, 200 - 40);
              tft.setTextSize(2);
              tft.setTextColor(BLACK);
              tft.println(average_humidity);
              tft.setCursor(50 - 20, 200 - 60);
              tft.setTextSize(1);
              tft.setTextColor(WHITE);
              tft.println("temp, light, humidity");
              schakel = 3;
              break;

            case 3:
              // module 4
              tft.setCursor(210, 200);
              tft.setTextSize(2);
              tft.setTextColor(RED);
              tft.println(average_temp);
              tft.setCursor(210, 200 - 60);
              tft.setTextSize(2);
              tft.setTextColor(WHITE);
              tft.println(average_light);
              tft.setCursor(210, 200 - 40);
              tft.setTextSize(2);
              tft.setTextColor(YELLOW);
              tft.println(average_humidity);
              tft.setCursor(210 - 20, 200 - 20);
              tft.setTextSize(1);
              tft.setTextColor(RED);
              tft.println("temp, light, humidity");
              schakel = 0;
              regenboogXD();  // ledsjes doen een dansje om te laten zien dat ie klaar is
              break;
          }
          Serial.print("HEX DATA:"); // print ontvangen data in hex
          for (char i = 4; i < buflen - 2; i++) 
          {
            Serial.print(buf[i], HEX);
            Serial.println(" ");
          }
          Serial.println();
          break;

        case FCODE_01_ONTV:  // functiecode 1 (ontvang)
          stuurFunctie();  // stuur opnieuw de laatste packet
          break;

        default:
          Serial.println("ERROR functiecode niet voor jou:");
          Serial.println(function_code, HEX);
      }
    } else Serial.println("niet voor jou");

    reset();
    statusledRoodContinu();
  }

  delay(500); 
}
