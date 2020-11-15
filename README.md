# co2monpp
`co2mon.hpp` is a header only library for co2 monitoring (though it depends on [hidapi](https://github.com/libusb/hidapi) to support multi-platform).

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
    while(true){
        std::this_thread::sleep_for(30s);
        auto&& [temp,co2] = dev.ReadData();
        if (temp) std::cout << "TMP: " << temp.value().value << std::endl;
        if (co2) std::cout << "CO2: " << co2.value().value << std::endl;
    }
}
```

## Dependency
- [hidapi](https://github.com/libusb/hidapi)

## How to build logger
create `configs.json` in `./logger`.
``` json
{
    "channel_id": 12345,
    "write_key": "YOUR_KEY",
    "wait_time_seconds": 30
} 
```
`channel_id` and `write_key` can be obtained from [Ambient](https://ambidata.io/).

``` sh
cmake -B build -S .
cmake --build build
```

### release build (for gcc or clang)
``` sh
cmake -DCMAKE_BUILD_TYPE=Release -B build -S . -GNinja
```
### release build (for MSVC)
``` sh
cmake --build build --config Release
```
## Run logger
Run `co2logger.exe`.
This program sends sensor data every 30 seconds to [Ambient](https://ambidata.io/).
