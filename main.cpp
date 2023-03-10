/**
* Weather Station using the Cayenne MQTT C++ library to send sensor data. 
* X-NUCLEO-IDW01M1 WiFi expansion board is used via X_NUCLEO_IDW01M1v2 library.
*/

/* Includes */
#include "mbed.h"
#include "XNucleoIKS01A2.h"


/*=========Start Wifi Module==============*/ 
#include "MQTTTimer.h"
#include "CayenneMQTTClient.h"
#include "MQTTNetworkIDW01M1.h"
#include "SpwfInterface.h"
// WiFi network info.
char* ssid = "SpectrumSetup-40";  //Wifi ssid and pwd to be replaced
char* wifiPassword = "easycow333";

// Cayenne authentication info. This was obtained from the Cayenne Dashboard.
char* username = "6b701820-9c39-11ec-8da3-474359af83d7";
char* password = "67505d2f0ab81922fcdc6d20d8fe57da9f8143bd";
char* clientID = "4272a3b0-d0b5-11ec-9f5b-45181495093e";

/*Pin Connection*/

SpwfSAInterface interface(D8, D2); // TX, RX
MQTTNetwork<SpwfSAInterface> network(interface);


CayenneMQTT::MQTTClient<MQTTNetwork<SpwfSAInterface>, MQTTTimer> mqttClient(network, 
                                                    username, password, clientID);


#if !defined(IDB0XA1_D13_PATCH)
DigitalOut led1(LED1, 1);   // LED conflicts SPI_CLK in case of D13 patch
#endif


Serial pc(USBTX,USBRX);
/* Instantiate the expansion board */
static XNucleoIKS01A2 *mems_expansion_board = XNucleoIKS01A2::instance(D14, D15, D4, D5);


/* Retrieve the composing elements of the expansion board */
static HTS221Sensor *hum_temp = mems_expansion_board->ht_sensor;
static LPS22HBSensor *press_temp = mems_expansion_board->pt_sensor;


/**
* Connect to the Cayenne server.
* @return Returns success if the connection succeeds, or error otherwise.
*/
int connectClient(void)
{
    int error = 0;
    
    printf("Connecting to %s:%d\n", CAYENNE_DOMAIN, CAYENNE_PORT);
    while ((error = network.connect(CAYENNE_DOMAIN, CAYENNE_PORT)) != 0) {
        printf("TCP connect failed, error: %d\n", error);
        wait(2);
    }

    if ((error = mqttClient.connect()) != MQTT::SUCCESS) {
        printf("MQTT connect failed, error: %d\n", error);
        return error;
    }
    printf("Connected\n");
    

    // Subscribe to required topics.
    if ((error = mqttClient.subscribe(COMMAND_TOPIC, CAYENNE_ALL_CHANNELS)) != CAYENNE_SUCCESS) {
        printf("Subscription to Command topic failed, error: %d\n", error);
    }
    if ((error = mqttClient.subscribe(CONFIG_TOPIC, CAYENNE_ALL_CHANNELS)) != CAYENNE_SUCCESS) {
        printf("Subscription to Config topic failed, error:%d\n", error);
    }

    // Send device info. Here we just send some example values for the system info. These should be changed to use actual system data, or removed if not needed.
    mqttClient.publishData(SYS_VERSION_TOPIC, CAYENNE_NO_CHANNEL, NULL, NULL, CAYENNE_VERSION);
    mqttClient.publishData(SYS_MODEL_TOPIC, CAYENNE_NO_CHANNEL, NULL, NULL, "mbedDevice");


    return CAYENNE_SUCCESS;
}

/**
* Main loop where MQTT code is run.
*/
void loop(void)
{
    float temperature=0,pressure=0,humidity=0;
   

    // Start the countdown timer for publishing data every 5 seconds. Change the timeout parameter to publish at a different interval.
    MQTTTimer timer(5000);
    

    while (true) {
        // Yield to allow MQTT message processing.
        mqttClient.yield(1000);


        // Check that we are still connected, if not, reconnect.
        if (!network.connected() || !mqttClient.connected()) {
            network.disconnect();
            mqttClient.disconnect();
            printf("Reconnecting\n");
            while (connectClient() != CAYENNE_SUCCESS) {
                wait(2);
                printf("Reconnect failed, retrying\n");
            }
        }

        // Publish some example data every few seconds. This should be changed to send your actual data to Cayenne.
        if (timer.expired()) {
            int error = 0;
            
            press_temp->get_fahrenheit(&temperature);
            press_temp->get_pressure(&pressure);
            hum_temp->get_humidity(&humidity );
            pc.printf(" Temp:% .2f Fahrenheit, Pressure:%.2f hectopascal, Humdity:%.2f \r\n", temperature, pressure, humidity);

            if ((error = mqttClient.publishData(DATA_TOPIC, 0, TYPE_TEMPERATURE, UNIT_FAHRENHEIT, temperature ))
                  != CAYENNE_SUCCESS) {
                printf("Publish temperature failed, error: %d\n", error);
            }

            if ((error = mqttClient.publishData(DATA_TOPIC, 1, TYPE_RELATIVE_HUMIDITY, UNIT_PERCENT, humidity )) != CAYENNE_SUCCESS) {
                printf("Publish humidity failed, error: %d\n", error);
            }
            if ((error = mqttClient.publishData(DATA_TOPIC, 2, TYPE_BAROMETRIC_PRESSURE, UNIT_HECTOPASCAL, pressure)) != CAYENNE_SUCCESS) {
                printf("Publish barometric pressure failed, error: %d\n", error);
            }

            // Restart the countdown timer for publishing data every 5 seconds. Change the timeout parameter to publish at a different interval.
            timer.countdown_ms(5000);
        }
    }
}

/*=========End Wifi Module==============*/ 



int main() {

  /* Enable all sensors */
  hum_temp->enable();
  press_temp->enable();

            
  printf("Initializing interface\n");
  interface.connect(ssid, wifiPassword, NSAPI_SECURITY_WPA2);

  // Connect to Cayenne.
  if (connectClient() == CAYENNE_SUCCESS) {
      // Run main loop.
      loop();
  }
  else {
      printf("Connection failed, exiting\n");
  }

  if (mqttClient.connected())
      mqttClient.disconnect();

  if (network.connected())
      network.disconnect();

  return 0;

}
