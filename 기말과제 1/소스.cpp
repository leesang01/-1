#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <windows.h>

using namespace std;

enum ProcessType { FG, BG };

struct Process {
    int pid;
    ProcessType type;
    bool promoted;
    int sleep_time;
    thread::id tid;

    bool operator<(const Process& other) const {
        return sleep_time > other.sleep_time;  // sleep_time이 작은 것이 우선 순위가 높도록 설정
    }
};

mutex dq_mtx, wq_mtx, print_mtx;
condition_variable cv;
vector<Process> dynamic_queue;
priority_queue<Process> wait_queue;
bool running = true;

void print_status(int interval) {
    while (running) {
        this_thread::sleep_for(chrono::seconds(interval));

        lock_guard<mutex> lock(print_mtx);
        cout << "Running: ";
        for (const auto& process : dynamic_queue) {
            cout << "[" << process.pid << (process.type == FG ? "F" : "B") << (process.promoted ? "*" : "") << "]";
        }
        cout << endl;
        cout << "---------------------------" << endl;

        // Print Dynamic Queue
        cout << "DQ: ";
        for (const auto& process : dynamic_queue) {
            cout << process.pid << (process.type == FG ? "F" : "B") << " ";
        }
        cout << endl;

        // Print Wait Queue
        cout << "WQ: ";
        priority_queue<Process> temp_wq = wait_queue;
        while (!temp_wq.empty()) {
            auto p = temp_wq.top();
            temp_wq.pop();
            cout << "[" << p.pid << (p.type == FG ? "F" : "B") << ":" << p.sleep_time << "]";
        }
        cout << endl;
        cout << "---------------------------" << endl;
    }
}

void exec_process(Process process) {
    this_thread::sleep_for(chrono::seconds(process.sleep_time));
    {
        lock_guard<mutex> lock(print_mtx);
        cout << "Process " << process.pid << (process.type == FG ? "F" : "B") << " finished." << endl;
    }
}

void shell_process(int interval, const string& command_file) {
    ifstream file(command_file);
    string line;
    int pid_counter = 0;

    while (running && getline(file, line)) {
        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "echo") {
            string arg;
            iss >> arg;

            Process process = { pid_counter++, FG, false, interval };
            process.tid = this_thread::get_id();
            dynamic_queue.push_back(process);

            thread t(exec_process, process);
            t.detach();
        }
        // Implement other commands as needed

        this_thread::sleep_for(chrono::seconds(interval));
    }
}

void monitor_process(int interval) {
    while (running) {
        this_thread::sleep_for(chrono::seconds(interval));

        {
            lock_guard<mutex> lock(dq_mtx);
            if (!dynamic_queue.empty()) {
                auto process = dynamic_queue.front();
                dynamic_queue.erase(dynamic_queue.begin());

                lock_guard<mutex> wait_lock(wq_mtx);
                wait_queue.push(process);
            }
        }

        cv.notify_all();
    }
}

int main() {
    thread monitor(print_status, 2);
    thread shell(shell_process, 5, "commands.txt");

    monitor.join();
    shell.join();

    return 0;
}
