#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <condition_variable>
#include <mutex>

#include "mosquitto.h"

struct user_data_t {
    std::mutex mtx;
    std::condition_variable cv;
};

void on_connect(struct mosquitto *inst, void *obj, int rc) {
    std::cout << "Connected to broker" << std::endl;
    int err = mosquitto_subscribe(inst, NULL, "topic", 0);
    if (err != MOSQ_ERR_SUCCESS) {
        std::cerr << "Unable to subscribe to topic (" << mosquitto_strerror(err) << ")" << std::endl;
    }
}

void on_subscribe(struct mosquitto *inst, void *obj, int mid, int qos_count, const int *granted_qos) {
}

void on_message(struct mosquitto *inst, void *obj, const struct mosquitto_message *message) {
    std::string msg(static_cast<char*>(message->payload));
    std::cout << "[" << message->topic << "] message(" << message->mid << ") = " << msg << std::endl;
    if (msg == "shutdown") {
        mosquitto_disconnect(inst);
    }
}

void on_disconnect(struct mosquitto *inst, void *obj, int rc) {
    std::cout << "Disconnected from broker" << std::endl;
    user_data_t *user_data = static_cast<user_data_t*>(obj);
    user_data->cv.notify_all();
}

int main(int argc, char **argv) {
    std::string host = "localhost";
    int port = 1883;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = strtoll(argv[2], NULL, 10);
    }

    // Initialize.
    mosquitto_lib_init();
    user_data_t user_data;
    mosquitto *inst = mosquitto_new("id:sub", true, &user_data);
    if (inst == NULL) {
        std::cerr << "Unable to create instance" << std::endl;
        return EXIT_FAILURE;
    }

    // Register callbacks.
    mosquitto_connect_callback_set(inst, &on_connect);
    mosquitto_subscribe_callback_set(inst, &on_subscribe);
    mosquitto_message_callback_set(inst, &on_message);
    mosquitto_disconnect_callback_set(inst, &on_disconnect);

    // Create an async connection, and launch an I/O loop in a separate thread.
    int err = mosquitto_connect_async(inst, host.c_str(), port, 15);
    if (err != MOSQ_ERR_SUCCESS) {
        std::cerr << "Unable to connect to broker (" << mosquitto_strerror(err) << ")" << std::endl;
        return EXIT_FAILURE;
    }
    mosquitto_loop_start(inst);

    // Keep the main thread alive until notified by the disconnect callback.
    std::unique_lock<std::mutex> lck(user_data.mtx);
    user_data.cv.wait(lck);

    // Clean up.
    mosquitto_destroy(inst);
    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}
