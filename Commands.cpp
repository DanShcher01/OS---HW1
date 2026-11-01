#define _LARGEFILE64_SOURCE

#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <map>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <iomanip>
#include "Commands.h"

//for sysinfo
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <time.h>

//for WhoAmI
#include <pwd.h>

//for IO redirection
#include <fcntl.h>

//for alias
#include <regex>

//for du
#include <sys/stat.h>
#include <sys/syscall.h>

//for usbinfo
#include <algorithm>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

size_t SpecialCharOutsideQuotes(const std::string& str, const std::string& target) {
    bool in_single_quotes = false;
    //search for '
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\'') {
            in_single_quotes = !in_single_quotes;
        }
        //search for special symbol if outside quotes (')
        if (!in_single_quotes) {
            if (str.substr(i, target.length()) == target) {
                return i;
            }
        }
    }
    return std::string::npos;
}


// initialize SmallShell static command map
const std::map<std::string, Command*(*)(const char*)> SmallShell::cmd_map = {
    {"chprompt", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new ChangePromptCommand(s); })},
    {"pwd", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new GetCurrDirCommand(s); })},
    {"showpid", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new ShowPidCommand(s); })},
    {"cd", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new ChangeDirCommand(s); })},
    {"jobs", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new JobsCommand(s); })},
    {"fg", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new ForegroundCommand(s); })},
    {"quit", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new QuitCommand(s); })},
    {"kill", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new KillCommand(s); })},
    {"alias", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new AliasCommand(s); })},
    {"unalias", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new UnAliasCommand(s); })},
    {"unsetenv", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new UnSetEnvCommand(s); })},
    {"sysinfo", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new SysInfoCommand(s); })},
    {"whoami", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new WhoAmICommand(s); })},
    {"du", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new DiskUsageCommand(s); })},
    {"usbinfo", static_cast<Command*(*)(const char*)>([](const char* s)->Command* { return new USBInfoCommand(s); })}
};

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    size_t idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}


Command::Command(const char *cmd_line) : pid(-1) {
    this->cmd_line = (char*)malloc(COMMAND_MAX_LENGTH);
    if (cmd_line != nullptr) {
        this->cmd_line = strdup(cmd_line);
    } else{
        this->cmd_line = nullptr;
    }

    argsNum = _parseCommandLine(cmd_line, args);
}
Command::~Command() {
    free((void*)cmd_line);
}

// JobsList Definitions
JobsList::JobsList() {}
JobsList::~JobsList() {}

SmallShell::SmallShell() : fg_pid(-1), lastPwd(nullptr), prompt("smash> "){
    jobs = new JobsList();
    
}

SmallShell::~SmallShell() {
    delete jobs;
    free(lastPwd);
}

BuiltInCommand :: BuiltInCommand(const char *cmd_line): Command(cmd_line){}
BuiltInCommand:: ~BuiltInCommand() {
    for (int i = 0; i < argsNum; i++) {
        free(args[i]);
    }
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    for (const auto &alias : aliases) {
        if (alias.first == firstWord) {
            cmd_s.replace(0, firstWord.length(), alias.second);
            firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
            break;
        }
    }

    if (SpecialCharOutsideQuotes(cmd_s, ">") != string::npos ||
        SpecialCharOutsideQuotes(cmd_s, ">>") != string::npos) {
        return new RedirectionCommand(cmd_s.c_str());
    }
    else if (SpecialCharOutsideQuotes(cmd_s, "|") != string::npos ||
             SpecialCharOutsideQuotes(cmd_s, "|&") != string::npos) {
        return new PipeCommand(cmd_s.c_str());
    }

    auto it = SmallShell::getCmdMap().find(firstWord);
    if (it != SmallShell::getCmdMap().end()) {
        return it->second(cmd_s.c_str());
    }
    return new ExternalCommand(cmd_s.c_str());
}

void SmallShell::executeCommand(const char *cmd_line) {
    if(jobs != nullptr) {
        jobs->removeFinishedJobs();
    }
    if (cmd_line == nullptr || strlen(cmd_line) == 0) {
        return;
    }
    Command* cmd = CreateCommand(cmd_line);
    //execute and free memory afterwards if command is valid
    if(cmd != nullptr){
        cmd->execute();
        delete cmd;
    }
}

//1. chprompt command
ChangePromptCommand::ChangePromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void ChangePromptCommand::execute() {
    if (argsNum == 1) {
        SmallShell::getInstance().setPrompt("smash> ");
    } else {
        SmallShell::getInstance().setPrompt(string(args[1]) + "> ");
    }
}

//2. ShowPidCommand
ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void ShowPidCommand::execute() {
    pid_t smash_pid = getpid();
    std::cout << "smash pid is " << smash_pid << std::endl;
}

//3. pwd command
GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void GetCurrDirCommand::execute() {
    char *cwd = getcwd(nullptr, 0);
    if(cwd == nullptr){
        perror("smash error: getcwd failed");
    } else {
        std::cout << cwd << std::endl;
        free(cwd);
    }
}

//4. cdCommand
ChangeDirCommand::ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void ChangeDirCommand::execute() {
    char *lastPath = SmallShell::getInstance().getLastPwd();
    char *path = nullptr;
    char *currentPath;
    //check number of arguments
    switch(argsNum){
        case 1:
            return;
        case 2:
            path = args[1];
            //special case -
            if(strcmp(path, "-") == 0) {
                if(lastPath == nullptr) {
                    std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
                    return;
                } else {
                    path = lastPath;
                }
            }
            currentPath = getcwd(nullptr, 0);
            //regular cd
            if(chdir(path) == -1){
                perror("smash error: chdir failed");
                free(currentPath);
            } else {
                if(currentPath != nullptr) {
                    free(lastPath);
                }
                //update last path
                SmallShell::getInstance().setLastPwd(currentPath);
            }
            break;
        default:
            std::cerr << "smash error: cd: too many arguments" << std::endl;
            break;
    }
}

//5. jobs command
JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void JobsCommand::execute() {
    JobsList *jobList = SmallShell::getInstance().getJobs();
    jobList->printJobsList();
}

//6. fg command
ForegroundCommand::ForegroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void ForegroundCommand::execute() {
    JobsList *jobList = SmallShell::getInstance().getJobs();
    int jobId = -1;
    JobsList::JobEntry *job = nullptr;
    switch(argsNum) {
        case 1:
            job = jobList->getLastJob(&jobId);
            if(job == nullptr) {
                std::cerr << "smash error: fg: jobs list is empty" << std::endl;
                break;
            }
            break;
        case 2:
            try {
                jobId = std::stoi(args[1]);
            } catch (std::exception &e) {
                std::cerr << "smash error: fg: invalid arguments" << std::endl;
                break;
            }
            job = jobList->getJobById(jobId);
            if(job == nullptr) {
                std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
                break;
            }
            break;
        default:
            std::cerr << "smash error: fg: invalid arguments" << std::endl;
    }
    if(job != nullptr) {
        std::cout << job->cmd_line << " " << job->pid << std::endl;
        SmallShell::getInstance().setFgPid(job->pid);
        waitpid(job->pid, nullptr, 0);
        SmallShell::getInstance().setFgPid(-1);
        jobList->removeJobById(jobId);
    }
}

//7. quit command
QuitCommand::QuitCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
void QuitCommand::execute() {
    // Check for kill argument
    JobsList *jobList = SmallShell::getInstance().getJobs();
    if (argsNum > 1 && strcmp(args[1], "kill") == 0) {
        std::cout << "smash: sending SIGKILL signal to " << jobList->getJobCount() << " jobs:" << std::endl;
        jobList->killAllJobs();
    }
    exit(0);
}

//8. kill command
KillCommand::KillCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}
void KillCommand::execute() {
    int jobId = -1;
    int signum = -1;
    JobsList::JobEntry *job;
    JobsList *jobList = SmallShell::getInstance().getJobs();
    std::string sig_str;
    bool isValid = true;
    //check the number of arguments
    switch(argsNum) {
        case 3:
            sig_str = args[1];
            if(sig_str.length() < 2|| sig_str[0] != '-') {
                std::cerr << "smash error: kill: invalid arguments" << std::endl;
                isValid = false;
                break;
            }
            try {
                jobId = std::stoi(args[2]);
                signum = std::stoi(sig_str.substr(1));
            } catch (std::exception &e) {
                std::cerr << "smash error: kill: invalid arguments" << std::endl;
                isValid = false;
                break;
            }
            job = jobList->getJobById(jobId);
            if (job == nullptr) {
                std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
                isValid = false;
                break;
            }
            break;
        default:
            std::cerr << "smash error: kill: invalid arguments" << std::endl;
            isValid = false;
            break;
    }
    if(isValid) {
        if (kill(job->pid, signum) == -1) {
            perror("smash error: kill failed");
        } else {
            std::cout << "signal number " << signum << " was sent to pid " << job->pid << std::endl;
        }
    }
}

//9. alias command
AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void AliasCommand::execute() {
    std::vector<std::pair<std::string, std::string>> &aliases = SmallShell::getInstance().getAliases();

    // list aliases if no arguments given
    if (argsNum == 1) {
        for (const auto &alias : aliases) {
            std::cout << alias.first << "='" << alias.second << "'" << std::endl;
        }
        return;
    }

    // validate cmd_line with regex
    std::string cmd = _trim(std::string(cmd_line));
    std::regex pattern(R"(^alias\s+([A-Za-z0-9_]+)='([^']*)'$)");
    std::smatch matches;
    if (!std::regex_match(cmd, matches, pattern) || matches.size() != 3) {
        std::cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    std::string name = matches[1].str();
    std::string value = matches[2].str();

    // check for reserved or existing alias
    if (SmallShell::getCmdMap().find(name) != SmallShell::getCmdMap().end()){
        std::cerr << "smash error: alias: " << name << " already exists or is a reserved command" << std::endl;
        return;
    }

    for (const auto &a : aliases) {
        if (a.first == name) {
            std::cerr << "smash error: alias: " << name << " already exists or is a reserved command" << std::endl;
            return;
        }
    }

    // add new alias (preserve inner value as-is)
    aliases.emplace_back(name, value);
}

//10. unalias command
UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void UnAliasCommand::execute() {
    std::vector<std::pair<std::string, std::string>> &aliases = SmallShell::getInstance().getAliases();

    if (argsNum < 2) {
        std::cerr << "smash error: unalias: not enough arguments" << std::endl;
        return;
    }

    for (int i = 1; i < argsNum; ++i) {
        std::string name = args[i];
        bool found = false;

        for (const auto &a : aliases) {
            if (a.first == name) {
                aliases.erase(
                    std::remove(aliases.begin(), aliases.end(), a),
                    aliases.end()
                );
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "smash error: unalias: "<< name << " alias does not exist" << std::endl;
            return;
        }  
    }
}

//11. unsetenv command
extern char **__environ;

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void UnSetEnvCommand::execute() {
    if (argsNum == 1) {
        std::cerr << "smash error: unsetenv: not enough arguments" << std::endl;
        return;
    }

    for (int i = 1; i < argsNum; ++i) {
        const char *varName = args[i];
        const size_t nameLen = strlen(varName);
        bool found = false;
        for (char **env = __environ; *env != nullptr; ++env) {
            if (strncmp(*env, varName, nameLen) == 0 && (*env)[nameLen] == '=') {
                found = true;
                char **removePtr = env;
                while (*removePtr != nullptr) {
                    *removePtr = *(removePtr + 1);
                    ++removePtr;
                }
                break;
            }
        }
        if (!found) {
            std::cerr << "smash error: unsetenv: " << varName << " does not exist" << std::endl;
            return;
        }
    }
}

//12. sysinfo
SysInfoCommand::SysInfoCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
void SysInfoCommand::execute() {

    //get OS name
    struct utsname uts;
    if (uname(&uts) == -1) {
        perror("smash error: uname failed");
        return;
    }
    // get time since boot
    struct sysinfo info;
    if (sysinfo(&info) == -1) {
        perror("smash error: sysinfo failed");
        return;
    }

    // boot time calculation and formatting
    time_t now;
    time(&now);
    time_t boot_time = now - info.uptime;
    struct tm *tm_info = localtime(&boot_time);
    char time_buffer[80];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    std::cout << "System: " << uts.sysname << std::endl;
    std::cout << "Hostname: " << uts.nodename << std::endl;
    std::cout << "Kernel: " << uts.release << std::endl;
    std::cout << "Architecture: " << uts.machine << std::endl;
    std::cout << "Boot Time: " << time_buffer << std::endl;

}

//External Command
ExternalCommand::ExternalCommand(const char *cmd_line): Command(cmd_line){}
void ExternalCommand::execute() {
    //clean command
    JobsList *jobList = SmallShell::getInstance().getJobs();
    string cmd_s = _trim(string(cmd_line));
    bool isBackground = _isBackgroundComamnd(cmd_line);
    char clean_cmd_line[COMMAND_MAX_LENGTH];
    strncpy(clean_cmd_line, cmd_s.c_str(), COMMAND_MAX_LENGTH);
    if(isBackground) {
        _removeBackgroundSign(clean_cmd_line);
    }
    bool isComplex = (string(clean_cmd_line).find_first_of("*?") != string::npos);
    //fork
    pid_t pid = fork();
    if(pid == -1){
        perror("smash error: fork failed");
        return;
    }
    if(pid == 0){
        //execute in child
        setpgrp();
        if(isComplex){
            execl("/bin/bash", "bash", "-c", clean_cmd_line, nullptr);
            //if execl returns then it failed
            perror("smash error: execl failed");
            exit(1);
        } else {
            char *args[COMMAND_MAX_ARGS];
            int argsNum = _parseCommandLine(clean_cmd_line, args);
            execvp(args[0], args);
            //if execvp returns then it failed
            perror("smash error: execvp failed");
                for (int i = 0; i < argsNum; i++) {
                    free(args[i]);
                }
            exit(1);
        }
    } else{
        //execute in parent
        if(isBackground) {
            this->pid = pid;
            jobList->addJob(this, false);
        } else{
            SmallShell::getInstance().setFgPid(pid);
            waitpid(pid, nullptr, 0);
            SmallShell::getInstance().setFgPid(-1);
        }
    }
}

//Special Commands

// IO Redirection
RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}
void RedirectionCommand::execute() {

    // determine the required redirection type
    std::string s = _trim(std::string(cmd_line));
    size_t pos = SpecialCharOutsideQuotes(s, ">>");
    size_t opLen = 2;
    if (pos == std::string::npos) {
        pos = SpecialCharOutsideQuotes(s, ">");
        opLen = 1;
    }

    std::string commandPart = _trim(s.substr(0, pos));
    std::string filePart = _trim(s.substr(pos + opLen));

    int flags = O_CREAT | O_WRONLY | ((opLen == 2) ? O_APPEND : O_TRUNC);
    int fd = open(filePart.c_str(), flags, 0644);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    int savedStdout = dup(STDOUT_FILENO);
    if (savedStdout == -1) {
        perror("smash error: dup failed");
        close(fd);
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed");
        close(fd);
        close(savedStdout);
        return;
    }
    close(fd);

    Command *cmd = SmallShell::getInstance().CreateCommand(commandPart.c_str());
    if (cmd == nullptr) {
        if (dup2(savedStdout, STDOUT_FILENO) == -1) {
            perror("smash error: dup2 failed");
        }
        close(savedStdout);
        return;
    }

    cmd->execute();
    delete cmd;

    if (dup2(savedStdout, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 restore failed");
    }
    close(savedStdout);
}


// du command
struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[1];
};
DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {}
long long DiskUsageCommand::calculateBlocks(const char *path) {
    struct stat st{};

    // get link status
    if (lstat(path, &st) == -1) {
        perror("smash error: lstat failed");
        return 0;
    }

    // symbolic links - return 0
    if (S_ISLNK(st.st_mode)) {
        return 0;
    }

    // open the file or directory
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return 0;
    }

    // get file status
    if (fstat(fd, &st) == -1) {
        close(fd);
        perror("smash error: fstat failed");
        return 0;
    }

    // regular file - return its size
    if (S_ISREG(st.st_mode)) {
        long long blocks = st.st_blocks;
        close(fd);
        return blocks;
    }

    // not a directory - return 0
    if (!S_ISDIR(st.st_mode)) {
        close(fd);
        return 0;
    }

    // include the directory's own blocks
    long long totalBlocks = st.st_blocks;
    char buffer[4096];

    // read directory entries using getdents64 syscall
    while (true) {
        int bytesRead = syscall(__NR_getdents64 , fd, buffer, sizeof(buffer));

        if (bytesRead == -1) {
            perror("smash error: getdents64 failed");
            close(fd);
            return totalBlocks;
        }

        if (bytesRead == 0) {
            break;
        }

        for (int pos = 0; pos < bytesRead; ) {
            auto* entry = reinterpret_cast<linux_dirent64*>(buffer + pos);

            std::string name = entry->d_name;

            if (name != "." && name != "..") {
                std::string childPath = std::string(path) + "/" + name;
                totalBlocks += calculateBlocks(childPath.c_str());
            }

            pos += entry->d_reclen;
        }
    }

    close(fd);
    return totalBlocks;
}
void DiskUsageCommand::execute() {
    if (argsNum > 2) {
        std::cerr << "smash error: du: too many arguments" << std::endl;
        return;
    }

    string path;
    if (argsNum == 1) {
        path = ".";
    } else {
        path = args[1];
    }

    long long totalBlocks = calculateBlocks(path.c_str());
    long long totalKB = (totalBlocks + 1) / 2; // 2 blocks of 512 bytes = 1 KB

    std::cout << "Total disk usage: " << totalKB << " KB" << std::endl;
}

//WhoAmI
WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line) {}
void WhoAmICommand::execute() {
    uid_t uid = getuid(); //user ID
    gid_t gid = getgid(); //group ID

    const char* user = std::getenv("USER");
    const char* home = std::getenv("HOME");

    if(user != nullptr && home != nullptr) {
        std::cout << user << std::endl;
        std::cout << uid << std::endl;
        std::cout << gid << std::endl;
        std::cout << home << std::endl;
    } else {
        perror("smash error: Environment variables USER or HOME not set");
    }
}
//Pipe
PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}
void PipeCommand::execute() {
    //1. parsing:
    //search for | or |&
    std::string s_cmd(cmd_line);
    bool isStdErr = false;

    size_t pos = SpecialCharOutsideQuotes(s_cmd, "|&");
    if (pos != std::string::npos) {
        isStdErr = true;
    } else {
        pos = SpecialCharOutsideQuotes(s_cmd, "|");
    }

    if (pos == std::string::npos) return;

    // 2. Split the commands
    std::string cmd1 = s_cmd.substr(0, pos);
    std::string cmd2 = s_cmd.substr(pos + (isStdErr ? 2 : 1));
    //split the command into two parts
    //2. create pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("smash error: pipe failed");
        return;
    }
    //left command - pipe writer
    pid_t pid1 = fork();
    if(pid1 == -1) {
        perror("smash error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if(pid1 == 0){
        setpgrp();
        //writes to pipe
        dup2(pipefd[1], isStdErr ? STDERR_FILENO : STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        //execute cmd2
        SmallShell::getInstance().executeCommand(cmd1.c_str());
        exit(0);

    }
    //end of pipe writer

    //right command - pipe read
    pid_t pid2 = fork();
    if(pid2 == -1) {
        perror("smash error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if(pid2 == 0){
        setpgrp();
        //reads from pipe
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        //execute cmd2
        SmallShell::getInstance().executeCommand(cmd2.c_str());
        exit(0);

    }
    //end of pipe reader

    //parent process
    close(pipefd[0]);
    close(pipefd[1]);
    //to avoid SIGPIPE we track pid2.
    SmallShell::getInstance().setFgPid(pid2);
    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
    SmallShell::getInstance().setFgPid(-1);
}

// usbinfo command
struct UsbDeviceInfo {
    int devnum;
    std::string vendor;
    std::string productId;
    std::string manufacturer;
    std::string product;
    std::string power;
};
USBInfoCommand::USBInfoCommand(const char *cmd_line) : Command(cmd_line) {}
std::string USBInfoCommand::readDeviceParam(const std::string& path)
{
    // open device parameter file
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return "N/A";
    }

    char buffer[256];
    int bytesRead = read(fd, buffer, sizeof(buffer)-1);
    close(fd);
    if (bytesRead < 0){
        perror("smash error: read failed");
        return "N/A";
    } else if (bytesRead == 0){
        return "N/A";
    }

    buffer[bytesRead] = '\0';

    return _trim(std::string(buffer));
}
void USBInfoCommand::execute() {
    const char* usbPath = "/sys/bus/usb/devices";

    std::vector<UsbDeviceInfo> devices;

    // open the USB devices directory
    int fd = open(usbPath, O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    char buffer[4096];

    // read directory entries using getdents64
    while(true)
    {
        int bytesRead = syscall(__NR_getdents64 , fd, buffer, sizeof(buffer));
        if(bytesRead == -1)
        {
            perror("smash error: getdents64 failed");
            close(fd);
            return;
        }
        
        // read until no more entries
        if(bytesRead == 0)
            break;

        // iterate over devices entries
        for(int pos=0; pos<bytesRead;)
        {
            linux_dirent64* entry = reinterpret_cast<linux_dirent64*>(buffer+pos);
            std::string name(entry->d_name);
            pos += entry->d_reclen;

            // skip "." , ".." , and interfaces 
            if(name=="." || name==".." || name.find(':')!=std::string::npos){
                continue;
            }

            std::string base = std::string(usbPath)+"/"+name;
            UsbDeviceInfo device;

            std::string devnumStr = readDeviceParam(base+"/devnum");

            // if devnum is not available skip the device
            if(devnumStr=="N/A"){
                continue;
            }

            device.devnum = stoi(devnumStr);
            device.vendor = readDeviceParam(base+"/idVendor");
            device.productId = readDeviceParam(base+"/idProduct");
            device.manufacturer = readDeviceParam(base+"/manufacturer");
            device.product = readDeviceParam(base+"/product");
            device.power = readDeviceParam(base+"/bMaxPower");

            devices.push_back(device);
        }
    }

    close(fd);

    if(devices.empty())
    {
        std::cerr << "smash error: usbinfo: no USB devices found" << std::endl;
        return;
    }

    std::sort(devices.begin(), devices.end(), 
                [](const UsbDeviceInfo& a, const UsbDeviceInfo& b){
                    return a.devnum < b.devnum;
                });

    for(const auto& d : devices)
    {
        std::cout << "Device " << d.devnum << ": ID " << d.vendor << ":" << d.productId << " "
        << d.manufacturer << " " << d.product << " MaxPower: " << d.power << std::endl;
    }
}
