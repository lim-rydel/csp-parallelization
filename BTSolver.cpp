#include "BTSolver.h"
#include <cmath>

BTSolver::BTSolver(int boardSize, const Solution &initial, int maxDepth, std::queue<Solution> *wq, std::mutex *qm)
    : n(boardSize), initialState(initial), foundFirst(false), maxDepth(maxDepth), workQueue(wq), queueMutex(qm) {}

bool BTSolver::isSafe(const Solution &board, int row, int col)
{
    // columns
    for (int i = 0; i < row; i++)
    {
        if (board[i] == col)
            return false;
    }

    // diagonals
    for (int i = 0; i < row; i++)
    {
        if (abs(board[i] - col) == abs(i - row))
            return false;
    }

    return true;
}

void BTSolver::solve()
{
    std::stack<SearchState> stateStack;

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

    stateStack.push(SearchState(initialState, startRow));

    while (!stateStack.empty())
    {
        SearchState current = stateStack.top();
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

        // why did the solutions use this reverse order? does it matter?
        // for (int col = n - 1; col >= 0; col--)
        for (int col = 0; col < n; col++)
        {
            if (isSafe(current.board, current.row, col))
            {
                Solution newBoard = current.board;
                newBoard[current.row] = col;
                stateStack.push(SearchState(newBoard, current.row + 1));
            }
        }
    }
}

const std::vector<Solution> &BTSolver::getSolutions() const
{
    return solutions;
}

std::chrono::high_resolution_clock::time_point BTSolver::getFirstSolutionTime() const
{
    return firstSolutionTime;
}