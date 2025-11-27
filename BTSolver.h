#ifndef BTSOLVER_H
#define BTSOLVER_H

#include "Solver.h"
#include <stack>
#include <queue>
#include <mutex>

struct SearchState
{
    Solution board;
    int row;

    SearchState(const Solution &b, int r) : board(b), row(r) {}
};

class BTSolver : public Solver
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

    bool isSafe(const Solution &board, int row, int col);

public:
    BTSolver(int boardSize, const Solution &initial, int maxDepth = 0, std::queue<Solution> *wq = nullptr, std::mutex *qm = nullptr);
    void solve() override;
    const std::vector<Solution> &getSolutions() const override;
    std::chrono::high_resolution_clock::time_point getFirstSolutionTime() const override;
};

#endif
