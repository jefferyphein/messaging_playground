#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#include "mosquitto.h"

int main(int argc, char **argv) {
    mosquitto_lib_init();

    int major, minor, revision;
    mosquitto_lib_version(&major, &minor, &revision);
    std::cout << "mosquitto v" << major << "." << minor << "." << revision << std::endl;

    mosquitto *inst = mosquitto_new(NULL, true, NULL);
    int err = mosquitto_connect(inst, "localhost", 1883, 0);
    if (err != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to broker" << std::endl;
        return EXIT_FAILURE;
    }

    std::string msg = "Hello, World!";
    for (int n=0; n<10; n++) {
        int pub = mosquitto_publish(inst, NULL, "topic", msg.length(), msg.c_str(), 0, false);
        if (pub != MOSQ_ERR_SUCCESS) {
            std::cerr << "Publish message failed." << std::endl;
            return EXIT_FAILURE;
        }
        sleep(1);
    }

    msg = "This is the last message.";
    int pub = mosquitto_publish(inst, NULL, "topic", msg.length(), msg.c_str(), 0, false);
    if (pub != MOSQ_ERR_SUCCESS) {
        std::cerr << "Publish message failed." << std::endl;
        return EXIT_FAILURE;
    }

    err = mosquitto_disconnect(inst);
    if (err == MOSQ_ERR_SUCCESS) {
        std::cout << "Successfully disconnected from broker." << std::endl;
    }

    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}
