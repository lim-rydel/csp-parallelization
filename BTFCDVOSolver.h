#ifndef BTFCDVOSOLVER_H
#define BTFCDVOSOLVER_H

#include "Solver.h"
#include <stack>
#include <queue>
#include <mutex>
#include <vector>
#include <cstdint>

struct DVOSearchState
{
    Solution board;
    std::vector<uint64_t> domains; // domains[i] = bitmask of available columns for row i

    DVOSearchState(const Solution &b, const std::vector<uint64_t> &d) : board(b), domains(d) {}
};

class BTFCDVOSolver : public Solver
{
private:
    int n;
    Solution initialState;
    std::vector<Solution> solutions;
    std::chrono::high_resolution_clock::time_point firstSolutionTime;
    bool foundFirst;
    int maxDepth;
    std::queue<Solution> *workQueue;
    std::mutex *queueMutex;

    // attackMask[r1][r2][col] = columns attacked in r2 if r1 has queen at col
    std::vector<std::vector<std::vector<uint64_t>>> attackMask;

    void precomputeAttackMasks();
    inline int popcount(uint64_t x) const;
    std::vector<uint64_t> initializeDomains(const Solution &board) const;
    int selectMRVRow(const Solution &board, const std::vector<uint64_t> &domains) const;
    int countAssigned(const Solution &board) const;

public:
    BTFCDVOSolver(int boardSize, const Solution &initial, int maxDepth = 0, std::queue<Solution> *wq = nullptr, std::mutex *qm = nullptr);
    void solve() override;
    const std::vector<Solution> &getSolutions() const override;
    std::chrono::high_resolution_clock::time_point getFirstSolutionTime() const override;
};

#endif