#include <iostream>
#include <iomanip>
#include <thread>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

#include <memory>
#include <mutex>
#include <queue>

#include "BTSolver.h"
#include "BTFCSolver.h"
#include "BTFCDVOSolver.h"

#include "AC3Solver.h"
#include "AC3DVOSolver.h"

struct Config
{
    std::string solverType;
    int nThreads;
    int boardSize;
    bool printAllSolutions;
    bool printResultsToTxt;
    bool saveSolutionsToTxt;
    bool isParallel;
    int domainGranularity;
};

// spawn solver based on config
// maxDepth is used for filling out the domain at the start
std::unique_ptr<Solver> spawnSolver(const std::string &solverType, int boardSize, const Solution &initialState, int maxDepth = 0, std::queue<Solution> *workQueue = nullptr, std::mutex *queueMutex = nullptr)
{
    if (solverType == "BT")
    {
        return std::make_unique<BTSolver>(boardSize, initialState, maxDepth, workQueue, queueMutex);
    }
    else if (solverType == "BT-FC")
    {
        return std::make_unique<BTFCSolver>(boardSize, initialState, maxDepth, workQueue, queueMutex);
    }
    else if (solverType == "BT-FC-DVO")
    {
        return std::make_unique<BTFCDVOSolver>(boardSize, initialState, maxDepth, workQueue, queueMutex);
    }
    else if (solverType == "AC3")
    {
        return std::make_unique<AC3Solver>(boardSize, initialState, maxDepth, workQueue, queueMutex);
    }
    else if (solverType == "AC3-DVO")
    {
        return std::make_unique<AC3DVOSolver>(boardSize, initialState, maxDepth, workQueue, queueMutex);
    }

    std::cout << "Error while spawning solver! Are you sure you typed in a valid type?\n";
    return nullptr;
}

Config readConfig(const std::string &filename)
{
    Config config;
    config.domainGranularity = 1; // by default, only populate first variable

    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string key, value;

        if (std::getline(iss, key, ':'))
        {
            std::getline(iss, value);

            // clean
            value.erase(0, value.find_first_not_of(" \t"));

            if (key == "solverType")
                config.solverType = value;
            else if (key == "nThreads")
                config.nThreads = std::stoi(value);
            else if (key == "boardSize")
                config.boardSize = std::stoi(value);
            else if (key == "printAllSolutions")
                config.printAllSolutions = (value == "true");
            else if (key == "printResultsToTxt")
                config.printResultsToTxt = (value == "true");
            else if (key == "saveSolutionsToTxt")
                config.saveSolutionsToTxt = (value == "true");
            else if (key == "domainGranularity")
                config.domainGranularity = std::stoi(value);
        }
    }

    config.isParallel = (config.nThreads > 1);
    return config;
}

// https://stackoverflow.com/questions/12347371/stdput-time-formats
std::string getCurrentTimestamp()
{
    // when to use auto vs time t?
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d-%H-%M-%S");
    return oss.str();
}

void printSolution(const Solution &sol)
{
    for (int row = 0; row < sol.size(); row++)
    {
        for (int col = 0; col < sol.size(); col++)
        {
            std::cout << (sol[row] == col ? "Q " : ". ");
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void writeResultsToFile(const Config &config, const std::vector<Solution> &solutions, double timeToFirst, double timeToAll)
{
    std::string filename = config.solverType + "-" + getCurrentTimestamp() + ".txt";
    std::ofstream file(filename);

    file << "Solver Type: " << config.solverType << "\n";
    file << "Threads: " << config.nThreads << "\n";
    file << "Board Size: " << config.boardSize << "\n";
    file << "Domain Granularity: " << config.domainGranularity << "\n";
    file << "Time to First Solution: " << timeToFirst << " seconds\n";
    file << "Time to All Solutions: " << timeToAll << " seconds\n";
    file << "Number of Solutions: " << solutions.size() << "\n\n";

    if (config.saveSolutionsToTxt)
    {
        file << "All Solutions:\n";
        for (size_t i = 0; i < solutions.size(); i++)
        {
            for (int col : solutions[i])
            {
                // don't print visually, makes massive outputs. just do raw variables
                file << col << " ";
            }
            file << "\n";
        }
    }

    file.close();
    std::cout << "Results written to " << filename << "\n";
}

// pop from primary work queue + allocate solver + start solve + loop if work queue not empty
void workerThread(std::queue<Solution> *workQueue, std::mutex *queueMutex, const Config &config, std::vector<std::unique_ptr<Solver>> *solvers, std::mutex *solversMutex)
{
    while (true)
    {
        Solution initialState;

        // pop work from queue
        {
            std::lock_guard<std::mutex> lock(*queueMutex);
            if (workQueue->empty())
            {
                break; // wq empty
            }
            initialState = workQueue->front();
            workQueue->pop();
        }

        auto solver = spawnSolver(config.solverType, config.boardSize, initialState);
        solver->solve();

        // double check if locking is proper
        {
            std::lock_guard<std::mutex> lock(*solversMutex);
            solvers->push_back(std::move(solver));
        }
    }
}

int main()
{
    Config config = readConfig("config.txt");

    std::cout << "N-Queens Solver" << "\n";
    std::cout << "- Solver: " << config.solverType << "\n";
    std::cout << "- Board Size: " << config.boardSize << "\n";
    std::cout << "- Parallel: " << (config.isParallel ? "Yes" : "No") << "\n";
    // std::cout << "- Domain Granularity: " << config.domainGranularity << "\n";
    if (config.isParallel)
    {
        std::cout << "- Threads: " << config.nThreads << "\n";
        std::cout << "- Domain Granularity: " << config.domainGranularity << "\n";
    }
    std::cout << "\n";

    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<Solution> allSolutions;
    std::chrono::high_resolution_clock::time_point firstSolutionTime;

    // if threads > 1, make work queue, init a solver with depth = domainGrnularity to populate wq
    // then, init nThreads workThreads
    if (config.isParallel)
    {
        std::queue<Solution> workQueue;
        std::mutex queueMutex;

        Solution baseState(config.boardSize, -1);
        auto seedSolver = spawnSolver(config.solverType, config.boardSize, baseState,
                                      config.domainGranularity, &workQueue, &queueMutex);
        seedSolver->solve();

        std::cout << "Work queue populated with " << workQueue.size() << " initial states\n \n";

        std::vector<std::unique_ptr<Solver>> solvers;
        std::mutex solversMutex;
        std::vector<std::thread> threads;
        for (int i = 0; i < config.nThreads; i++)
        {
            threads.emplace_back(workerThread, &workQueue, &queueMutex, std::ref(config), &solvers, &solversMutex);
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        // compile solutions from all solvers
        bool foundFirst = false;
        for (auto &solver : solvers)
        {
            // std::vector<Solution> &solutions = solver->getSolutions();
            const std::vector<Solution> &solutions = solver->getSolutions();
            allSolutions.insert(allSolutions.end(), solutions.begin(), solutions.end());

            // yoink the fastest first sol from all solvers

            // you have to check if solutions empty, bc otherwise, it crashes if nStates < initial domains,
            // or the initial domain it gets ends up being a dead end
            // if (!foundFirst)
            if (!foundFirst && !solutions.empty())
            {
                firstSolutionTime = solver->getFirstSolutionTime();
                foundFirst = true;
            }
            else if (!solutions.empty())
            {
                if (solver->getFirstSolutionTime() < firstSolutionTime)
                    firstSolutionTime = solver->getFirstSolutionTime();
            }
        }
    }

    // if NOT PARALLEL, just run solver plainly, with seed domain of empty board
    else
    {
        Solution initialState(config.boardSize, -1);
        auto solver = spawnSolver(config.solverType, config.boardSize, initialState);
        solver->solve();

        allSolutions = solver->getSolutions();
        firstSolutionTime = solver->getFirstSolutionTime();
    }

    auto endTime = std::chrono::high_resolution_clock::now();

    // double timeToFirst = std::chrono::duration<double>(firstSolutionTime - startTime);
    double timeToFirst = std::chrono::duration<double>(firstSolutionTime - startTime).count();
    double timeToAll = std::chrono::duration<double>(endTime - startTime).count();

    // results
    std::cout << "Time to First Solution: " << timeToFirst << " seconds\n";
    std::cout << "Time to All Solutions: " << timeToAll << " seconds\n";
    std::cout << "Number of Solutions: " << allSolutions.size() << "\n\n";

    if (config.printAllSolutions)
    {
        std::cout << "All Solutions: \n\n";
        for (size_t i = 0; i < allSolutions.size(); i++)
        {
            std::cout << "Solution " << (i + 1) << ":\n";
            printSolution(allSolutions[i]);
        }
    }

    if (config.printResultsToTxt)
    {
        writeResultsToFile(config, allSolutions, timeToFirst, timeToAll);
    }

    return 0;
}