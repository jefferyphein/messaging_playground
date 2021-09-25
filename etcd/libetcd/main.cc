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
            std::cout << "get key invalid" << std::endl;
        }
    }
    else {
        std::cout << response.error_message() << std::endl;
    }
}

void test_set(::libetcd::Client& client) {
    ::libetcd::Future fut = client.set("/driver/state/1", "321", true);
    ::libetcd::Response response = fut.get();
    if (response.ok()) {
        if (response.prev_value().is_valid()) {
            std::cout << response.prev_value().string() << std::endl;
        }
        else {
            std::cout << "previous key invalid" << std::endl;
        }
    }
    else {
        std::cout << response.error_message() << std::endl;
    }
}

void test_del(::libetcd::Client& client) {
    ::libetcd::Future fut = client.del("/driver/state/1", true);
    ::libetcd::Response response = fut.get();
    if (response.ok()) {
        if (response.prev_value().is_valid()) {
            std::cout << response.prev_value().key() << " xxx " << response.prev_value().string() << std::endl;
        }
        else {
            std::cout << "del key invalid" << std::endl;
        }
    }
}

void test_del_range(::libetcd::Client& client) {
    ::libetcd::Future fut = client.del_range("/driver/state/0", "/driver/state/2");
    ::libetcd::Response response = fut.get();
    if (response.ok()) {
        for (const auto& value : response.prev_values()) {
            if (value.is_valid()) {
                std::cout << value.key() << " xxx " << value.string() << std::endl;
            }
            else {
                std::cout << "del_range key invalid" << std::endl;
            }
        }
    }
}

int main(int argc, char **argv) {
    ::libetcd::Client client("localhost:2379");

    test_get(client);
    test_set(client);
    test_del(client);
    test_del_range(client);

    return EXIT_SUCCESS;
}
