#define NOMINMAX

#include <pcapplusplus/Packet.h>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std;

atomic<bool> gRunning{ true };

void signalHandler(int)
{
    gRunning = false;
}

namespace metrics
{
    // Thread-safe metric aggregator that tracks total captured traffic.
    class HttpMetrics
    {
    public:
        void recordRequest()
        {
            total.fetch_add(1, memory_order_relaxed);
        }

        uint64_t getTotal() const { return total.load(); }

    private:
        atomic<uint64_t> total{ 0 };
    };
}

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

namespace capture
{
    // RAII wrapper for PcapLiveDevice to configure and manage the packet capture lifecycle.
    class Engine
    {
    public:
        explicit Engine(pcpp::PcapLiveDevice* dev)
            : dev(dev)
        {
            if (!dev || !dev->open())
                throw runtime_error("Could not open device.");
        }

        ~Engine()
        {
            stop();
            if (dev)
                dev->close();
        }

        void start(metrics::HttpMetrics& m)
        {
            pcpp::PortFilter filter(80, pcpp::SRC_OR_DST);
            dev->setFilter(filter);

            dev->startCapture(
                [](pcpp::RawPacket*, pcpp::PcapLiveDevice*, void* ctx)
                {
                    auto* metrics = static_cast<metrics::HttpMetrics*>(ctx);
                    metrics->recordRequest(); // Temporary basic counting
                },
                &m);
        }

        void stop()
        {
            if (dev)
                dev->stopCapture();
        }

    private:
        pcpp::PcapLiveDevice* dev;
    };
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
    signal(SIGINT, signalHandler);

    try
    {
        Args args = parseArgs(argc, argv);
        auto* dev = ui::selectInterface();

        if (!dev)
            return 0;

        metrics::HttpMetrics metrics;
        capture::Engine engine(dev);

        cout << "\nCapture started on: " << dev->getDesc() << "\n";
        cout << "Running for " << args.duration << " seconds\n";

        engine.start(metrics);

        auto start = chrono::steady_clock::now();
        while (gRunning &&
            chrono::steady_clock::now() - start <
            chrono::seconds(args.duration))
        {
            this_thread::sleep_for(chrono::milliseconds(250));
        }

        engine.stop();

        cout << "\nTotal packets matching filter: " << metrics.getTotal() << "\n";
    }
    catch (const exception& e)
    {
        cerr << "\nFatal error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}