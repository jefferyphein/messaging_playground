#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#include "mosquitto.h"

int on_message(struct mosquitto * inst, void *, const struct mosquitto_message *msg) {
    std::string message(static_cast<char*>(msg->payload));
    std::cout << message << std::endl;
    return (message.compare("This is the last message.") == 0) ? 1 : 0;
}

int main(int argc, char **argv) {
    // Must initialize before using mosquitto.
    mosquitto_lib_init();

    // Display version numbers for fun.
    int major, minor, revision;
    mosquitto_lib_version(&major, &minor, &revision);
    std::cout << "mosquitto v" << major << "." << minor << "." << revision << std::endl;

    // Create a new mosquitto instance.
    mosquitto *inst = mosquitto_new(NULL, true, NULL);

    // Create the mosquitto subscribe-message callback loop.
    int err = mosquitto_subscribe_callback(
        on_message,
        NULL,
        "topic",
        0,
        "localhost",
        1883,
        NULL,
        0,
        true,
        NULL,
        NULL,
        NULL,
        NULL
    );

    // Clean mosquitto up before we go go.
    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}
