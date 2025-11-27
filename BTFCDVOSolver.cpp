#include "BTFCDVOSolver.h"
#include <cmath>

BTFCDVOSolver::BTFCDVOSolver(int boardSize, const Solution &initial, int maxDepth, std::queue<Solution> *wq, std::mutex *qm)
    : n(boardSize), initialState(initial), foundFirst(false), maxDepth(maxDepth), workQueue(wq), queueMutex(qm)
{
    precomputeAttackMasks();
}

void BTFCDVOSolver::precomputeAttackMasks()
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
inline int BTFCDVOSolver::popcount(uint64_t x) const
{
    int count = 0;
    while (x)
    {
        count++;
        x &= x - 1;
    }
    return count;
}

std::vector<uint64_t> BTFCDVOSolver::initializeDomains(const Solution &board) const
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

int BTFCDVOSolver::selectMRVRow(const Solution &board, const std::vector<uint64_t> &domains) const
{
    int bestRow = -1;
    int minDomainSize = n + 1;

    for (int row = 0; row < n; row++)
    {
        // already assigned
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

int BTFCDVOSolver::countAssigned(const Solution &board) const
{
    int count = 0;
    for (int i = 0; i < n; i++)
    {
        if (board[i] != -1)
            count++;
    }
    return count;
}

void BTFCDVOSolver::solve()
{
    std::stack<DVOSearchState> stateStack;

    std::vector<uint64_t> initialDomains = initializeDomains(initialState);

    stateStack.push(DVOSearchState(initialState, initialDomains));

    while (!stateStack.empty())
    {
        DVOSearchState current = stateStack.top();
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

            // forward check
            // would this assignment wipe out any future domain? if yes, die
            bool causesWipeout = false;

            for (int futureRow = 0; futureRow < n; futureRow++)
            {
                if (current.board[futureRow] != -1)
                    continue; // already assigned
                if (futureRow == row)
                    continue; // its the current row

                uint64_t futureDomain = current.domains[futureRow];

                // remove columns attacked by (row, col) using precomputed mask
                futureDomain &= ~attackMask[row][futureRow][col];

                if (futureDomain == 0)
                {
                    causesWipeout = true;
                    break;
                }
            }

            if (causesWipeout)
                continue;

            std::vector<uint64_t> newDomains = current.domains;

            // update domains by remove attacked columns
            for (int futureRow = 0; futureRow < n; futureRow++)
            {
                if (current.board[futureRow] != -1)
                    continue; // already assigned
                if (futureRow == row)
                    continue; // its the current row

                newDomains[futureRow] &= ~attackMask[row][futureRow][col];
            }

            // mark this row as assigned
            newDomains[row] = 0;

            Solution newBoard = current.board;
            newBoard[row] = col;
            stateStack.push(DVOSearchState(newBoard, newDomains));
            // stateStack.push(FCSearchState(newBoard, current.row + 1, newDomains));
        }
    }
}

const std::vector<Solution> &BTFCDVOSolver::getSolutions() const
{
    return solutions;
}

std::chrono::high_resolution_clock::time_point BTFCDVOSolver::getFirstSolutionTime() const
{
    return firstSolutionTime;
}