#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "mosquitto.h"

static int SHUTDOWN = 0xdeadbeef;

struct user_data_t {
    std::mutex mtx;
    std::condition_variable cv;
};

void on_publish(struct mosquitto *inst, void *obj, int mid) {
    if (mid == SHUTDOWN) {
        mosquitto_disconnect(inst);
    }
}

void on_connect(struct mosquitto *inst, void *obj, int rc) {
    std::cout << "Connected to broker" << std::endl;
    for (int n=0; n<5; n++) {
        std::string message("Message #" + std::to_string(n));
        int err = mosquitto_publish(inst, NULL, "topic", message.length(), message.c_str(), 2, false);
        if (err != MOSQ_ERR_SUCCESS) {
            std::cerr << "Unable to publish message (" << mosquitto_strerror(err) << ")" << std::endl;
        }

        std::cout << "Sent message: " << message << std::endl;
    }

    int err = mosquitto_publish(inst, &SHUTDOWN, "topic", 8, "shutdown", 2, false);
    if (err != MOSQ_ERR_SUCCESS) {
        std::cerr << "Unable to publish shutdown message (" << mosquitto_strerror(err) << ")" << std::endl;
    }
}

void on_disconnect(struct mosquitto *inst, void *obj, int rc) {
    std::cout << "Disconnected from broker" << std::endl;
    user_data_t *user_data = static_cast<user_data_t*>(obj);
    user_data->cv.notify_all();
}

int main(int argc, char **argv) {
    // Initialize.
    mosquitto_lib_init();
    user_data_t user_data;
    mosquitto *inst = mosquitto_new("id:pub", true, &user_data);
    if (inst == NULL) {
        std::cerr << "Unable to create instance" << std::endl;
        return EXIT_FAILURE;
    }

    // Register callbacks.
    mosquitto_connect_callback_set(inst, &on_connect);
    mosquitto_disconnect_callback_set(inst, &on_disconnect);
    mosquitto_publish_callback_set(inst, &on_publish);

    // Create an async connection, and launch an I/O loop in a separate thread.
    int err = mosquitto_connect_async(inst, "localhost", 1883, 15);
    if (err != MOSQ_ERR_SUCCESS) {
        std::cerr << "Unable to connect to broker (" << mosquitto_strerror(err) << ")" << std::endl;
        return EXIT_FAILURE;
    }
    mosquitto_loop_start(inst);

    // Keep the main thread alive until notified the last message was sent.
    std::unique_lock<std::mutex> lck(user_data.mtx);
    user_data.cv.wait(lck);

    // Clean up.
    mosquitto_destroy(inst);
    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}
