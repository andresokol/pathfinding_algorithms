#include "jpsearch.h"
#include "map.h"
#include "ilogger.h"
#include "environmentoptions.h"
#include <set>
#include <chrono>
#include "orderedvector.h"
#include <cmath>

JPSearch::JPSearch(double w, int BT, int SL)
{
    hweight = w;
    breakingties = BT;
    sizelimit = SL;
}

SearchResult JPSearch::startSearch(ILogger *Logger, const Map &Map, const EnvironmentOptions &options)
{
    std::set<Node> closed;

    auto cmp = [&](const Node & a, const Node & b) {return a.F < b.F;};
    OrderedVector<Node, decltype(&cmp)> open(&cmp);

    auto start_time = std::chrono::system_clock::now();

    Node start;
    start.i = Map.start_i;
    start.j = Map.start_j;
    start.g = 0;

    Node goal;
    goal.i = Map.goal_i;
    goal.j = Map.goal_j;

    open.push(start);

    sresult.pathfound = false;
    sresult.numberofsteps = 0;

    Node current_node, new_node;
    bool is_diagonal;
    while (!open.empty()) {
        current_node = open.pop();
        if (closed.count(current_node) != 0) continue;

        auto current_node_iterator = closed.insert(current_node).first;

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;

                if (i * j != 0) { // this means i != 0 and j != 0
                    if (options.allowdiagonal == 0) continue;
                    is_diagonal = true;
                }
                else {
                    is_diagonal = false;
                }

                new_node = Node(current_node.i + i, current_node.j + j);

                std::pair<bool,Node> jump_result = jump(new_node, i, j, Map, goal);

                if(jump_result.first) {
                    new_node.i = jump_result.second.i;
                    new_node.j = jump_result.second.j;
                    new_node.parent = &(*current_node_iterator);
                    calculateHeuristic(new_node, Map, options);

                    open.push(new_node);
                }

                if(new_node == goal) {
                    sresult.pathfound = true;
                    break;
                }
            }
            if(sresult.pathfound) break;
        }
        sresult.numberofsteps++;
        if(sresult.pathfound) break;
    }

    auto end_time = std::chrono::system_clock::now();
    sresult.time = (std::chrono::duration<double>(end_time - start_time)).count();

    sresult.nodescreated = open.size() + closed.size();

    if (sresult.pathfound) {
        sresult.pathlength = 0;

        current_node = new_node;

        sresult.lppath = new NodeList();
        sresult.hppath = new NodeList();

        while (!(current_node == start)) {
            sresult.lppath->push_front(current_node);
            sresult.hppath->push_front(current_node);
            sresult.pathlength++;

            current_node = *current_node.parent;
        }

        // Adding start node to path
        sresult.lppath->push_front(current_node);
        sresult.hppath->push_front(current_node);
    }
    else {
        sresult.lppath = NULL;
        sresult.hppath = NULL;
        sresult.pathlength = 0;
    }

    return sresult;
}

std::pair<bool, Node> JPSearch::jump(const Node &node, int di, int dj, const Map &map, const Node &goal)
{
    // goal point is also the jump point
    if (node == goal) return std::pair<bool, Node>(true, node);
    if (!map.CellOnGrid(node.i + di, node.j + dj)) {std::cout << "border" << std::endl; return std::pair<bool, Node>(false, node);}

    if (di * dj == 0) { // moving along lines
        if (map.CellIsObstacle(node.i + di, node.j + dj)) return std::pair<bool, Node>(false, node);

        // check if we found jump point
        if (dj == 0) {
            if (map.CellIsObstacle(node.i, node.j + 1) &&
                !map.CellIsObstacle(node.i + di, node.j + 1)) return std::pair<bool, Node>(true, node);
            if (map.CellIsObstacle(node.i, node.j - 1) &&
                !map.CellIsObstacle(node.i + di, node.j - 1)) return std::pair<bool, Node>(true, node);
        }

        if (di == 0) {
            if (map.CellIsObstacle(node.i + 1, node.j) &&
                !map.CellIsObstacle(node.i + 1, node.j + dj)) return std::pair<bool, Node>(true, node);
            if (map.CellIsObstacle(node.i - 1, node.j) &&
                !map.CellIsObstacle(node.i - 1, node.j + dj)) return std::pair<bool, Node>(true, node);
        }

        // can move forward in any other case
        return jump(Node(node.i + di, node.j + dj), di, dj, map, goal);
    }
    else { // moving diagonal
        if (map.CellIsObstacle(node.i - di, node.j) &&
                !map.CellIsObstacle(node.i - di, node.j + dj)) return std::pair<bool,Node>(true, node);
        if (map.CellIsObstacle(node.i, node.j - dj) &&
                !map.CellIsObstacle(node.i + di, node.j - dj)) return std::pair<bool,Node>(true, node);

        if (map.CellIsObstacle(node.i + di, node.j + dj)) return std::pair<bool, Node>(false, node);

        if (jump(node, di, 0, map, goal).first) return std::pair<bool,Node>(true, node);

        if (jump(node, 0, dj, map, goal).first) return std::pair<bool,Node>(true, node);

        return jump(Node(node.i + di, node.j + dj), di, dj, map, goal);
    }
}

void JPSearch::calculateHeuristic(Node &a, const Map &map, const EnvironmentOptions &options)
{
    int di = abs(a.i - map.goal_i),
        dj = abs(a.j - map.goal_j);

    a.g = a.parent->g;
    if(options.allowdiagonal)
        a.g += std::min(di, dj) * options.diagonalcost + abs(di - dj) * options.linecost;
    else
        a.g += (di + dj) * options.linecost;

    a.F = a.g;

    // Normalizing heuristics with linecost
    if (options.metrictype == CN_SP_MT_EUCL)      a.H = sqrt(di * di + dj * dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_MANH) a.H = (di + dj) * options.linecost;
    else if (options.metrictype == CN_SP_MT_DIAG) a.H = std::min(di, dj) * options.diagonalcost +
                                                                        abs(di - dj) * options.linecost;
    else a.H = std::max(di, dj) * options.linecost;

    a.F += hweight * a.H;
}