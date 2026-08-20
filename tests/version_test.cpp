#include <guardian/version.hpp>

#include <iostream>

int main() {
    if (guardian::version_major != 0 || guardian::version_minor != 1 ||
        guardian::version_patch != 0 || guardian::version != "0.1") {
        std::cerr << "Unexpected project version: " << guardian::version << '\n';
        return 1;
    }

    return 0;
}
