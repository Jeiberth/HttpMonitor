#define NOMINMAX

#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>

#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

namespace ui
{
    string truncate(string_view s, size_t len)
    {
        if (s.size() <= len)
            return string(s);

        return string(s.substr(0, len - 3)) + "...";
    }

    // Prompts the user to select an active network interface from available devices.
    pcpp::PcapLiveDevice* selectInterface()
    {
        const auto& devices =
            pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();

        if (devices.empty())
            throw runtime_error("No network interfaces found.");

        cout << "\n"
            << left << setw(6) << "ID"
            << setw(40) << "INTERFACE"
            << "IP\n";

        cout << string(70, '-') << "\n";

        for (size_t i = 0; i < devices.size(); ++i)
        {
            string ip = devices[i]->getIPv4Address().toString();

            cout << "[" << setw(3) << i << "] "
                << setw(38) << truncate(devices[i]->getDesc(), 36)
                << ip << "\n";
        }

        cout << string(70, '-') << "\n";

        while (true)
        {
            cout << "Select interface (q to quit): ";

            string input;
            cin >> input;

            if (input == "q")
                return nullptr;

            try
            {
                size_t idx = stoul(input);
                if (idx < devices.size())
                    return devices[idx];

                cout << "Invalid index\n";
            }
            catch (...)
            {
                cout << "Enter a number\n";
            }
        }
    }
}

struct Args
{
    int duration;
};

// Parses command-line arguments to extract capture duration.
Args parseArgs(int argc, char* argv[])
{
    if (argc < 2)
        throw runtime_error("Usage: httpmon <seconds>");

    Args args{};
    args.duration = stoi(argv[1]);

    if (args.duration <= 0)
        throw runtime_error("Duration must be positive.");

    return args;
}

int main(int argc, char* argv[])
{
    try
    {
        Args args = parseArgs(argc, argv);
        auto* dev = ui::selectInterface();

        if (!dev)
            return 0;

        cout << "\nSelected: " << dev->getDesc() << " for " << args.duration << " seconds.\n";
    }
    catch (const exception& e)
    {
        cerr << "\nFatal error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}