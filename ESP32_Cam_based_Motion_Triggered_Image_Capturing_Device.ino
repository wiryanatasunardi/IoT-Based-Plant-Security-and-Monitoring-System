#include "esp_camera.h"
#include "FS.h"
#include "SPI.h"
#include "SD.h"
#include "EEPROM.h"
#include "driver/rtc_io.h"
#include "ESP32_MailClient.h"

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

#define ID_ADDRESS            0x00
#define COUNT_ADDRESS         0x01
#define ID_BYTE               0xAA
#define EEPROM_SIZE           0x0F

uint16_t nextImageNumber = 0;

#define WIFI_SSID             "Home"
#define WIFI_PASSWORD         "02101967"


#define emailSenderAccount    "natasunardi61@gmail.com"    //To use send Email for Gmail to port 465 (SSL), less secure app option should be enabled. https://myaccount.google.com/lesssecureapps?pli=1
#define emailSenderPassword   "nata43nuts"

#define emailRecipient        "natasunardi@gmail.com"
//#define emailRecipient2     "recipient2@email.com"


//The Email Sending data object contains config and data to send
SMTPData smtpData;

//Callback function to get the Email sending status
void sendCallback(SendStatus info);

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");

  pinMode(4, INPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);  //diable pin hold if it was enabled before sleeping

  //connect to WiFi network
  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(200);
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
    
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  //init with high specs to pre-allocate larger buffers
  if(psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else 
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  //initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //set the camera parameters
  sensor_t * s = esp_camera_sensor_get();
  s->set_contrast(s, 2);    //min=-2, max=2
  s->set_brightness(s, 2);  //min=-2, max=2
  s->set_saturation(s, 2);  //min=-2, max=2
  delay(100);               //wait a little for settings to take effect
  
  //mount SD card
  Serial.println("Mounting SD Card...");
  MailClient.sdBegin(14,2,15,13);

  if(!SD.begin())
  {
    Serial.println("Card Mount Failed");
    return;
  }

  //initialize EEPROM & get file number
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("Failed to initialise EEPROM"); 
    Serial.println("Exiting now"); 
    while(1);   //wait here as something is not right
  }

  if(EEPROM.read(ID_ADDRESS) != ID_BYTE)    //there will not be a valid picture number
  {
    Serial.println("Initializing ID byte & restarting picture count");
    nextImageNumber = 0;
    EEPROM.write(ID_ADDRESS, ID_BYTE);  
    EEPROM.commit(); 
  }
  else                                      //obtain next picture number
  {
    EEPROM.get(COUNT_ADDRESS, nextImageNumber);
    nextImageNumber +=  1;    
    Serial.print("Next image number:");
    Serial.println(nextImageNumber);
  }

  //take new image
  camera_fb_t * fb = NULL;
    
  //obtain camera frame buffer
  fb = esp_camera_fb_get();
  if (!fb) 
  {
    Serial.println("Camera capture failed");
    Serial.println("Exiting now"); 
    while(1);   //wait here as something is not right
  }

  //save to SD card
  //generate file path
  String path = "/IMG" + String(nextImageNumber) + ".jpg";
    
  fs::FS &fs = SD;

  //create new file
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file)
  {
    Serial.println("Failed to create file");
    Serial.println("Exiting now"); 
    while(1);   //wait here as something is not right    
  } 
  else 
  {
    file.write(fb->buf, fb->len); 
    EEPROM.put(COUNT_ADDRESS, nextImageNumber);
    EEPROM.commit();
  }
  file.close();

  //return camera frame buffer
  esp_camera_fb_return(fb);
  Serial.printf("Image saved: %s\n", path.c_str());

  //send email
  Serial.println("Sending email...");
  //Set the Email host, port, account and password
  smtpData.setLogin("smtp.gmail.com", 587, emailSenderAccount, emailSenderPassword);
  
  //Set the sender name and Email
  smtpData.setSender("ESP32-CAM", emailSenderAccount);
  
  //Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("Normal");

  //Set the subject
  smtpData.setSubject("Motion Detected - ESP32-CAM");
    
  //Set the message - normal text or html format
  smtpData.setMessage("<div style=\"color:#003366;font-size:20px;\">Image captured and attached.</div>", true);

  //Add recipients, can add more than one recipient
  smtpData.addRecipient(emailRecipient);
  //smtpData.addRecipient(emailRecipient2);
  
  //Add attach files from SD card
  smtpData.addAttachFile(path);
  
  //Set the storage types to read the attach files (SD is default)
  smtpData.setFileStorageType(MailClientStorageType::SD);
  
  smtpData.setSendCallback(sendCallback);
  
  //Start sending Email, can be set callback function to track the status
  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

  //Clear all data from Email object to free memory
  smtpData.empty();

  pinMode(4, OUTPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);            //turn OFF flash LED
  rtc_gpio_hold_en(GPIO_NUM_4);    //make sure flash is held LOW in sleep
  Serial.println("Entering deep sleep mode");
  Serial.flush(); 
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0);   //wake up when pin 13 goes LOW
  delay(10000);                                   //wait for 10 seconds to let PIR sensor settle
  esp_deep_sleep_start();
}

void loop() 
{

}

//Callback function to get the Email sending status
void sendCallback(SendStatus msg)
{
  //Print the current status
  Serial.println(msg.info());

  //Do something when complete
  if (msg.success())
  {
    Serial.println("----------------");
  }
}
