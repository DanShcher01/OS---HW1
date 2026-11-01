#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    std::cout << "smash: got ctrl-C" << std::endl;
    pid_t fg_pid = SmallShell::getInstance().getFgPid();
    //if the process is in the foreground
    if(fg_pid != -1) {
        if (kill(fg_pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        } else{
            std::cout << "smash: process " << fg_pid << " was killed" << std::endl;
        }
    }
}
