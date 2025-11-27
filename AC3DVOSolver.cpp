// AC3DVOSolver.cpp
#include "AC3DVOSolver.h"
#include <cmath>
#include <queue>

AC3DVOSolver::AC3DVOSolver(int boardSize, const Solution &initial, int maxDepth, std::queue<Solution> *wq, std::mutex *qm)
    : n(boardSize), initialState(initial), foundFirst(false), maxDepth(maxDepth), workQueue(wq), queueMutex(qm)
{
    precomputeAttackMasks();
}

void AC3DVOSolver::precomputeAttackMasks()
{
    // we're only computing attackMask once, then resuing it per check
    // attackMask[r1][r2][col] = bitmask of columns in r2 attacked by queen at (r1, col)
    attackMask.resize(n, std::vector<std::vector<uint64_t>>(n, std::vector<uint64_t>(n, 0)));

    for (int r1 = 0; r1 < n; r1++)
    {
        for (int r2 = 0; r2 < n; r2++)
        {
            if (r1 == r2)
                continue;

            for (int col = 0; col < n; col++)
            {
                uint64_t mask = 0;

                // column
                mask |= (1ULL << col);

                // diagonals
                int diagDist = abs(r2 - r1);
                if (col + diagDist < n)
                    mask |= (1ULL << (col + diagDist));
                if (col - diagDist >= 0)
                    mask |= (1ULL << (col - diagDist));

                attackMask[r1][r2][col] = mask;
            }
        }
    }
}

// we could have just used this https://www.geeksforgeeks.org/cpp/cpp-__builtin_popcount-function/
inline int AC3DVOSolver::popcount(uint64_t x) const
{
    int count = 0;
    while (x)
    {
        count++;
        x &= x - 1;
    }
    return count;
}

std::vector<uint64_t> AC3DVOSolver::initializeDomains(const Solution &board) const
{
    // start with all columns available
    std::vector<uint64_t> domains(n, (1ULL << n) - 1);

    // then remove attacked columns based on already-assigned queens
    for (int row = 0; row < n; row++)
    {
        if (board[row] != -1)
        {
            int col = board[row];
            domains[row] = 0; // set row as assigned

            // remove columns attacked by this queen using precomputed mask
            for (int otherRow = 0; otherRow < n; otherRow++)
            {
                if (otherRow != row)
                {
                    // available &= ~attackMask[prevRow][row][prevCol];
                    domains[otherRow] &= ~attackMask[row][otherRow][col];
                }
            }
        }
    }

    return domains;
}

// checks whether row1 is arc consistent with row2, nothing else
inline bool AC3DVOSolver::revise(int row1, int row2, std::vector<uint64_t> &domains, const Solution &board) const
{
    if (board[row1] != -1 || board[row2] != -1)
        return false;

    uint64_t domain1 = domains[row1];
    uint64_t domain2 = domains[row2];
    uint64_t toRemove = 0;

    // for each value in row1's domain, check if there's support in row2
    uint64_t testDomain = domain1;
    while (testDomain)
    {
        // get next set bit
        // int col1 = __builtin_ctzll(testDomain);
        int col1 = __builtin_ctzll(testDomain);

        // clear lowest set bit
        testDomain &= testDomain - 1;

        // check if row2 has ANY value compatible with (row1, col1)
        // can be found by doman2 minus values attacked by (row1, col1)
        uint64_t compatible = domain2 & ~attackMask[row1][row2][col1];

        if (compatible == 0)
        {
            // meant that there is no support for col1 in row1
            toRemove |= (1ULL << col1);
        }
    }

    // if there has been a removal, return true to indicate dirty, and enforce has to readd
    if (toRemove)
    {
        domains[row1] &= ~toRemove;
        return true;
    }

    // no revisions
    return false;
}

bool AC3DVOSolver::enforceArcConsistency(std::vector<uint64_t> &domains, const Solution &board) const
{
    std::queue<std::pair<int, int>> worklist;

    // build initial worklist of only unassigned rows
    for (int i = 0; i < n; i++)
    {
        if (board[i] != -1)
            continue;
        for (int j = 0; j < n; j++)
        {
            if (i != j && board[j] == -1)
            {
                worklist.push({i, j});
            }
        }
    }

    while (!worklist.empty())
    {
        // pop an arc
        auto [row1, row2] = worklist.front();
        worklist.pop();

        // if something changes, like a domain gets pruned
        if (revise(row1, row2, domains, board))
        {
            // if there is no remaining options for row1
            if (domains[row1] == 0)
            {
                return false; // domain wipeout, this timeline is a deadend
            }

            // add arcs represented by (k, row1) foreach unassigned k != row2
            // aka, re add all arcs pointing to row1 to reevaluate, except row2 since we just did that
            for (int k = 0; k < n; k++)
            {
                if (k != row1 && k != row2 && board[k] == -1)
                {
                    worklist.push({k, row1});
                }
            }
        }
    }

    // only complete when work queue is empty, and
    return true;
}

int AC3DVOSolver::selectMRVRow(const Solution &board, const std::vector<uint64_t> &domains) const
{
    int bestRow = -1;
    int minDomainSize = n + 1;

    // already assigned
    for (int row = 0; row < n; row++)
    {
        if (board[row] != -1)
            continue;

        int domainSize = popcount(domains[row]);

        if (domainSize < minDomainSize)
        {
            minDomainSize = domainSize;
            bestRow = row;
        }
    }

    return bestRow;
}

int AC3DVOSolver::countAssigned(const Solution &board) const
{
    int count = 0;
    for (int i = 0; i < n; i++)
    {
        if (board[i] != -1)
            count++;
    }
    return count;
}

void AC3DVOSolver::solve()
{
    std::stack<AC3DVOSearchState> stateStack;

    // initialize domains for all unassigned rows
    std::vector<uint64_t> initialDomains = initializeDomains(initialState);

    stateStack.push(AC3DVOSearchState(initialState, initialDomains));

    while (!stateStack.empty())
    {
        AC3DVOSearchState current = stateStack.top();
        stateStack.pop();

        // if maxDepth is set and we've reached it, add to work queue instead of continuing
        // this is only used for the seed generator solver
        if (maxDepth > 0 && countAssigned(current.board) == maxDepth)
        {
            std::lock_guard<std::mutex> lock(*queueMutex);
            workQueue->push(current.board);
            continue;
        }

        // if solution is found
        if (countAssigned(current.board) == n)
        {
            solutions.push_back(current.board);

            if (!foundFirst)
            {
                firstSolutionTime = std::chrono::high_resolution_clock::now();
                foundFirst = true;
            }
            continue;
        }

        // select row with mrv left
        int row = selectMRVRow(current.board, current.domains);

        if (row == -1)
            continue; // no valid row, but like, this shouldnt happen?

        uint64_t domain = current.domains[row];

        for (int col = 0; col < n; col++)
        {
            if (!(domain & (1ULL << col)))
                continue; // this value is not in domain

            // create new state with updated domains
            std::vector<uint64_t> newDomains = current.domains;

            // mark this row as assigned
            newDomains[row] = 0;

            // remove columns attacked by (row, col) using precomputed mask
            for (int otherRow = 0; otherRow < n; otherRow++)
            {
                if (otherRow != row)
                {
                    newDomains[otherRow] &= ~attackMask[row][otherRow][col];
                }
            }

            Solution newBoard = current.board;
            newBoard[row] = col;

            // enforce arc consistency
            if (enforceArcConsistency(newDomains, newBoard))
            {
                stateStack.push(AC3DVOSearchState(newBoard, newDomains));
            }
        }
    }
}

const std::vector<Solution> &AC3DVOSolver::getSolutions() const
{
    return solutions;
}

std::chrono::high_resolution_clock::time_point AC3DVOSolver::getFirstSolutionTime() const
{
    return firstSolutionTime;
}