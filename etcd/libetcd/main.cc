#include <cstdio>
#include <cstdlib>
#include "libetcd.h"

int main(int argc, char **argv) {
    ::libetcd::Client client("localhost:2379");
    ::libetcd::Future fut = client.get("/driver/state/0", "/driver/state/3");
    ::libetcd::Response response = fut.get();
    if (response.ok()) {
        std::cout << response.size() << std::endl;
        if (response.size() == 1) {
            ::libetcd::Value& value = response.value();
            std::cout << value.key() << " -> " << value.value() << std::endl;
        }
        else {
            for (::libetcd::Value& value : response.values()) {
                std::cout << value.key() << " -> " << value.value() << std::endl;
            }
        }
    }
    else {
        std::cout << "not ok" << std::endl;
    }

    return EXIT_SUCCESS;
}
