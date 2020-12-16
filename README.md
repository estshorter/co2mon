# co2mon
`co2mon.hpp` is a header only C++ library for co2 monitoring (though it depends on [hidapi](https://github.com/libusb/hidapi) to support multi-platform).

## Supported sensor
- [CO2Mini](https://www.co2meter.com/products/co2mini-co2-indoor-air-quality-monitor) compatible sensors

## How to use
``` cpp
#include <chrono>
#include <iostream>
#include <stdexcept>

#include "co2mon.hpp"

int main(int argc, char* argv[]) {
    using namespace std::literals::chrono_literals;

    co2meter::Co2meter dev;
    try {
        dev.Open();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    // non-blocking monitoring
    dev.StartMonitoring(4s); // pass monitoring cycle
    while(true){
        std::this_thread::sleep_for(30s);
        auto temp = dev.GetTemp();
        auto co2 = dev.GetCo2();
        if (temp) std::cout << "TMP: " << temp.value().value << std::endl;
        if (co2)  std::cout << "CO2: " <<  co2.value().value << std::endl;
    }
    dev.StopMonitoring();
    co2meter::Co2meter::Exit();
}
```

## Dependency
- [hidapi](https://github.com/libusb/hidapi)

## Example
[co2logger](https://github.com/estshorter/co2logger)
