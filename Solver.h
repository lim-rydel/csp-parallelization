#ifndef SOLVER_H
#define SOLVER_H

#include <vector>
#include <chrono>

// TODO: update all solvers to use solution instead of vector int
using Solution = std::vector<int>;

class Solver
{
public:
    virtual ~Solver() = default;
    virtual void solve() = 0;
    virtual const std::vector<Solution> &getSolutions() const = 0;
    virtual std::chrono::high_resolution_clock::time_point getFirstSolutionTime() const = 0;
};

#endif