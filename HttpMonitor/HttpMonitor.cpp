#define NOMINMAX

#include <pcapplusplus/HttpLayer.h>
#include <pcapplusplus/Packet.h>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;

atomic<bool> gRunning{ true };

void signalHandler(int)
{
    gRunning = false;
}

namespace metrics
{
    struct Snapshot
    {
        uint64_t total;
        uint64_t dropped;
        vector<pair<string, int>> ranked;
    };

    // Thread-safe metric aggregator that tracks HTTP requests and drops.
    class HttpMetrics
    {
    public:
        void recordRequest(string_view host)
        {
            total.fetch_add(1, memory_order_relaxed);

            lock_guard lock(mutex);

            string key(host);

            auto it = hosts.find(key);
            if (it != hosts.end())
                it->second++;
            else
                hosts.emplace(move(key), 1);
        }

        void recordDrop()
        {
            dropped.fetch_add(1, memory_order_relaxed);
        }

        Snapshot snapshot()
        {
            lock_guard lock(mutex);

            vector<pair<string, int>> ranked(hosts.begin(), hosts.end());

            sort(ranked.begin(), ranked.end(),
                [](auto& a, auto& b)
                { return a.second > b.second; });

            return { total.load(), dropped.load(), move(ranked) };
        }

    private:
        atomic<uint64_t> total{ 0 };
        atomic<uint64_t> dropped{ 0 };

        unordered_map<string, int> hosts;
        mutex mutex;
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

    // Formats and prints a summarized report of captured HTTP metrics to standard output.
    void renderReport(const metrics::Snapshot& snap)
    {
        cout << "\n\n"
            << string(50, '=') << "\n";
        cout << "HTTP MONITOR SUMMARY\n";
        cout << string(50, '=') << "\n";

        cout << "Total HTTP Requests: " << snap.total << "\n";

        if (snap.dropped)
            cout << "Parse Errors: " << snap.dropped << "\n";

        cout << "\n"
            << left << setw(30) << "HOST"
            << "COUNT\n";

        cout << string(45, '-') << "\n";

        size_t limit = min<size_t>(10, snap.ranked.size());

        for (size_t i = 0; i < limit; ++i)
        {
            const auto& [host, count] = snap.ranked[i];

            cout << left << setw(25) << truncate(host, 23)
                << setw(6) << count
                << string(count, '*')
                << "\n";
        }

        cout << string(50, '=') << "\n";
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
                [](pcpp::RawPacket* pkt, pcpp::PcapLiveDevice*, void* ctx)
                {
                    auto* metrics = static_cast<metrics::HttpMetrics*>(ctx);

                    pcpp::Packet parsed(pkt);

                    auto* http =
                        parsed.getLayerOfType<pcpp::HttpRequestLayer>();

                    if (!http)
                        return;

                    auto* host =
                        http->getFieldByName(PCPP_HTTP_HOST_FIELD);

                    if (host)
                        metrics->recordRequest(host->getFieldValue());
                    else
                        metrics->recordRequest("unknown");
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

        cout << "\nCapture started on: "
            << dev->getDesc() << "\n";

        cout << "Running for "
            << args.duration
            << " seconds\n";

        engine.start(metrics);

        auto start = chrono::steady_clock::now();

        while (gRunning &&
            chrono::steady_clock::now() - start <
            chrono::seconds(args.duration))
        {
            this_thread::sleep_for(chrono::milliseconds(250));
        }

        engine.stop();

        ui::renderReport(metrics.snapshot());
    }
    catch (const exception& e)
    {
        cerr << "\nFatal error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}