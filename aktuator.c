#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <unistd.h>

#define BROKER "localhost"
#define PORT 1883
#define ACTUATOR_TOPIC "bridges/mostA/actuators/alert"

void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    if (rc == 0) {
        printf("[INFO] Multi actuator: Connected to MQTT Broker\n");
        mosquitto_subscribe(mosq, NULL, ACTUATOR_TOPIC, 0);
        printf("----------------------------------------\n");
        printf("[INFO] Subscribed to topic: %s\n", ACTUATOR_TOPIC);
    } else {
        fprintf(stderr, "[ERROR] Failed to connect to broker: %d\n", rc);
    }
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    //printf("[INFO] Received topic: %s | Payload: %s\n", msg->topic, (char *)msg->payload);
    
    char device_id[64], sensor_type[64];
    double value = 0.0;
    if (sscanf(msg->payload, "{ \"device_id\":\"%63[^\"]\", \"sensor_type\":\"%63[^\"]\", \"value\":%lf }", 
               device_id, sensor_type, &value) != 3) {
        printf("----------------------------------------\n");
        printf("[ERROR] Malformed payload: %s\n", (char *)msg->payload);
        return;
    }

    // PROVERI VREDNOSTI 
    if (strcmp(sensor_type, "temperature") == 0) {
        if (value > 60.0) {
            printf("![WARNING]: High temperature detected (%.2f°C) from %s! Take action!\n", 
                   value, device_id);
        } 
    } else if (strcmp(sensor_type, "strain") == 0) {
        if (value > 0.005) {
            printf("![WARNING]: High strain detected (%.4f) from %s! Structural risk!\n", 
                   value, device_id);
        } 
    } else if (strcmp(sensor_type, "vibration") == 0) {
        if (value > 2.5) {
            printf("![WARNING]: High vibration detected (%.2f) from %s! Check stability!\n", 
                   value, device_id);
        } 
    } else if (strcmp(sensor_type, "ultrasonic") == 0) {
        if (value < 2.0) {
            printf("![WARNING]: Crack detected (%.2f) from %s! Inspect immediately!\n", 
                   value, device_id);
        } 
    } else {
        printf("----------------------------------------\n");
        printf("[ERROR] Unknown sensor type: %s from %s\n", sensor_type, device_id);
    }
}

int main() {
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("multi-actuator", true, NULL);
    if (!mosq) {
        fprintf(stderr, "[ERROR] Failed to create mosquitto client\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, BROKER, PORT, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "[ERROR] Could not connect to broker\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    while (1) {
        if (mosquitto_loop(mosq, -1, 1) != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "[ERROR] Connection lost, attempting reconnect...\n");
            sleep(1);
            mosquitto_reconnect(mosq);
        }
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}