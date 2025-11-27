#include "BTFCSolver.h"
#include <cmath>

BTFCSolver::BTFCSolver(int boardSize, const Solution &initial, int maxDepth, std::queue<Solution> *wq, std::mutex *qm)
    : n(boardSize), initialState(initial), foundFirst(false), maxDepth(maxDepth), workQueue(wq), queueMutex(qm)
{
    precomputeAttackMasks();
}

void BTFCSolver::precomputeAttackMasks()
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

std::vector<uint64_t> BTFCSolver::initializeDomains(const Solution &board, int startRow) const
{
    std::vector<uint64_t> domains(n);

    // initialize all unassigned rows with full domain
    for (int row = startRow; row < n; row++)
    {
        uint64_t available = (1ULL << n) - 1;

        // rmove columns that conflict with already assigned vars
        for (int prevRow = 0; prevRow < row; prevRow++)
        {
            if (board[prevRow] != -1)
            {
                int prevCol = board[prevRow];
                // remove columns attacked by this queen using precomputed mask
                available &= ~attackMask[prevRow][row][prevCol];
            }
        }

        domains[row] = available;
    }

    return domains;
}

void BTFCSolver::solve()
{
    std::stack<FCSearchState> stateStack;

    // find first unassigned row in initial state
    // can't start at 0, because parallel solvers have diff start states
    int startRow = 0;
    for (int i = 0; i < n; i++)
    {
        if (initialState[i] == -1)
        {
            startRow = i;
            break;
        }
    }

    // initialize domains for all unassigned rows
    std::vector<uint64_t> initialDomains = initializeDomains(initialState, startRow);

    stateStack.push(FCSearchState(initialState, startRow, initialDomains));

    while (!stateStack.empty())
    {
        FCSearchState current = stateStack.top();
        stateStack.pop();

        // if maxDepth is set and we've reached it, add to work queue instead of continuing
        // this is only used for the seed generator solver
        if (maxDepth > 0 && current.row == maxDepth)
        {
            std::lock_guard<std::mutex> lock(*queueMutex);
            workQueue->push(current.board);
            continue;
        }

        // if solution is found
        if (current.row == n)
        {
            solutions.push_back(current.board);

            if (!foundFirst)
            {
                firstSolutionTime = std::chrono::high_resolution_clock::now();
                foundFirst = true;
            }
            continue;
        }

        uint64_t domain = current.domains[current.row];

        for (int col = 0; col < n; col++)
        {
            if (!(domain & (1ULL << col)))
                continue; // this value is not in domain

            // forward check
            // would this assignment wipe out any future domain? if yes, die
            bool causesWipeout = false;

            for (int futureRow = current.row + 1; futureRow < n; futureRow++)
            {
                uint64_t futureDomain = current.domains[futureRow];

                // remove columns attacked by (row, col) using precomputed mask
                futureDomain &= ~attackMask[current.row][futureRow][col];

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
            for (int futureRow = current.row + 1; futureRow < n; futureRow++)
            {
                newDomains[futureRow] &= ~attackMask[current.row][futureRow][col];
            }

            Solution newBoard = current.board;
            newBoard[current.row] = col;
            stateStack.push(FCSearchState(newBoard, current.row + 1, newDomains));
        }
    }
}

const std::vector<Solution> &BTFCSolver::getSolutions() const
{
    return solutions;
}

std::chrono::high_resolution_clock::time_point BTFCSolver::getFirstSolutionTime() const
{
    return firstSolutionTime;
}