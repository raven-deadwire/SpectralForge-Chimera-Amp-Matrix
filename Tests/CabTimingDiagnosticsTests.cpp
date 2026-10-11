#include "CabTimingDiagnostics.h"
#include <iostream>
#include <sstream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
std::vector<std::string> columns(const std::string& line)
{
    std::istringstream input(line);
    std::vector<std::string> result;
    std::string value;
    while (std::getline(input, value, ',')) result.push_back(value);
    return result;
}
}

int main()
{
    try
    {
        std::vector<cabTiming::Sample> samples;
        for (size_t i = 0; i < cabTiming::measuredCallbacks; ++i)
        {
            const cabTiming::ThreadClock begin{double(i * 10000 + 1), i * 100000, true, 0};
            const cabTiming::ThreadClock end{begin.cpuUs + double(1600 - i), begin.cycles + 10000 - i, true, 0};
            samples.push_back({i, double(i + 1), begin, end});
        }
        cabTiming::validate(samples);
        const auto all = cabTiming::wallPairs(samples, 0, 1600);
        require(all.p99 == 1584 && all.maximum == 1599, "unchanged wall order statistics");
        require(samples[all.p99].cpuUs() == 16 && samples[all.p99].cycles() == 8416,
                "wall p99 must use the same callback's CPU/cycles, never independent quantiles");
        const auto worker = cabTiming::wallPairs(samples, 0, 1200);
        const auto forced = cabTiming::wallPairs(samples, 1200, 1600);
        require(worker.p99 == 1188 && forced.p99 == 1596, "phase quantiles preserve global callback indices");
        const cabTiming::Configuration config{"expanded-v2", "integration", 64000, 64, 1};
        std::ostringstream csv;
        cabTiming::writeCsv(csv, config, samples);
        std::istringstream rows(csv.str());
        std::string line;
        std::getline(rows, line);
        require(columns(line).size() == 23, "CSV schema");
        size_t count = 0, workerCount = 0, forcedCount = 0, automationCount = 0, misses = 0;
        while (std::getline(rows, line))
        {
            const auto row = columns(line);
            require(row.size() == 23 && std::stoull(row[6]) == count, "CSV preserves each index exactly once");
            require(std::stod(row[10]) == double(count + 1), "CSV preserves unsorted wall samples");
            require(std::stoull(row[15]) == 10000 - count && row[16] == "1", "CSV preserves paired cycles");
            require(std::stoull(row[8]) == (count < 1200 ? count : count - 1200), "CSV phase index");
            workerCount += row[7] == "worker_and_automation";
            forcedCount += row[7] == "forced_six_slot_publication";
            automationCount += row[9] == "1";
            misses += row[12] == "1";
            ++count;
        }
        require(count == 1600 && workerCount == 1200 && forcedCount == 400 && automationCount == 80,
                "CSV must include all original callbacks and automation phases");
        require(misses == 600, "deadline miss uses unchanged strictly-greater budget comparison");
        samples[1584].end.cycles = samples[1584].begin.cycles - 1;
        require(!samples[1584].cyclesValid() && samples[1584].cycles() == 0, "reject wrapped/backwards cycles");
        samples[1584].begin.cyclesValid = false;
        samples[1584].end.cycles = samples[1584].begin.cycles + 10;
        require(!samples[1584].cyclesValid(), "failed cycle read must not look like a valid zero delta");
        samples[1584].begin.cpuUs = -1;
        require(!samples[1584].cpuValid() && samples[1584].cpuUs() == -1, "unsupported CPU time stays unavailable");
        samples[20].index = 21;
        bool rejected = false;
        try { cabTiming::validate(samples); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "duplicate callback index rejected");
        samples.pop_back();
        rejected = false;
        try { cabTiming::validate(samples); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "missing callback rejected");
        std::cout << "PASS paired CAB timing evidence: 1600 samples, unchanged quantiles, CSV phases and clock validity\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
