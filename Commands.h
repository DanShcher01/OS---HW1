// Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <string.h>
#include <sys/wait.h>
#include <map>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {

protected:
    char *args[COMMAND_MAX_ARGS];
    int argsNum;

public:
    pid_t pid;
    char* cmd_line;
    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();

};
class JobsList {
public:
    class JobEntry {
    public:
        int jobId;
        char *cmd_line;
        pid_t pid;
        bool isStopped;
        JobEntry(int jobId, char *cmd_line, pid_t pid, bool isStopped = false)
                : jobId(jobId), cmd_line(cmd_line), pid(pid), isStopped(isStopped) {}
        ~JobEntry() {
            free(cmd_line);
        }
    };
    std::vector<JobEntry*> jobs;


public:
    JobsList();

    ~JobsList();

    void addJob(Command *cmd, bool isStopped = false){
        int jobId = jobs.empty() ? 1 : jobs.back()->jobId + 1;
        char *cmd_line = strdup(cmd->cmd_line);
        jobs.push_back(new JobEntry(jobId, cmd_line, cmd->pid, isStopped));
    }

    void printJobsList(){
        for(auto it = jobs.begin(); it != jobs.end();){
            if(!(*it)->isStopped){
                std::cout << "[" << (*it)->jobId << "] " << (*it)->cmd_line << std::endl;
            }
            ++it;
        }
    }

    void killAllJobs() {
        for (auto job : jobs) {
            if (kill(job->pid, SIGKILL) == -1) {
                perror("smash error: kill failed");
            }
            std::cout << job->pid << ": " << job->cmd_line << std::endl;
        }
    }

    int getJobCount() const {
        return jobs.size();
    }

    void removeFinishedJobs(){
        for (auto it = jobs.begin(); it != jobs.end();) {
            int status;
            pid_t result = waitpid((*it)->pid, &status, WNOHANG);
            if(result > 0 || result == -1) {
                //0 means the process is still running
                //-1 means the process does not exist anymore
                delete *it;
                it = jobs.erase(it);
            } else {
                ++it;
            }
        }
    }

    JobEntry *getJobById(int jobId){
        for (auto job : jobs) {
            if (job->jobId == jobId) {
                return job;
            }
        }
        return nullptr;
    }

    void removeJobById(int jobId){
        for (auto it = jobs.begin(); it != jobs.end();) {
            if((*it)->jobId == jobId) {
                delete *it;
                it = jobs.erase(it);
            } else {
                ++it;
            }
        }
    }

    JobEntry *getLastJob(int *lastJobId){
        if (jobs.empty()) {
            return nullptr;
        }
        *lastJobId = jobs.back()->jobId;
        return jobs.back();
    }

    JobEntry *getLastStoppedJob(int *jobId){
        for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
            if ((*it)->isStopped) {
                *jobId = (*it)->jobId;
                return *it;
            }
        }
        return nullptr;
    }

};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);
    virtual ~BuiltInCommand();
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};


class RedirectionCommand : public Command {

public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {

public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
    long long calculateBlocks(const char *path);
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class USBInfoCommand : public Command {
    std::string readDeviceParam(const std::string& path);
public:
    USBInfoCommand(const char *cmd_line);

    virtual ~USBInfoCommand() {
    }

    void execute() override;
};

class ChangePromptCommand : public BuiltInCommand {
public:
    ChangePromptCommand(const char *cmd_line);

    virtual ~ChangePromptCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {

    char **plastPwd; //last used directory
public:
    ChangeDirCommand(const char *cmd_line);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};



class QuitCommand : public BuiltInCommand {

public:
    QuitCommand(const char *cmd_line);

    virtual ~QuitCommand() {
    }

    void execute() override;
};


class JobsCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
public:
    KillCommand(const char *cmd_line);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
public:
    ForegroundCommand(const char *cmd_line);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line);

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};

class SmallShell {
private:
    // smash jobs list
    JobsList *jobs;

    // foreground process
    pid_t fg_pid;

    //last working directory
    char *lastPwd;

    //prompt string
    std::string prompt;

    // alias list
    std::vector<std::pair<std::string, std::string>> aliases;

    // map of built in commands
    static const std::map<std::string, Command*(*)(const char*)> cmd_map;

    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);
    
    void setFgPid(pid_t pid) { fg_pid = pid; }
    pid_t getFgPid() const { return fg_pid; }
    void setLastPwd(char *pwd) { lastPwd = pwd; }
    char *getLastPwd() const { return lastPwd; }
    JobsList* getJobs() const { return jobs; }
    const std::string &getPrompt() const {return prompt;}
    void setPrompt(const std::string &newPrompt){ prompt = newPrompt;}
    std::vector<std::pair<std::string, std::string>> &getAliases(){ return aliases; }
    static const std::map<std::string, Command*(*)(const char*)> &getCmdMap(){return cmd_map;};
};

#endif //SMASH_COMMAND_H_
