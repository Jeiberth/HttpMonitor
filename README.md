\# HTTP Activity Monitor



A command-line tool that captures network traffic and monitors \*\*plaintext HTTP requests\*\* on a machine for a specified duration.

The program analyzes captured packets and produces a summary including the \*\*total number of HTTP requests\*\* and a \*\*histogram of the most requested hosts\*\*.



This implementation uses \*\*PcapPlusPlus\*\* for packet capture and HTTP parsing.



---



\# Features



\* Real-time network packet capture

\* Detection of HTTP requests

\* Aggregation of host statistics

\* Histogram of the \*\*Top 10 requested hosts\*\*

\* Optional request logging to a file

\* Graceful shutdown via `Ctrl+C`



---



\# Command Syntax



```

HttpMonitor.exe \[/L:<log\_file>] <duration\_in\_seconds>

```



\### Examples



Run monitoring for 60 seconds:



```

HttpMonitor.exe 60

```



Run monitoring and log detected hosts to a file:



```

HttpMonitor.exe /L:requests.log 60

```



---



\# Example Output



```

==================================================

HTTP MONITOR SUMMARY

==================================================

Total HTTP Requests: 148



HOST                      COUNT

---------------------------------------------

google.com                45    \*\*\*\*\*\*\*\*\*\*\*\*\*

github.com                32    \*\*\*\*\*\*\*\*

stackoverflow.com         21    \*\*\*\*\*

example.com               10    \*\*

==================================================

```



---



\# Architecture



The program is organized into three main components:



| Component | Responsibility                                        |

| --------- | ----------------------------------------------------- |

| `capture` | Handles packet capture and HTTP packet detection      |

| `metrics` | Aggregates request statistics and host counts         |

| `ui`      | Handles user interaction and summary report rendering |



This separation keeps the packet-processing path lightweight while isolating reporting logic.



---



\# Algorithm Complexity



Let \*\*U\*\* be the number of unique hosts observed.



| Operation         | Complexity                                           |

| ----------------- | ---------------------------------------------------- |

| Tracking requests | \*\*O(1)\*\* average using `std::unordered\_map`          |

| Generating report | \*\*O(U log U)\*\* due to sorting hosts by request count |



---



\# Dependencies



The project relies on the following external libraries:



\* \*\*PcapPlusPlus\*\*

\* \*\*Npcap\*\*



Npcap provides the packet capture driver, while PcapPlusPlus provides the C++ API for packet processing.



---



\# Building (Windows / Visual Studio)



1\. Install \*\*Npcap\*\* in \*WinPcap compatibility mode\*.

2\. Download the \*\*Npcap SDK\*\*.

3\. Download the \*\*PcapPlusPlus precompiled binaries\*\*.

4\. Create a new \*\*Empty C++ Project\*\* in Visual Studio.

5\. Add `HttpMonitor.cpp` to the project.

6\. Configure the following paths.



\### Include Directories



```

C:\\Development\\PcapPlusPlus\\include\\pcapplusplus

C:\\Development\\npcap-sdk\\Include

```



\### Library Directories



```

C:\\Development\\npcap-sdk\\Lib\\x64

C:\\Development\\PcapPlusPlus\\lib

```



\### Linker Dependencies



```

Pcap++.lib

Packet++.lib

Common++.lib

wpcap.lib

Ws2\_32.lib

iphlpapi.lib

```



Build the project using \*\*x64 configuration\*\*.



---



\# Limitations



\* Only \*\*plaintext HTTP (port 80)\*\* traffic is analyzed.

\* \*\*HTTPS (port 443)\*\* traffic cannot be inspected because the payload is encrypted.

\* Traffic on non-standard HTTP ports (e.g., 8080) is not currently included in the capture filter.

\* Statistics are generated only for the current capture session.



---



\# Notes



Packet capture typically requires \*\*administrator privileges\*\*, so the program should be run from an elevated terminal.



