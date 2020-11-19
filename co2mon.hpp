#pragma once

#include <memory>
#include <stdexcept>
#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <cstdio>
#include <vector>
#include <numeric>
#include <iostream>
#include <thread>
#include <mutex>

#include <hidapi.h>

namespace co2meter
{
	//http://pyopyopyo.hatenablog.com/entry/2019/02/08/102456
	template <typename... Args>
	std::string format(const std::string &fmt, Args... args)
	{
		size_t len = std::snprintf(nullptr, 0, fmt.c_str(), args...);
		std::vector<char> buf(len + 1);
		std::snprintf(&buf[0], len + 1, fmt.c_str(), args...);
		return std::string(&buf[0], &buf[0] + len);
	}

	template <typename T,
			  typename Titer = decltype(std::begin(std::declval<T>())),
			  typename = decltype(std::end(std::declval<T>()))>
	std::string join_numeric(T &&container, const std::string &delim = " ",
							 decltype(std::ios_base::basefield) fmt = std::ios::oct)
	{
		std::ostringstream s;
		s.setf(fmt, std::ios::basefield);
		for (Titer iter = std::begin(container); iter != std::end(container); iter++)
		{
			if (iter != std::begin(container))
			{
				s << delim;
			}
			s << +(*iter);
		}
		return s.str();
	}

	template <typename T>
	struct MutexData
	{
		MutexData() : data(T()), mtx(std::mutex()){};
		T data;
		std::mutex mtx;
	};

	template <typename T>
	struct TimeData
	{
	public:
		TimeData(T &&val, std::chrono::system_clock::time_point &&t) : value(val), time(t){};
		T value;
		std::chrono::system_clock::time_point time;
	};

	constexpr static bool DEBUG = false;

	class Co2meter
	{
	public:
		using DataFormat = std::array<unsigned char, 8>;

		Co2meter() : dev(std::unique_ptr<hid_device, decltype(&hid_close)>(nullptr, hid_close)){};

		void Open()
		{
			dev = std::unique_ptr<hid_device, decltype(&hid_close)>(hid_open(vendor_id, product_id, NULL), [](hid_device* dev) {hid_close(dev); hid_exit(); });
			if (!dev)
			{
				throw std::runtime_error(
					format("cannot open the co2 device: vendor_id: %x, product_id: %x\n", vendor_id, product_id));
			}
			SendKey();
		}

		void ReadData(const int max_requests = 50)
		{
			bool got_temp = false;
			bool got_co2 = false;
			for (size_t i = 0; i < max_requests; i++)
			{
				switch (ReadDataRaw())
				{
				case CODE_TEMP:
					got_temp = true;
					break;
				case CODE_CO2:
					got_co2 = true;
					break;
				}
				if ((got_temp && got_co2))
					break;
			}
		}

		//read sensor data in a new thread
		void StartMonitoring(std::chrono::duration<int> observaton_cycle)
		{
			if (thread_monitoring)
				return;
			thread_monitoring = std::make_unique<std::thread>(std::thread([this, observaton_cycle]() {
				while (true)
				{
					{
						std::lock_guard<std::mutex> lock(thread_stop_signal.mtx);
						if (thread_stop_signal.data)
							return;
					}
					ReadData();
					std::this_thread::sleep_for(observaton_cycle);
				}
			}));
		}

		// send internal stop signal to the monitoring thread, and wait to stop it
		void StopMonitoring()
		{
			if (!thread_monitoring)
				return;

			{
				std::lock_guard<std::mutex> lock(thread_stop_signal.mtx);
				thread_stop_signal.data = true;
			}
			thread_monitoring->join();
			thread_stop_signal.data = false;
			thread_monitoring.release();
		}

		std::optional<TimeData<int>> GetCo2()
		{
			std::lock_guard<std::mutex> lock(co2.mtx);
			return co2.data;
		}
		std::optional<TimeData<double>> GetTemp()
		{
			std::lock_guard<std::mutex> lock(temperature.mtx);
			return temperature.data;
		}

	private:
		DataFormat Decrypt(const DataFormat &data) const
		{
			constexpr DataFormat cstate = {0x48, 0x74, 0x65, 0x6D, 0x70, 0x39, 0x39, 0x65};
			constexpr DataFormat shuffle = {2, 4, 0, 7, 1, 6, 5, 3};
			DataFormat data_xor, results;

			for (size_t i = 0; i < 8; i++)
			{
				auto idx = shuffle[i];
				data_xor[idx] = data[i] ^ key[idx];
			}
			for (size_t i = 0; i < 8; i++)
			{
				auto ctmp = (cstate[i] >> 4) | (cstate[i] << 4);
				results[i] = ((data_xor[i] >> 3) | (data_xor[(i - 1 + 8) % 8] << 5)) - ctmp;
			}
			return results;
		}

		void SendKey() const
		{
			std::array<unsigned char, 9> key_with_report_id = {0};
			std::copy(key.begin(), key.end(), key_with_report_id.begin() + 1);
			int res = hid_send_feature_report(dev.get(), key_with_report_id.data(), key_with_report_id.size());
			if (res != key_with_report_id.size())
			{
				throw std::runtime_error(format("Unable to send a key: response: %d, send_size: %d",
												res, key_with_report_id.size()));
			}
		}

		unsigned char ReadDataRaw()
		{
			DataFormat data;
			int res = hid_read_timeout(dev.get(), data.data(), data.size(), read_timeout_ms);
			auto decrypted = Decrypt(data);
			auto checksum = decrypted[3];
			auto sum = std::reduce(decrypted.begin(), decrypted.begin() + 3);
			if (decrypted[4] != 0x0d)
			{
				throw std::runtime_error(format("Fourth byte does not equal: 0x0d: %x, data: %s\n",
												decrypted[4], join_numeric(decrypted, " ", std::ios::hex).c_str()));
			}
			else if (checksum != sum)
			{
				throw std::runtime_error(format("Checksum error: expect: %x, sum: %x, data: %s\n",
												checksum, sum, join_numeric(decrypted, " ", std::ios::hex).c_str()));
			}
			if constexpr (DEBUG)
			{
				std::cout << "raw : " << join_numeric(data, " ", std::ios::hex) << std::endl;
				std::cout << "dec : " << join_numeric(decrypted, " ", std::ios::hex) << std::endl;
			}

			auto code = decrypted[0];
			int value = decrypted[1] << 8 | decrypted[2];
			if (code == CODE_TEMP)
			{
				// Temperature
				std::lock_guard<std::mutex> lock(temperature.mtx);
				temperature.data = TimeData((double)value * 0.0625 - 273.15, std::chrono::system_clock::now());
				if constexpr (DEBUG)
				{
					std::cout << "MONITOR_THREAD: TMP: " << temperature.data.value().value << std::endl;
					;
				}
			}
			else if (code == CODE_CO2)
			{
				// CO2
				std::lock_guard<std::mutex> lock(co2.mtx);
				co2.data = TimeData(std::move(value), std::chrono::system_clock::now());
				if constexpr (DEBUG)
				{
					std::cout << "MONITOR_THREAD: CO2: " << value << std::endl;
				}
			}
			return code;
		}

		std::unique_ptr<hid_device, decltype(&hid_close)> dev;
		std::unique_ptr<std::thread> thread_monitoring;
		MutexData<std::optional<TimeData<int>>> co2;
		MutexData<std::optional<TimeData<double>>> temperature;
		MutexData<bool> thread_stop_signal;
		constexpr static int wchar_max_str = 255;
		constexpr static unsigned short vendor_id = 0x4d9;
		constexpr static unsigned short product_id = 0xa052;
		constexpr static int read_timeout_ms = 5000;
		constexpr static std::array<unsigned char, 8> key = {0xc4, 0xc6, 0xc0, 0x92, 0x40, 0x23, 0xdc, 0x96};
		constexpr static unsigned char CODE_CO2 = 0x50;
		constexpr static unsigned char CODE_TEMP = 0x42;
	};
} // namespace co2meter