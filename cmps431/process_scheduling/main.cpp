#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <unistd.h>

using namespace std;

#define TEXT_BLACK "\033[30m"
#define TEXT_RED "\033[31m"
#define TEXT_GREEN "\033[32m"
#define TEXT_YELLOW "\033[33m"
const string loadingBars[] = {"|", "/", "─", "\\"};

struct PCB
{
    int pId;
    int arrival;
    int cpuBurst;
    int initialCpuBurst;
    int priority;
    vector<int> starts;
    vector<int> ends;
    int getStart()
    {
        return starts.empty() ? -1 : starts.front();
    }
    int getCompletion()
    {
        return ends.empty() ? -1 : ends.back();
    }
};
vector<PCB *> newQ;
vector<PCB *> ready;
vector<PCB *> run;
vector<PCB *> wait;
vector<PCB *> terminated;

vector<PCB *> getAllProcesses()
{
    vector<PCB *> allProcesses;
    allProcesses.insert(allProcesses.end(), newQ.begin(), newQ.end());
    allProcesses.insert(allProcesses.end(), ready.begin(), ready.end());
    allProcesses.insert(allProcesses.end(), run.begin(), run.end());
    allProcesses.insert(allProcesses.end(), terminated.begin(), terminated.end());

    sort(
        allProcesses.begin(), allProcesses.end(), [](PCB *a, PCB *b)
        { return a->pId < b->pId; });
    return allProcesses;
}

void printTable(string title, int time)
{
    vector<PCB *> allProcesses = getAllProcesses();

    cout << "\033[2J\033[0;0H"
         << title << "\n"
         << "Process Id\tCPU Bursts\tArrival\tStart\tCompletion\tStatus" << endl;

    for (auto job : allProcesses)
    {
        string status;
        if (count(newQ.begin(), newQ.end(), job) > 0)
        {
            cout << TEXT_BLACK;
            status = "◦ Created";
        }
        else if (count(ready.begin(), ready.end(), job) > 0)
        {
            cout << TEXT_RED;
            status = "• Ready";
        }
        else if (count(run.begin(), run.end(), job) > 0)
        {
            cout << TEXT_YELLOW;
            status = loadingBars[time % 4] + " Running";
        }
        else if (count(terminated.begin(), terminated.end(), job) > 0)
        {
            cout << TEXT_GREEN;
            status = "✓ Complete";
        }
        cout << job->pId
             << "\t\t" << job->cpuBurst << "/" << job->initialCpuBurst
             << "\t\t" << job->arrival
             << "\t" << (job->getStart() == -1 ? "X" : to_string(job->getStart()))
             << "\t" << (job->getCompletion() == -1 ? "X" : to_string(job->getCompletion()))
             << "\t\t" << status
             << "\033[0m"
             << endl;
    }
    cout << "\n";
}

void drawGraph(int time, int maxTime)
{
    vector<PCB *> allProcesses = getAllProcesses();
    int t;

    // Draw top border
    cout << "╔";
    for (int i = 0; i < maxTime * 2; i++)
    {
        cout << "═";
    }
    cout << "╗\n\n";

    // Draw bottom border
    cout << "╚";
    for (int i = 0; i < maxTime * 2; i++)
    {
        cout << "═";
    }
    cout << "╝"
         << "\033[1F";

    cout << "║";
    // Draw blocks
    for (t = 0; t < maxTime; t++)
    {

        string block = t == time
                           ? "██"
                       : t > time
                           ? "░░"
                           : "▓▓";
        cout << block;
    }
    // Draw overlayed stuff
    cout << "\033[2G";
    for (t = 0; t < maxTime; t++)
    {
        vector<PCB *> arrivingProcesses;
        PCB *startingProcess = nullptr;
        bool processEnded = false;
        for (auto job : allProcesses)
        {
            if (job->arrival == t)
            {
                arrivingProcesses.push_back(job);
            }
            if (startingProcess == nullptr && count(job->starts.begin(), job->starts.end(), t) > 0)
            {
                startingProcess = job;
            }
            if (!processEnded && count(job->ends.begin(), job->ends.end(), t) > 0)
            {
                processEnded = true;
            }
        }
        if (!arrivingProcesses.empty())
        {
            cout << "\033[s\033[1B│\033[1D";
            for (auto job : arrivingProcesses)
            {
                string pIdString = to_string(job->pId);
                cout << "\033[1B" << pIdString << "\033[" << pIdString.length() << "D";
            }
            cout << "\033[u";
        }
        if (startingProcess != nullptr)
        {
            string pIdString = to_string(startingProcess->pId);
            cout << pIdString << "\033[" << pIdString.length() << "D";
        }
        if (processEnded)
        {
            cout << "\033[1D║\033[1D\033[1A╦\033[2B\033[1D╩\033[1A" << flush;
        }
        cout << "\033[2C";
    }

    cout << "║";

    cout << "\n\n"
         << endl;
}

bool arrivesFirst(PCB *a, PCB *b)
{
    return a->arrival < b->arrival;
}

int loadJobs(char *filename)
{
    FILE *jobs;
    char string[80];
    int pId, arrival, cpuBurst, priority;
    int j;
    int nJobs;
    /* Open file of jobs to be put in the ready que. */
    jobs = fopen(filename, "r");
    /* Load the ready que from the file. */
    fgets(string, 80, jobs);
    j = 0;
    while (fscanf(jobs, "%d %d %d %d", &pId, &arrival, &cpuBurst,
                  &priority) != EOF)
    {
        newQ.push_back(new PCB());
        newQ[j]->pId = pId;
        newQ[j]->arrival = arrival;
        newQ[j]->cpuBurst = cpuBurst;
        newQ[j]->initialCpuBurst = cpuBurst;
        newQ[j]->priority = priority;
        j = j + 1;
    }
    // Sort new queue by arrival times
    sort(newQ.begin(), newQ.end(), arrivesFirst);
    nJobs = j;
    fclose(jobs);

    return nJobs;
}

void showRunStats(int time)
{
    int numJobs = terminated.size();
    float tPut, turn, resp;
    /* Initialize variables. */
    turn = 0.0f;
    resp = 0.0f;
    /* Calculate turnaround time and waiting time. In this
    calculation the waiting time is really should be called response
    time. */
    for (auto job : terminated)
    {
        turn = turn + (job->getCompletion() - job->arrival);
        resp = resp + (job->getStart() - job->arrival);
    }
    /* Calculate averge values. */
    turn = turn / (float)numJobs;
    resp = resp / (float)numJobs;
    /* Show stats to user. */
    printf("\n");
    printf("Run Stats \n");
    /* Throughput. */
    tPut = (float)numJobs / (float)time;
    printf("Throughput = %.2f \n", tPut);
    /* Turnaround time. */
    printf("Average turnaround time = %.2f \n", turn);
    /* Waiting time. */
    printf("Average response time = %.2f \n", resp);
}

void clearQueue(vector<PCB *> *queue)
{
    for (auto pcb : *queue)
    {
        delete pcb;
    }
    queue->clear();
}

void clearQueues()
{
    clearQueue(&newQ);
    clearQueue(&ready);
    clearQueue(&run);
    clearQueue(&wait);
    clearQueue(&terminated);
}

int FCFS()
{
    int time = 0;
    int maxTime = 0;
    for (auto job : newQ)
    {
        maxTime += job->cpuBurst;
    }

    int nJobs = newQ.size();
    vector<PCB *>::iterator j;

    while (terminated.size() < nJobs)
    {
        // Move arriving processes to ready queue
        j = newQ.begin();
        while (j != newQ.end())
        {
            if ((**j).arrival <= time)
            {
                ready.push_back(*j);
                j = newQ.erase(j);
            }
            else
            {
                j++;
            }
        }

        if (ready.empty() && run.empty())
        {
            time++;
            continue;
        }

        if (run.empty())
        {
            // Move the first ready process to run queue
            j = ready.begin();
            (**j).starts.push_back(time);
            run.push_back(*j);
            ready.erase(j);
        }

        // Run process in the run queue until it is complete
        j = run.begin();
        auto job = *j;

        time++;
        job->cpuBurst--;

        if (job->cpuBurst == 0)
        {
            job->ends.push_back(time);
            terminated.push_back(job);
            run.erase(j);
        }
        printTable("First Come, First Serve", time);
        drawGraph(time, maxTime);
        usleep(100000);
    }

    return time;
}

bool shorter(PCB *a, PCB *b)
{
    return a->cpuBurst < b->cpuBurst;
}

int ShortestJobFirst()
{
    int time = 0;
    int maxTime = 0;
    for (auto job : newQ)
    {
        maxTime += job->cpuBurst;
    }

    int nJobs = newQ.size();
    vector<PCB *>::iterator j;

    while (terminated.size() < nJobs)
    {
        // Move arriving processes to ready queue
        j = newQ.begin();
        while (j != newQ.end())
        {
            if ((**j).arrival <= time)
            {
                ready.push_back(*j);
                j = newQ.erase(j);
            }
            else
            {
                j++;
            }
        }

        if (ready.empty() && run.empty())
        {
            time++;
            continue;
        }

        if (run.empty())
        {
            // Move the shortest ready process to the run queue
            j = min_element(ready.begin(), ready.end(), shorter);
            (**j).starts.push_back(time);
            run.push_back(*j);
            j = ready.erase(j);
        }

        // Run process in the run queue until it is complete
        j = run.begin();
        auto job = *j;

        time++;
        job->cpuBurst--;

        if (job->cpuBurst == 0)
        {
            job->ends.push_back(time);
            terminated.push_back(job);
            run.erase(j);
        }
        printTable("Shortest Job First", time);
        drawGraph(time, maxTime);
        usleep(100000);
    }

    return time;
}

int RoundRobin(int timeQ)
{
    int time = 0;
    int runtime = 0;
    int maxTime = 0;
    for (auto job : newQ)
    {
        maxTime += job->cpuBurst;
    }

    int nJobs = newQ.size();
    vector<PCB *>::iterator j;

    while (terminated.size() < nJobs)
    {
        // Move arriving processes to ready queue
        j = newQ.begin();
        while (j != newQ.end())
        {
            if ((**j).arrival <= time)
            {
                ready.push_back(*j);
                j = newQ.erase(j);
            }
            else
            {
                j++;
            }
        }

        // If there was a process that exceeded the time quanta, move it to the back of the ready queue
        if (!run.empty() && runtime >= timeQ)
        {
            j = run.begin();
            (**j).ends.push_back(time);
            ready.push_back(*j);
            run.erase(j);
        }

        // Sleep if there are no processes to run
        if (ready.empty() && run.empty())
        {
            time++;
            continue;
        }

        // Move the first ready process to run queue
        if (run.empty())
        {
            j = ready.begin();
            (**j).starts.push_back(time);
            run.push_back(*j);
            ready.erase(j);
            runtime = 0;
        }

        // Run process in the run queue
        j = run.begin();
        auto job = *j;

        time++;
        runtime++;
        job->cpuBurst--;

        if (job->cpuBurst == 0)
        {
            job->ends.push_back(time);
            terminated.push_back(job);
            run.erase(j);
        }
        printTable("Round Robin", time);
        drawGraph(time, maxTime);
        usleep(100000);
    }

    return time;
}

void scrollscreen(int amount)
{
    cout << "\033[99E";

    for (int j = 0; j < amount; j++)
    {
        cout << endl;
        usleep(50000);
    }
}

int main()
{
    char jobsFile[60] = "./jobs.txt";
    int timeOfLastRun;
    int nJobs;

    nJobs = loadJobs(jobsFile);
    timeOfLastRun = FCFS();
    showRunStats(timeOfLastRun);
    clearQueues();

    scrollscreen(13 + nJobs);

    nJobs = loadJobs(jobsFile);
    timeOfLastRun = ShortestJobFirst();
    showRunStats(timeOfLastRun);
    clearQueues();

    scrollscreen(13 + nJobs);

    loadJobs(jobsFile);
    timeOfLastRun = RoundRobin(15);
    showRunStats(timeOfLastRun);
    clearQueues();

    return 0;
}