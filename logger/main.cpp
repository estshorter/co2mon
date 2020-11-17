#include <csignal>
#include <chrono>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <fstream>

#include <httplib.h>
#include <json.hpp>

#include "co2mon.hpp"

using namespace co2meter;

static std::unique_ptr<Co2meter> dev = std::make_unique<Co2meter>();

void signal_handler(int sig_num)
{
	switch (sig_num) {
	case SIGINT:
	case SIGTERM:
		dev->StopMonitoring();
		exit(0);
		break;
	}
}


//std::string getTimeStamp(const std::chrono::system_clock::time_point time_chrono)
//{
//	auto now_c = std::chrono::system_clock::to_time_t(time_chrono);
//	std::stringstream ss;
//#ifdef _MSC_VER
//#pragma warning(push)
//#pragma warning(disable : 4996)
//#endif
//	ss << std::put_time(localtime(&now_c), "%Y-%m-%d %H%M%S");
//#ifdef _MSC_VER
//#pragma warning(pop)
//#endif
//	return ss.str();
//}

int main(int argc, char *argv[])
{
	std::string path_json = "configs.json";
	if (argc == 2)
	{
		path_json = argv[1];
	}

	// read a config json file
	nlohmann::json config;
	{
		std::ifstream config_ifs(path_json);
		if (!config_ifs)
		{
			std::cerr << format("failed to open a config file: %s", path_json.c_str()) << std::endl;
			return 1;
		}
		config_ifs >> config;
	}

	signal(SIGINT, signal_handler);
	try
	{
		dev->Open();
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	//std::cout << config.dump() << std::endl;
	if (config["channel_id"] == nullptr || config["write_key"] == nullptr 
		|| config["monitoring_cycle_seconds"] == nullptr 
		|| config["reporting_cycle_seconds"] == nullptr)
	{
		std::cerr << "contents of the json file is wrong" << std::endl;
		return 1;
	}

	int channel_id = config["channel_id"];
	httplib::Client cli("ambidata.io", 80);
	cli.set_connection_timeout(5, 0);
	cli.set_read_timeout(5, 0);
	cli.set_write_timeout(5, 0);

	std::string post_path = format("/api/v2/channels/%d/dataarray", int(config["channel_id"]));
	nlohmann::json send_data;
	send_data["writeKey"] = config["write_key"];
	send_data["data"] = {{{"d1", "20.0"}, {"d2", 1000}}};
	dev->StartMonitoring(std::chrono::seconds(config["monitoring_cycle_seconds"]));

	int err_cnt = 0;
	constexpr int err_threshold = 10;
	for (int i = 0; i <= 1000; i++)
	{
		std::this_thread::sleep_for(std::chrono::seconds(config["reporting_cycle_seconds"]));
		auto temp = dev->GetTemp();
		auto co2 = dev->GetCo2();

		// if (temp)
		// 	std::cout << "TMP: " << temp.value().value << std::endl;
		// if (co2)
		// 	std::cout << "CO2: " << co2.value().value << std::endl;

		if (temp && co2)
		{
			send_data["data"][0]["d1"] = format("%.2f", temp.value().value);
			send_data["data"][0]["d2"] = co2.value().value;
			//std::cout << send_data.dump() << std::endl;
			if (auto res = cli.Post(post_path.c_str(), send_data.dump(), "application/json"))
			{
				if (res->status != 200)
				{
					std::cerr << "failed to send data: "
							  << "response code: " << res->status << std::endl;
					;
					err_cnt++;
				}
				else {
					err_cnt = 0;
				}
			}
			else
			{
				std::cerr << "error occurred while sending data: error code: " << res.error() << std::endl;
				err_cnt++;
			}
			// network error detected
			if (err_cnt >= err_threshold) {
				dev->StopMonitoring();
				return 1;
			}
		}
	}
	dev->StopMonitoring();
	return 0;
}