from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient
import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime
import signal
import pyconfig


# Set the callback function for MQTT messages
def on_message(_client, _userdata, message):
    """Handle messages"""

    print('-----')

    date = datetime.now().strftime('%d-%m-%YT%H:%M:%S')

    print(date + ' - Received message: ' + str(message.payload))

    # Parse the incoming JSON string into a dictionary
    payload = json.loads(message.payload)

    # If "flame" key exists, publish in MQTT_PUB_TOPIC_FIRE
    if "flame" in payload:

        # Add datetime information as the second field
        new_payload = {
            'id': payload['id'],
            'datetime': date,
            'voltage': payload['voltage'],
            'flame': payload['flame']
        }

        # Convert the resulting dictionary back into a JSON string
        json_payload = json.dumps(new_payload)

        # Topic will be MQTT_PUB_TOPIC_FIRE
        topic = MQTT_PUB_TOPIC_FIRE

    else:
        # Do something if "fire" key does not exist
        new_payload = {
            'id': payload['id'],
            'datetime': date,
            'voltage': payload['voltage'],
            'gas': payload['gas']
        }

        # Convert the resulting dictionary back into a JSON string
        json_payload = json.dumps(new_payload)

        # Topic will be MQTT_PUB_TOPIC_AIR
        topic = MQTT_PUB_TOPIC_AIR

    success = myMQTTClient.publish(topic, json_payload, 0)

    time.sleep(5)
    print(success)
    print('-----')
    
# On connect subscribe to topic
def on_connect(_client, _userdata, _flags, result):
    """Subscribe to input topic"""

    print('Connected ' + str(result))
    myMQTTClient.publish(MQTT_PUB_TOPIC_FIRE, "ciao", 0)
    print("ho inviato ciao")

    print('Subscribing to ' + MQTT_SUB_TOPIC)
    MQTT_CLIENT.subscribe(MQTT_SUB_TOPIC)

# Disconnect function
def disconnect_clients(signum, frame):
    MQTT_CLIENT.loop_stop()
    MQTT_CLIENT.disconnect()
    myMQTTClient.disconnect()
    print("Disconnected from clients")
    exit(0)

# Register signal handler for CTRL+C
signal.signal(signal.SIGINT, disconnect_clients)

# MQTT broker settings
MQTT_BROKER_ADDR = pyconfig.MQTT_BROKER_ADDR
MQTT_BROKER_PORT = 1883
MQTT_BROKER_CLIENT_ID = "broker"

# AWS IoT settings
AWS_IOT_ENDPOINT = pyconfig.AWS_IOT_ENDPOINT
AWS_IOT_PORT = 8883
AWS_IOT_CLIENT_ID = pyconfig.AWS_IOT_CLIENT_ID

# Set the relative path to the AWS IoT Root CA file
AWS_IOT_ROOT_CA = pyconfig.AWS_IOT_ROOT_CA

# Set the relative path to the AWS IoT Private Key file
AWS_IOT_PRIVATE_KEY = pyconfig.AWS_IOT_PRIVATE_KEY

# Set the relative path to the AWS IoT Certificate file
AWS_IOT_CERTIFICATE = pyconfig.AWS_IOT_CERTIFICATE

# For certificate based connection
myMQTTClient = AWSIoTMQTTClient(AWS_IOT_CLIENT_ID)

# Configurations
# For TLS mutual authentication
myMQTTClient.configureEndpoint(AWS_IOT_ENDPOINT, 8883)
myMQTTClient.configureCredentials(AWS_IOT_ROOT_CA, AWS_IOT_PRIVATE_KEY, AWS_IOT_CERTIFICATE)

myMQTTClient.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
myMQTTClient.configureDrainingFrequency(2)  # Draining: 2 Hz
myMQTTClient.configureConnectDisconnectTimeout(10)  # 10 sec
myMQTTClient.configureMQTTOperationTimeout(5)  # 5 sec

#TOPIC
MQTT_SUB_TOPIC = "flamex"
MQTT_PUB_TOPIC_FIRE = "sensor/fire"
MQTT_PUB_TOPIC_AIR = "sensor/gas"

# Create a MQTT client instance
MQTT_CLIENT = mqtt.Client(client_id=MQTT_BROKER_CLIENT_ID)

# MQTT callback function
def main():
    MQTT_CLIENT.on_connect = on_connect
    MQTT_CLIENT.on_message = on_message
    MQTT_CLIENT.connect(MQTT_BROKER_ADDR, MQTT_BROKER_PORT)
    myMQTTClient.connect()
    MQTT_CLIENT.loop_forever()

if __name__ == '__main__':
    main()
