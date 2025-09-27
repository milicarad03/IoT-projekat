#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mosquitto.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BROKER "localhost"
#define PORT 1883
#define SSDP_ADDR "239.255.255.250"
#define SSDP_PORT 1900

#define SENSOR_TOPIC "bridges/mostA/sensors/#"
#define COMMAND_TOPIC "bridges/mostA/commands/%s"

const char *allowed_devices[] = { "sensorA", "sensorB", "sensorC" };
const int num_allowed_devices = 3;
volatile int notify_enabled = 1;
volatile int notify_stopped = 0;

typedef struct {
    char device_id[20];
    time_t last_seen;
} Device;
Device active_devices[10];
int device_count = 0;

int is_authorized(const char *device_id) {
    for (int i = 0; i < num_allowed_devices; i++) {
        if (strcmp(device_id, allowed_devices[i]) == 0)
            return 1;
    }
    return 0;
}

void save_device_state(const char *device_id, const char *sensor_type, float value) {
    FILE *db = fopen("devices.db", "a");
    if (db) {
        fprintf(db, "%s|%s|%.2f|%s\n", device_id, sensor_type, value, __TIME__);
        fclose(db);
    }
}

void update_device_status(const char *device_id) {
    for (int i = 0; i < device_count; i++) {
        if (strcmp(active_devices[i].device_id, device_id) == 0) {
            active_devices[i].last_seen = time(NULL);
            return;
        }
    }
    strncpy(active_devices[device_count].device_id, device_id, 19);
    active_devices[device_count].last_seen = time(NULL);
    device_count++;
}

void check_inactive_devices() {
    time_t now = time(NULL);
    for (int i = 0; i < device_count; i++) {
        if (difftime(now, active_devices[i].last_seen) > 10) {
            printf("----------------------------------------\n");
            printf("[ERROR] Device %s marked as inactive\n", active_devices[i].device_id);
            printf("----------------------------------------\n");
            printf("[INFO] Sent SSDP NOTIFY (ssdp:byebye) for %s\n", active_devices[i].device_id);
            for (int j = i; j < device_count - 1; j++) {
                active_devices[j] = active_devices[j + 1];
            }
            device_count--;
            i--;
        }
    }
}

void notify_actuator(struct mosquitto *mosq, const char *device_id, const char *sensor_type, double value) {
    char topic[] = "bridges/mostA/actuators/alert";
    char payload[256];
    snprintf(payload, sizeof(payload), "{ \"device_id\":\"%s\", \"sensor_type\":\"%s\", \"value\":%.3f }", 
             device_id, sensor_type, value);
    
    if (mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, 0, false) == MOSQ_ERR_SUCCESS) {
        printf("----------------------------------------\n");
        printf("[INFO] Notified actuator [%s]: %s\n", topic, payload);
    } else {
        fprintf(stderr, "[ERROR] Actuator notification failed\n");
    }
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    printf("----------------------------------------\n");
    printf("[INFO] Received topic: %s | Payload: %s\n", msg->topic, (char *)msg->payload);
    char device_id[64], sensor_type[64];
    double value = 0.0;

    if (sscanf(msg->payload,
               "{ \"device_id\":\"%63[^\"]\", \"sensor_type\":\"%63[^\"]\", \"value\":%lf }",
               device_id, sensor_type, &value) != 3) {
        printf("----------------------------------------\n");
        printf("[ERROR] Malformed payload: %s\n", (char *)msg->payload);
        return;
    }

    if (!is_authorized(device_id)) {
        if (!notify_stopped) {
            printf("----------------------------------------\n");
            printf("[ERROR] Unauthorized device detected: %s | Stopping NOTIFY\n", device_id);
            notify_stopped = 1;
        }
        notify_enabled = 0;
        return;
    }

    update_device_status(device_id);
    save_device_state(device_id, sensor_type, value);

    int alert_triggered = 0;
    if (strcmp(sensor_type, "strain") == 0 && value > 0.005) {
        printf("----------------------------------------\n");
        printf("[ALERT]: STRAIN TOO HIGH (%.4f) from %s\n", value, device_id);
        notify_actuator(mosq, device_id, sensor_type, value);
        alert_triggered = 1;
    } else if (strcmp(sensor_type, "vibration") == 0 && value > 2.5) {
        printf("----------------------------------------\n");
        printf("[ALERT]: VIBRATION TOO HIGH (%.2f) from %s\n", value, device_id);
        notify_actuator(mosq, device_id, sensor_type, value); 
        alert_triggered = 1;
    } else if (strcmp(sensor_type, "temperature") == 0 && value > 60.0) {
        printf("----------------------------------------\n");
        printf("[ALERT]: TEMPERATURE TOO HIGH (%.2f) from %s\n", value, device_id);
        notify_actuator(mosq, device_id, sensor_type, value);
        alert_triggered = 1;
    } else if (strcmp(sensor_type, "ultrasonic") == 0 && value < 2.0) {
        printf("----------------------------------------\n");
        printf("[ALERT]: CRACK DETECTED (%.2f) from %s\n", value, device_id);
        notify_actuator(mosq, device_id, sensor_type, value); 
        alert_triggered = 1;
    } 

    if (!alert_triggered) {
        printf("----------------------------------------\n");
        printf("[INFO] OK: %s = %.2f from %s\n", sensor_type, value, device_id);
    }
}

void *notify_loop(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return NULL; }

    int loop = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(SSDP_ADDR);
    dest.sin_port = htons(SSDP_PORT);

    const char *notify_msg =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:alive\r\n"
        "LOCATION: http://127.0.0.1:8080/description.xml\r\n\r\n";

    while (1) {
        if (notify_enabled) {
            if (sendto(sock, notify_msg, strlen(notify_msg), 0,
                       (struct sockaddr *)&dest, sizeof(dest)) < 0) {
                perror("sendto");
            }
            printf("----------------------------------------\n");
            printf("[INFO] Sent SSDP NOTIFY\n");
            sleep(10);
        } else {
            printf("----------------------------------------\n");
            printf("[ERROR] NOTIFY disabled due to unauthorized device\n");
            break;
        }
    }
    close(sock);
    return NULL;
}

void *ssdp_loop(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return NULL; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int loop = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("IP_ADD_MEMBERSHIP");
        close(sock);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SSDP_PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return NULL;
    }

    char buf[1024];
    struct sockaddr_in recv_addr;
    socklen_t recvlen = sizeof(recv_addr);

    while (1) {
        int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&recv_addr, &recvlen);
        if (n > 0) {
            buf[n] = '\0';
            printf("Received packet: %s\n", buf);
            if (strstr(buf, "M-SEARCH")) {
                if (notify_enabled) {
                    printf("[INFO] Received valid M-SEARCH\n");
                    const char *resp =
                        "HTTP/1.1 200 OK\r\n"
                        "CACHE-CONTROL: max-age=1800\r\n"
                        "LOCATION: http://127.0.0.1:8080/description.xml\r\n"
                        "ST: upnp:rootdevice\r\n"
                        "USN: uuid:controller-bridge-001::upnp:rootdevice\r\n\r\n";

                    if (sendto(sock, resp, strlen(resp), 0,
                               (struct sockaddr *)&recv_addr, recvlen) < 0) {
                        perror("sendto");
                    }
                    printf("[INFO] Sent RESPONSE\n");
                }
            }
        } else if (n < 0) {
            printf("recvfrom error: %s\n", strerror(errno));
        }
    }
    close(sock);
    return NULL;
}

void send_command(struct mosquitto *mosq, const char *device_id, const char *command) {
    char topic[64];
    snprintf(topic, sizeof(topic), COMMAND_TOPIC, device_id);
    if (mosquitto_publish(mosq, NULL, topic, strlen(command), command, 0, false) == MOSQ_ERR_SUCCESS)
        printf("[INFO] Sent command [%s]: %s\n", topic, command);
    else
        fprintf(stderr, "[ERROR] Command send failed\n");
}

void *mqtt_loop_thread(void *arg) {
    struct mosquitto *mosq = (struct mosquitto *)arg;
    while (1) {
        if (mosquitto_loop(mosq, -1, 100) != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "[ERROR] Connection lost, attempting reconnect...\n");
            sleep(1);
            if (mosquitto_reconnect(mosq) == MOSQ_ERR_SUCCESS) {
                fprintf(stderr, "[INFO] Reconnected to broker\n");
                mosquitto_subscribe(mosq, NULL, SENSOR_TOPIC, 0);
            }
        }
    }
    return NULL;
}

int main() {
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("bridge-monitor", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create mosquitto client\n");
        return 1;
    }
    mosquitto_message_callback_set(mosq, on_message);
    if (mosquitto_connect(mosq, BROKER, PORT, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Could not connect to broker\n");
        return 1;
    }
    if (mosquitto_subscribe(mosq, NULL, SENSOR_TOPIC, 0) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "[ERROR] Subscription to %s failed\n", SENSOR_TOPIC);
    } else {
        printf("[INFO] Subscribed to topic: %s\n", SENSOR_TOPIC);
    }

    pthread_t ssdp_thread, notify_thread, mqtt_thread;
    pthread_create(&ssdp_thread, NULL, ssdp_loop, NULL);
    pthread_create(&notify_thread, NULL, notify_loop, NULL);
    pthread_create(&mqtt_thread, NULL, mqtt_loop_thread, mosq);

    char input[256];
    //const char *device_id = "sensorC";

  
    while (1) {
    check_inactive_devices();
    printf("Enter command (start/stop/set_period N/status/exit) for all devices: ");
    if (fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\n")] = 0;
        if (strcmp(input, "exit") == 0) break;
        else if (strcmp(input, "status") == 0) {
            for (int i = 0; i < device_count; i++) {
                printf("Device: %s, Last seen: %s", active_devices[i].device_id,
                       ctime(&active_devices[i].last_seen));
            }
        }
        else if (strcmp(input, "start") == 0 || strcmp(input, "stop") == 0 ||
                 strncmp(input, "set_period", 10) == 0) {
            if (device_count == 0) {
                printf("[ERROR] No active devices to send command to\n");
            } else {
                for (int i = 0; i < device_count; i++) {
                    send_command(mosq, active_devices[i].device_id, input);
                }
                sleep(2);
            }
        }
        else {
            printf("[ERROR] Invalid command. Use: start, stop, set_period N, status, or exit\n");
        }
    }
    sleep(2);
}

    pthread_join(ssdp_thread, NULL);
    pthread_join(notify_thread, NULL);
    pthread_join(mqtt_thread, NULL);

mosquitto_destroy(mosq);
mosquitto_lib_cleanup();
return 0;

}