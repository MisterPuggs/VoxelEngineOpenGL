//
// Created by cew05 on 23/08/2024.
//

#ifndef UNTITLED7_CHUNKTHREADS_H
#define UNTITLED7_CHUNKTHREADS_H

#include <thread>
#include <mutex>
#include <condition_variable>

#include <queue>
#include <functional>
#include <chrono>
#include <string>

#include <glm/glm.hpp>
#include <SDL.h>

typedef int THREAD_ACTION_RESULT;

/*
 * Simple struct to house a position of the chunk that the given function will be applied to.
 */

struct ThreadAction {
    std::function<THREAD_ACTION_RESULT(const glm::ivec2&, const glm::vec3&)> function {};
    glm::ivec2 chunkPos {0, 0};
    glm::vec3 chunkBlock {0, 0, 0};
    int attempted = 0;

    enum {
        OK, FAIL, RETRY, // ...
    };

    // Functionality
    [[nodiscard]] THREAD_ACTION_RESULT DoAction() const { return function(chunkPos, chunkBlock); }
};


/*
 *
 */

struct ActionTimer {
    int actionsCompleted = 0;
    Uint64 avgNStaken = 0;
    Uint64 sumNStaken = 0;
};



/*
 *
 */

/*
 *
 */

class ChunkThreads {
    protected:
        std::deque<ThreadAction> actionQueue {};
        std::mutex queueMutex;

        // Thread management
        std::condition_variable threadCV;
        std::mutex threadMutex;
        bool enabled = false;
        bool finished = true;

        // Thread functionality
        void ThreadLoop();
        std::thread chunkThread;
        std::function<bool(const glm::ivec2&, const glm::vec3&)> retryCheckFunction {};

        // action time measurements
        ActionTimer lightActions;
        ActionTimer heavyActions;
        unsigned int allActions = 0;

        // Thread Name (primarily for debugging)
        std::string threadName {"UNNAMED_THREAD"};

    public:
        ChunkThreads();
        explicit ChunkThreads(const std::string& _threadName);
        ~ChunkThreads();

        // Thread Management
        void StartThread();
        void EndThread();
        void SetRetryCheckFunction(const std::function<bool(const glm::ivec2&, const glm::vec3&)>& _retryCheckFunction);

        // Adding new actions to be completed in the thread
        void AddActions(const std::vector<ThreadAction>& _actions);
        void AddPriorityActions(const std::vector<ThreadAction>& _actions);
        void AddActionRegion(const ThreadAction& _originAction, int _radius, bool _squareRegion = false);
        void AddPriorityActionRegion(const ThreadAction& _originAction, int _radius, bool _squareRegion = false);

        // Debug Output
        void PrintThreadResults();
        bool debugOutputEnabled = true;

        //
        [[nodiscard]] bool HasActions() const {
            return !actionQueue.empty();
        }
};

#endif //UNTITLED7_CHUNKTHREADS_H
