#include <cstdio>
#include <cstdlib>
#include "libetcd.h"

void test_get(::libetcd::Client& client) {
    ::libetcd::Future fut = client.get("/driver/state/0");
    ::libetcd::Response response = fut.get();
    if (response.ok()) {
        if (response.value().is_valid()) {
            std::cout << response.value().key() << " -> " << response.value().string() << std::endl;
        }
        else {
            std::cout << "invalid key" << std::endl;
        }
    }
    else {
        std::cout << response.error_message() << std::endl;
    }
}

void test_set(::libetcd::Client& client) {
    ::libetcd::Future fut = client.set("/driver/state/1", "321");
    ::libetcd::Response response = fut.get();
    if (response.ok()) {
        std::cout << response.prev_value().string() << std::endl;
    }
    else {
        std::cout << response.error_message() << std::endl;
    }
}

int main(int argc, char **argv) {
    ::libetcd::Client client("localhost:2379");

    test_get(client);
    test_set(client);

    return EXIT_SUCCESS;
}
