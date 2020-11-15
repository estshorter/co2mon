# co2monpp
`co2mon.hpp`

## How to use
``` cpp
#include <iostream>
#include <stdexcept>

#include "co2meterpp.h"

int main(int argc, char* argv[]) {
    using namespace co2meter;
    using namespace std::literals::chrono_literals;

    Co2meter dev;
	try {
		dev->Open();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
    // non-blocking monitoring
    dev.StartMonitoring(2s)); // pass monitoring cycle
    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(30s));
		auto temp = dev.GetTemp();
		auto co2 = dev.GetCo2();
		if (temp) std::cout << "TMP: " << temp.value().value << std::endl;
		if (co2) std::cout << "CO2: " << co2.value().value << std::endl;
    }
    dev.StopMonitoring();
}
```

## How to build
``` sh
cmake -B build -S .
cmake --build build
```

### release build (for gcc or clang)
``` sh
cmake -DCMAKE_BUILD_TYPE=Release -B build -S .
```
### release build (for MSVC)
``` sh
cmake --build build --config Release
```
