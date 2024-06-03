//2-1 x 2-2 o 2-3 x
#include <iostream>
#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <queue>
#include <sstream>
#include <chrono>

using namespace std;

// Process structure
struct Process {
    int pid;
    char type; // 'F' for Foreground, 'B' for Background
    int remainingTime;
    bool promoted;
    Process(int id, char t, int time = 0) : pid(id), type(t), remainingTime(time), promoted(false) {}
};

// Stack Node structure
struct StackNode {
    list<Process> processes;
    shared_ptr<StackNode> next;
    StackNode() : next(nullptr) {}
};

class DynamicQueue {
private:
    shared_ptr<StackNode> top;
    mutex mtx;
    int threshold;

public:
    DynamicQueue() : top(make_shared<StackNode>()), threshold(5) {} // Adjust threshold as needed

    void enqueue(Process proc) {
        lock_guard<mutex> lock(mtx);
        if (proc.type == 'F') {
            top->processes.push_back(proc);
        }
        else {
            top->processes.push_front(proc);
        }
        split_n_merge();
    }

    Process dequeue() {
        lock_guard<mutex> lock(mtx);
        while (top && top->processes.empty() && top->next) {
            top = top->next;
        }
        if (!top->processes.empty()) {
            Process proc = top->processes.front();
            top->processes.pop_front();
            return proc;
        }
        return Process(-1, 'N'); // Indicate an empty process
    }

    void promote() {
        lock_guard<mutex> lock(mtx);
        if (top->next && !top->processes.empty()) {
            Process proc = top->processes.front();
            top->processes.pop_front();
            proc.promoted = true;
            top->next->processes.push_back(proc);
            if (top->processes.empty()) {
                top = top->next;
            }
        }
    }

    void split_n_merge() {
        auto current = top;
        while (current) {
            if (current->processes.size() > threshold) {
                auto new_node = make_shared<StackNode>();
                auto it = current->processes.begin();
                advance(it, current->processes.size() / 2);
                new_node->processes.splice(new_node->processes.begin(), current->processes, it, current->processes.end());
                new_node->next = current->next;
                current->next = new_node;
            }
            current = current->next;
        }
    }

    void display() {
        lock_guard<mutex> lock(mtx);
        auto current = top;
        bool first = true;
        while (current) {
            if (first) {
                cout << "P => ";
                first = false;
            }
            else {
                cout << "      ";
            }
            cout << "[";
            for (const auto& proc : current->processes) {
                cout << proc.pid << (proc.type == 'F' ? 'F' : 'B');
                if (proc.promoted) cout << '*';
                cout << " ";
            }
            if (current->next == nullptr) {
                cout << "] (top)" << endl;
            }
            else {
                cout << "]" << endl;
            }
            current = current->next;
        }
        cout << "(bottom)" << endl;
    }
};

// Parse function
vector<string> parse(const string& command) {
    istringstream iss(command);
    vector<string> tokens;
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Execute function
void exec(const vector<string>& args) {
    if (args.empty()) return;

    // Example commands (you can expand these)
    if (args[0] == "echo") {
        for (size_t i = 1; i < args.size(); ++i) {
            cout << args[i] << " ";
        }
        cout << endl;
    }
    else if (args[0] == "dummy") {
        // Do nothing
    } // Add more commands as needed
}

class Scheduler {
private:
    DynamicQueue dq;
    queue<Process> waitQueue;
    mutex mtx;
    condition_variable cv;
    atomic<bool> running;
    bool newCommand;

public:
    Scheduler() : running(true), newCommand(false) {}

    void enqueue(Process proc) {
        dq.enqueue(proc);
        notify();
    }

    Process dequeue() {
        return dq.dequeue();
    }

    void promote() {
        dq.promote();
        notify();
    }

    void run() {
        while (running || newCommand) {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [this] { return newCommand || !running; });

            if (!running && !newCommand) break;

            cout << "Running: [1B]" << endl;
            cout << "---------------------------" << endl;
            cout << "DQ: ";
            dq.display();
            cout << "---------------------------" << endl;
            cout << "WQ: ";
            queue<Process> tempWQ = waitQueue;
            while (!tempWQ.empty()) {
                Process proc = tempWQ.front();
                tempWQ.pop();
                cout << "[" << proc.pid << (proc.type == 'F' ? 'F' : 'B') << ":" << proc.remainingTime << "] ";
            }
            cout << endl;

            newCommand = false; // Reset flag
        }
    }

    void stop() {
        running = false;
        notify();
    }

    void notify() {
        {
            lock_guard<mutex> lock(mtx);
            newCommand = true;
        }
        cv.notify_all();
    }

    void addWaitQueue(Process proc) {
        lock_guard<mutex> lock(mtx);
        waitQueue.push(proc);
    }
};

atomic<int> pidCounter(0);

void shell(Scheduler& scheduler) {
    static vector<string> commands = {
        "dummy",
        "dummy",
        "dummy",
        "dummy"
    };
    for (const auto& command : commands) {
        vector<string> args = parse(command);
        exec(args);

        // Simulate adding a process to the queue
        scheduler.enqueue(Process(pidCounter++, 'F'));

        // Simulate sleep
        this_thread::sleep_for(chrono::seconds(1));
    }
    scheduler.stop(); // Signal to stop the scheduler
}

int main() {
    Scheduler scheduler;

    // Initialize dynamic queue with the first shell and monitor process
    scheduler.enqueue(Process(pidCounter++, 'F')); // Shell process
    scheduler.enqueue(Process(pidCounter++, 'B')); // Monitor process

    // Create shell and scheduler threads
    thread schedulerThread(&Scheduler::run, &scheduler);
    thread shellThread(shell, ref(scheduler));

    // Add some processes to the wait queue to demonstrate WQ output
    scheduler.addWaitQueue(Process(pidCounter++, 'F', 5));
    scheduler.addWaitQueue(Process(pidCounter++, 'B', 3));

    // Wait for shell and scheduler threads to finish
    shellThread.join();
    schedulerThread.join();

    return 0;
}
