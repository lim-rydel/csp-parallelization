#ifndef AC3DVOSOLVER_H
#define AC3DVOSOLVER_H

#include "Solver.h"
#include <stack>
#include <queue>
#include <mutex>
#include <vector>
#include <cstdint>

struct AC3DVOSearchState
{
    Solution board;
    std::vector<uint64_t> domains; // domains[i] = bitmask of available columns for row i

    AC3DVOSearchState(const Solution &b, const std::vector<uint64_t> &d) : board(b), domains(d) {}
};

class AC3DVOSolver : public Solver
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
    std::vector<uint64_t> initializeDomains(const Solution &board) const;
    bool enforceArcConsistency(std::vector<uint64_t> &domains, const Solution &board) const;
    inline bool revise(int row1, int row2, std::vector<uint64_t> &domains, const Solution &board) const;
    inline int popcount(uint64_t x) const;
    int selectMRVRow(const Solution &board, const std::vector<uint64_t> &domains) const;
    int countAssigned(const Solution &board) const;

public:
    AC3DVOSolver(int boardSize, const Solution &initial, int maxDepth = 0, std::queue<Solution> *wq = nullptr, std::mutex *qm = nullptr);
    void solve() override;
    const std::vector<Solution> &getSolutions() const override;
    std::chrono::high_resolution_clock::time_point getFirstSolutionTime() const override;
};

#endif