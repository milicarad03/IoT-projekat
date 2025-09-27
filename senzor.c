#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mosquitto.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 

#define BROKER "localhost"
#define PORT 1883
#define SSDP_ADDR "239.255.255.250"
#define SSDP_PORT 1900
#define COMMAND_TOPIC "bridges/mostA/commands/%s" // %s za DEVICE_ID

//const char *DEVICE_ID = "sensorA";
//const char *DEVICE_ID = "sensorB";
const char *DEVICE_ID = "sensorC";

int is_active = 1;
int active_period = 10;
int sleep_period = 10;

void save_device_state(const char *device_id, const char *sensor_type, float value) {
    FILE *db = fopen("devices.db", "a");
    if (db) {
        fprintf(db, "%s|%s|%.2f|%s\n", device_id, sensor_type, value, __TIME__);
        fclose(db);
    } else {
        fprintf(stderr, "[ERROR] Failed to open devices.db\n");
    }
}

void publish(struct mosquitto *mosq, const char *topic, const char *device_id, const char *stype, double value) {
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{ \"device_id\":\"%s\", \"sensor_type\":\"%s\", \"value\":%.3f }",
        device_id, stype, value);

    if (mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, 0, false) == MOSQ_ERR_SUCCESS)
        
        printf("[INFO] Published [%s]: %s\n", topic, payload);
    else
        fprintf(stderr, "[ERROR] Publish failed\n");
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    if (message->payloadlen) {
        char *payload = malloc(message->payloadlen + 1);
        memcpy(payload, message->payload, message->payloadlen);
        payload[message->payloadlen] = '\0';
        printf("----------------------------------------\n");
        printf("[INFO] Received command [%s]: %s\n", message->topic, payload);
        printf("----------------------------------------\n");

        char topic_cmd[64];
        snprintf(topic_cmd, sizeof(topic_cmd), COMMAND_TOPIC, DEVICE_ID);
        if (strcmp(message->topic, topic_cmd) == 0) {
            if (strcmp(payload, "start") == 0) {
                is_active = 1;
                printf("----------------------------------------\n");
                printf("[INFO_DEVICE] Device activated\n");
            }
            else if (strcmp(payload, "stop") == 0) {
                is_active = 0;
                printf("----------------------------------------\n");
                printf("[INFO_DEVICE] Device deactivated\n");
            }
            else if (strncmp(payload, "set_period", 10) == 0) {
                int new_period = atoi(payload + 11);
                if (new_period > 0) {
                    active_period = new_period;
                    printf("----------------------------------------\n");
                    printf("[INFO_PERIOD] Period set to %d seconds\n", active_period);
                }
            }
        }
        free(payload);
    }
}

int discover_controller() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }

    int loop = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("IP_ADD_MEMBERSHIP");
        close(sock);
        return 0;
    }

    struct sockaddr_in dest, recv_addr;
    char buf[1024];
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(SSDP_ADDR);
    dest.sin_port = htons(SSDP_PORT);

    const char *msearch =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 1\r\n"
        "ST: upnp:rootdevice\r\n\r\n";

    struct timeval tv;
    tv.tv_sec = 5; 
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    socklen_t recvlen = sizeof(recv_addr);
    for (int i = 0; i < 5; i++) {
        if (sendto(sock, msearch, strlen(msearch), 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            perror("sendto");
        }
        printf("----------------------------------------\n");
        printf("[INFO] Sent M-SEARCH (attempt %d)\n", i + 1);

        int n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&recv_addr, &recvlen);
        if (n > 0) {
            buf[n] = '\0';
            printf("----------------------------------------\n");
            printf("[INFO] Got RESPONSE:\n%s\n", buf);
            close(sock);
            return 1;
        }
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
        sleep(1);
    }

    close(sock);
    printf("----------------------------------------\n");
    printf("[INFO] Got RESPONSE: (simulated for localhost)\n");
    return 1;
}

int main() {
    if (!discover_controller()) { 
        printf("[ERROR] No controller found\n"); 
        return 1; 
    }

    srand(time(NULL));
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new(DEVICE_ID, true, NULL);
    if (!mosq) { 
        fprintf(stderr, "Failed to create mosquitto client\n"); 
        return 1;
    }
    mosquitto_message_callback_set(mosq, on_message);
    if (mosquitto_connect(mosq, BROKER, PORT, 60) != MOSQ_ERR_SUCCESS) { 
        fprintf(stderr, "Could not connect to broker\n"); 
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1; 
    }

    char topic_cmd[64];
    snprintf(topic_cmd, sizeof(topic_cmd), COMMAND_TOPIC, DEVICE_ID);
    mosquitto_subscribe(mosq, NULL, topic_cmd, 0);
    printf("[INFO] Subscribed to topic: %s\n", topic_cmd);

    time_t last_activity = time(NULL);
    time_t last_publish = time(NULL);

    while (1) {
        time_t current_time = time(NULL);
        if (mosquitto_loop(mosq, -1, 1) != MOSQ_ERR_SUCCESS) {
            printf("[ERROR] Connection lost, attempting reconnect...\n");
            mosquitto_reconnect(mosq);
        } else if (is_active) {
            double time_since_last_publish = difftime(current_time, last_publish);
            if (time_since_last_publish >= 2) {
                if (strcmp(DEVICE_ID, "sensorA") == 0) {
                    double temperature = 20.0 + ((double)rand() / RAND_MAX) * 50.0;
                    publish(mosq, "bridges/mostA/sensors/temperature", DEVICE_ID, "temperature", temperature);
                    save_device_state(DEVICE_ID, "temperature", temperature);
                } else if (strcmp(DEVICE_ID, "sensorB") == 0) {
                    double vibration = ((double)rand() / RAND_MAX) * 4.0;
                    double ultrasonic = ((double)rand() / RAND_MAX) * 10.0;
                    
                    publish(mosq, "bridges/mostA/sensors/vibration", DEVICE_ID, "vibration", vibration);
                    publish(mosq, "bridges/mostA/sensors/ultrasonic", DEVICE_ID, "ultrasonic", ultrasonic);
                    
                    save_device_state(DEVICE_ID, "vibration", vibration);
                    save_device_state(DEVICE_ID, "ultrasonic", ultrasonic);
                } else if (strcmp(DEVICE_ID, "sensorC") == 0) {
                    double strain = 0.002 + ((double)rand() / RAND_MAX) * 0.006;
                    double vibration = ((double)rand() / RAND_MAX) * 4.0;
                    double temperature = 20.0 + ((double)rand() / RAND_MAX) * 50.0;
                    double ultrasonic = ((double)rand() / RAND_MAX) * 10.0;
                    
                    publish(mosq, "bridges/mostA/sensors/strain", DEVICE_ID, "strain", strain);
                    publish(mosq, "bridges/mostA/sensors/vibration", DEVICE_ID, "vibration", vibration);
                    publish(mosq, "bridges/mostA/sensors/temperature", DEVICE_ID, "temperature", temperature);
                    publish(mosq, "bridges/mostA/sensors/ultrasonic", DEVICE_ID, "ultrasonic", ultrasonic);
                    
                    save_device_state(DEVICE_ID, "strain", strain);
                    save_device_state(DEVICE_ID, "vibration", vibration);
                    save_device_state(DEVICE_ID, "temperature", temperature);
                    save_device_state(DEVICE_ID, "ultrasonic", ultrasonic);
                }
                
                last_publish = current_time;
                if (last_activity == time(NULL)) 
                    last_activity = current_time;
                printf("----------------------------------------\n");
                printf("[INFO_TIME] Time since last publish: %.1f seconds | Last activity: %ld \n", time_since_last_publish, (long)last_activity);
                printf("----------------------------------------\n");
            }
            
        } 
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}