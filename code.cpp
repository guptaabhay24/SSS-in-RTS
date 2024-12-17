#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#define TOTAL_WIDTH 1200
#define TOTAL_HEIGHT 700
#define TOTAL_TASKS 2

#define TOTAL_TIME 30
#define WAIT_FACTOR 0.1f
#define LOOP_TILL (int)(TOTAL_TIME / WAIT_FACTOR)
#define INITAL_WAIT 1

#define CONTENT_START_X  60
#define CONTENT_START_Y  50
#define CONTENT_END_X  TOTAL_WIDTH-50
#define CONTENT_END_Y  TOTAL_HEIGHT - 50
#define processLineSeparationDis 100
#define totalTimeLineLength CONTENT_END_X - CONTENT_START_X
#define serverCapacityLabelDis TOTAL_HEIGHT/25

using namespace std;

static void* PeriodicTaskFunc(ALLEGRO_THREAD* thr, void* arg);
static void* CurrentTimeFunc(ALLEGRO_THREAD* thr, void* arg);
static void* ServerCapacityFunc(ALLEGRO_THREAD* thr, void* arg);
static void* AperiodicTaskFunc(ALLEGRO_THREAD* thr, void* arg);
static void* SchedularFunc(ALLEGRO_THREAD* thr, void* arg);
static void* addAperiodicFunc(ALLEGRO_THREAD* thr, void* arg);

float utilizationFactor=0.0f;

class Replenish {
public:
    float c;
    float t;
    Replenish(float com, float tim) {
        c = com;
        t = tim;
    }
};

class Task {
public:
    float c;
    float a;
    float d;
    float completed = 0;
    vector<float> excecuting;
    Task(float com, float arr, float dead) {
        c = com;
        a = arr;
        d = dead;
    }
};

class PeriodicTask {
public:
    float c;
    float t;
    float a;
    float d;
    bool isAquired;
    int pr;
    vector<Task> tasks;
    string name;
    vector<float> verLines;
    bool wantCPU = false;
    PeriodicTask() {}
    PeriodicTask(string na, float com, float ti, float arrival,
                 float deadline, int priority) {
        name = na;
        c = com;
        t = ti;
        a = arrival;
        d = deadline;
        pr = priority;
        isAquired = false;
    }
};

class AperiodicTask {
public:
    float c;
    float a;
    float completed = 0;
    vector<float> excecuting;
    float finishTime = -1;
    AperiodicTask(float com, float arrival) {
        c = com;
        a = arrival;
    }
};

class ServerCapacityCordinate {
public:
    float x;
    float y;
    ServerCapacityCordinate(float xCor, float yCor) {
        x = xCor;
        y = yCor;
    }
};

class DATA {

public:

    ALLEGRO_MUTEX* mutex;
    ALLEGRO_COND* cond;
    float          posiX;
    float          posiY;
    bool           modi_X;
    bool           ready;
    vector<PeriodicTask> threads;
    vector<AperiodicTask> aperiodicTask;
    vector<ServerCapacityCordinate> serverCapacityCor;
    float          cs;
    float          ts;
    int            ps; // priority of server
    bool           serverWantCPU = false;
    vector<float>   sVerLines;

    float          currentCapacity;
    float          currentTime = 0.0f; // Start from time 0
    int            currentExc;

    ALLEGRO_EVENT_QUEUE* event_queue;

    DATA() : mutex(al_create_mutex()),
             cond(al_create_cond()),
             posiX(0),
             posiY(0),
             modi_X(false),
             ready(false) {
        event_queue = al_create_event_queue();
        al_install_keyboard();
        al_register_event_source(event_queue,
                                 al_get_keyboard_event_source());
    }

    ~DATA() {
        al_destroy_mutex(mutex);
        al_destroy_cond(cond);
        al_destroy_event_queue(event_queue);
    }
};

class engine {
public:
    void drawLine(int x11, int y11, int x22, int y22, int thickness) {
        al_draw_line(min(x11, CONTENT_END_X), y11,
                     min(x22, CONTENT_END_X), y22,
                     al_map_rgb(0, 0, 0), thickness);
    }

    void drawVerLines(float x, float y) {
        al_draw_line(x - 5, y - 35, x, y - 40,
                     al_map_rgb(0, 0, 0), 2);
        al_draw_line(x + 5, y - 35, x, y - 40,
                     al_map_rgb(0, 0, 0), 2);
        al_draw_line(x, y, x, y - 40, al_map_rgb(0, 0, 0), 2);
    }

    void drawSererGraphPoint(ServerCapacityCordinate cor) {
        al_draw_line(min((int)cor.x - 1, CONTENT_END_X), cor.y - 1,
                     min((int)cor.x + 1, CONTENT_END_X), cor.y + 1,
                     al_map_rgb(0, 0, 0), 1);
    }

    void drawExcetuting(float x, float y, float height) {
        al_draw_line(x - 1, y, x + 1, y - height,
                     al_map_rgb(0, 0, 0), 1);
    }

    void drawTimeLine(float y11, float thickness) {
        drawLine(50, y11, TOTAL_WIDTH - 50, y11, thickness);
    }

    void drawAperiodicTaskTimeLine(float y) {
        al_draw_line(50, y, TOTAL_WIDTH - 50, y,
                     al_map_rgb(0, 0, 255), 3);
    }

    void drawServerCapacityTimeLine(float y) {
        al_draw_line(50, y, TOTAL_WIDTH - 50, y,
                     al_map_rgb(0, 255, 0), 2);
    }

    void drawServerStatusLine(float y) {
        al_draw_line(50, y, TOTAL_WIDTH - 50, y,
                     al_map_rgb(132, 111, 60), 2);
    }

    void drawProcessLine(float x) {
        drawLine(x, TOTAL_HEIGHT - 20, x, 100, 5);
    }

    void drawCurrentTimeLine(float x) {
        al_draw_line(min((int)x, CONTENT_END_X),
                     TOTAL_HEIGHT - 20,
                     min((int)x, CONTENT_END_X), 100,
                     al_map_rgb(255, 0, 0), 3);
    }

    void drawTimeLabelLine(float x, float y) {
        drawLine(x, y - 10, x, y + 10, 2);
    }

    void drawServerLabelLine(float y) {
        drawLine(50, y, 70, y, 2);
    }
};

int main() {
    ALLEGRO_DISPLAY* display = NULL;

    al_init();
    al_init_primitives_addon();
    al_init_font_addon(); // initialize the font addon

    display = al_create_display(TOTAL_WIDTH, TOTAL_HEIGHT);
    al_clear_to_color(al_map_rgb(255, 255, 255));
    if (!display) return -1;

    ALLEGRO_FONT* font = al_create_builtin_font();
    if (!font) {
        fprintf(stderr, "Could not create built-in font.\n");
        return -1;
    }

    al_clear_to_color(al_map_rgb(255, 255, 255));

    ALLEGRO_THREAD* thread_1 = NULL; // thread for periodic task 1
    ALLEGRO_THREAD* thread_2 = NULL; // thread for periodic task 2
    ALLEGRO_THREAD* thread_3 = NULL; // thread to show current time
    ALLEGRO_THREAD* thread_4 = NULL; // thread for server capacity
    ALLEGRO_THREAD* thread_5 = NULL; // thread for aperiodic tasks
    ALLEGRO_THREAD* thread_6 = NULL; // thread to schedule
    ALLEGRO_THREAD* thread_7 = NULL; // thread to add aperiodic tasks

    DATA data;
    
     // Read task data from file
    ifstream inputFile("tasks.txt");
    if (!inputFile) {
        std::cerr << "Error: Could not open input file.\n";
        return -1;
    }
    
    float hyperbolicBound=1.0;
    string name;
    int c, t, offset, deadline, priority;
    while (inputFile >> name >> c >> t >> offset >> deadline >> priority) {
        PeriodicTask task(name, c, t, offset, deadline, priority);
        data.threads.push_back(task);
        utilizationFactor += task.c/task.t;
        hyperbolicBound *= ((task.c/task.t)+1);
    }
    inputFile.close();
  
  
    if(utilizationFactor>1.00f){
      cout<<"Necessarily not schedulable";
      return 0;
    }
    else{
      if(utilizationFactor>2*((pow(2, 1/2.00f))-1)){
        cout<<"Definitely not schedulable by Liu-Layland Bound";
        return 0;
      }
      else{
        if(hyperbolicBound>2.00f){
          cout<<"Definitely not schedulable by Hyperbolic Bound";
          return 0;
        }
        else{
          cout<<"Utilization Factor: "<<utilizationFactor<<endl;
        }
      }
    }

    // Read server data from file
    ifstream serverFile("server.txt");
    if (!serverFile) {
        std::cerr << "Error: Could not open server file.\n";
        return -1;
    }

    serverFile >> data.cs >> data.ts >> data.ps;
    serverFile.close();

    data.currentCapacity = data.cs;
    data.currentExc = 0;

    thread_1 = al_create_thread(PeriodicTaskFunc, &data);
    thread_2 = al_create_thread(PeriodicTaskFunc, &data);
    thread_3 = al_create_thread(CurrentTimeFunc, &data);
    thread_4 = al_create_thread(ServerCapacityFunc, &data);
    thread_5 = al_create_thread(AperiodicTaskFunc, &data);
    thread_6 = al_create_thread(SchedularFunc, &data);
    thread_7 = al_create_thread(addAperiodicFunc, &data);
    al_start_thread(thread_1);
    al_start_thread(thread_2);
    al_start_thread(thread_3);
    al_start_thread(thread_4);
    al_start_thread(thread_5);
    al_start_thread(thread_6);
    al_start_thread(thread_7);
    engine e;

    // Calculate scaling factor
    float timeLabelDis = (CONTENT_END_X - CONTENT_START_X) / TOTAL_TIME;

    for (int i = 0; i < LOOP_TILL; i++) {
        al_clear_to_color(al_map_rgb(255, 255, 255));

        al_draw_text(font, al_map_rgb(0, 0, 0),
                     TOTAL_WIDTH / 2, CONTENT_START_Y,
                     ALLEGRO_ALIGN_CENTRE,
                     "Q) Simulate and graphically visualize "
                     "sporadic server scheduling algorithm for "
                     "the given task set.");

        al_draw_text(font, al_map_rgb(0, 0, 0),
                     TOTAL_WIDTH / 2, CONTENT_START_Y + 40,
                     ALLEGRO_ALIGN_CENTRE,
                     "Press keys '1' to '9' to add an aperiodic "
                     "task with corresponding C");

        // draw time line
        e.drawTimeLine(CONTENT_END_Y + 20, 5);
        e.drawProcessLine(CONTENT_START_X);

        // draw time labels
        for (int j = 0; j <= TOTAL_TIME; j++) {
            float xCor = CONTENT_START_X + j * timeLabelDis;
            float yCor = CONTENT_END_Y + 20;
            e.drawTimeLabelLine(xCor, yCor);
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "%d", j);
            al_draw_text(font, al_map_rgb(0, 0, 0),
                         xCor, yCor + 10, ALLEGRO_ALIGN_CENTRE,
                         buffer);
        }

        int serverStatusLineY = CONTENT_END_Y + 20 - (TOTAL_HEIGHT / 4);

        al_lock_mutex(data.mutex);
        // draw each process line
        for (size_t processCount = 0;
             processCount != data.threads.size();
             processCount++) {
            float yCor = CONTENT_END_Y + 20 - (TOTAL_HEIGHT / 2.5f)
                         - (processCount + 1) * processLineSeparationDis;
            e.drawTimeLine(yCor, 3);
            al_draw_text(font, al_map_rgb(0, 0, 0), 25,
                         yCor - 40, ALLEGRO_ALIGN_CENTRE,
                         processCount == 0 ? "P1" : "P0");

            // draw instance arrive ver lines
            for (size_t lineCount = 0;
                 lineCount != data.threads[processCount].verLines.size();
                 lineCount++) {
                e.drawVerLines(data.threads[processCount].verLines[lineCount],
                               yCor);
            }
            // draw executing
            for (size_t taskCount = 0;
                 taskCount != data.threads[processCount].tasks.size();
                 taskCount++) {
                for (size_t point = 0;
                     point != data.threads[processCount]
                                   .tasks[taskCount].excecuting.size();
                     point++) {
                    e.drawExcetuting(
                        data.threads[processCount].tasks[taskCount]
                            .excecuting[point],
                        yCor, 30);
                    // draw server status if this task priority is
                    // higher than server
                    if (data.threads[processCount].pr < data.ps) {
                        e.drawExcetuting(
                            data.threads[processCount].tasks[taskCount]
                                .excecuting[point],
                            serverStatusLineY, 15);
                    }
                }
            }
        }

        // draw aperiodic task line
        float yCorServerLine = CONTENT_END_Y + 20 - (TOTAL_HEIGHT / 2.5f);
        e.drawAperiodicTaskTimeLine(yCorServerLine);
        al_draw_text(font, al_map_rgb(0, 0, 0), 25,
                     yCorServerLine - 40, ALLEGRO_ALIGN_CENTRE,
                     "Aperiodic");
        al_draw_text(font, al_map_rgb(0, 0, 0), 25,
                     yCorServerLine - 20, ALLEGRO_ALIGN_CENTRE,
                     "requests");
        // draw instance arrive ver lines
        for (size_t lineCount = 0;
             lineCount != data.sVerLines.size();
             lineCount++) {
            e.drawVerLines(data.sVerLines[lineCount], yCorServerLine);
        }
        // draw executing
        for (size_t tasks = 0;
             tasks != data.aperiodicTask.size();
             tasks++) {
            for (size_t lineCount = 0;
                 lineCount != data.aperiodicTask[tasks]
                                  .excecuting.size();
                 lineCount++) {
                e.drawExcetuting(
                    data.aperiodicTask[tasks].excecuting[lineCount],
                    yCorServerLine, 30);
                e.drawExcetuting(
                    data.aperiodicTask[tasks].excecuting[lineCount],
                    serverStatusLineY, 15);
            }
        }

        // draw server status (active/idle) line
        e.drawServerStatusLine(serverStatusLineY);
        al_draw_text(font, al_map_rgb(0, 0, 0), 25,
                     serverStatusLineY - 20, ALLEGRO_ALIGN_CENTRE,
                     "SS active");

        // draw server capacity line
        e.drawServerCapacityTimeLine(CONTENT_END_Y + 20 - 3);

        // draw server capacity graph
        for (size_t point = 0;
             point != data.serverCapacityCor.size();
             point++) {
            e.drawSererGraphPoint(data.serverCapacityCor[point]);
        }

        // draw server capacity label
        for (int j = 0; j <= 5; j++) {
            float serverCapLableYCor = CONTENT_END_Y + 20 - (j)*serverCapacityLabelDis;
            e.drawServerLabelLine(serverCapLableYCor);
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "%d", j);
            al_draw_text(font, al_map_rgb(0, 0, 0), 25,
                         serverCapLableYCor - 5, ALLEGRO_ALIGN_CENTRE,
                         buffer);
        }

        // move current time line
        float currentTimePosition = CONTENT_START_X + data.currentTime * timeLabelDis;
        e.drawCurrentTimeLine(currentTimePosition);

        al_unlock_mutex(data.mutex);

        if (i == 0) {
            // wait before eyes are setup
            al_flip_display();
            al_rest(INITAL_WAIT);
        }

        al_flip_display();
        al_rest(WAIT_FACTOR);
    }

    al_destroy_thread(thread_1);
    al_destroy_thread(thread_2);
    al_destroy_thread(thread_3);
    al_destroy_thread(thread_4);
    al_destroy_thread(thread_5);
    al_destroy_thread(thread_6);
    al_destroy_thread(thread_7);
    al_destroy_display(display);
    return 0;
}

static void* addAperiodicFunc(ALLEGRO_THREAD* thr, void* arg) {
    DATA* data = (DATA*)arg;

    while (!al_get_thread_should_stop(thr)) {
        ALLEGRO_EVENT ev;
        while (al_get_next_event(data->event_queue, &ev)) {
            if (ev.type == ALLEGRO_EVENT_KEY_CHAR) {
                al_lock_mutex(data->mutex);
                float currentTime = data->currentTime;
                char key_char = ev.keyboard.unichar;
                if ((key_char >= '1' && key_char <= '9') ||
                    (ev.keyboard.keycode >= ALLEGRO_KEY_PAD_1 &&
                     ev.keyboard.keycode <= ALLEGRO_KEY_PAD_9)) {
                    int computationTime;
                    if (key_char >= '1' && key_char <= '9') {
                        computationTime = key_char - '0';
                    } else {
                        computationTime = ev.keyboard.keycode - ALLEGRO_KEY_PAD_0;
                    }
                    AperiodicTask aperiodicTask1 = AperiodicTask(
                        computationTime, currentTime);
                    data->aperiodicTask.push_back(aperiodicTask1);

                    std::cout << "Aperiodic Task: new instance arrived c:"
                              << computationTime << ", arrived at:"
                              << currentTime << "\n";
                    float x_position = CONTENT_START_X + currentTime *
                                       (CONTENT_END_X - CONTENT_START_X) /
                                       TOTAL_TIME;
                    data->sVerLines.push_back(x_position);

                    // Mark want CPU true
                    data->serverWantCPU = true;
                }
                al_unlock_mutex(data->mutex);
            }
        }

        al_rest(0.01); // Sleep for a short duration to prevent busy waiting
    }

    return NULL;
}

static void* CurrentTimeFunc(ALLEGRO_THREAD* thr, void* arg) {

    DATA* data = (DATA*)arg;

    for (int i = 0; i < LOOP_TILL; i++) {
        if (i == 0) {
            al_rest(INITAL_WAIT);
        }

        al_lock_mutex(data->mutex);
        float simulationTime = i * WAIT_FACTOR;
        data->currentTime = simulationTime;
        al_unlock_mutex(data->mutex);
        al_rest(WAIT_FACTOR);
    }

    return NULL;
}

static void* ServerCapacityFunc(ALLEGRO_THREAD* thr, void* arg) {

    DATA* data = (DATA*)arg;

    for (int i = 0; i < LOOP_TILL; i++) {
        if (i == 0) {
            al_rest(INITAL_WAIT);
        }

        al_lock_mutex(data->mutex);

        float simulationTime = i * WAIT_FACTOR;

        float x_position = CONTENT_START_X + simulationTime * (CONTENT_END_X - CONTENT_START_X) / TOTAL_TIME;

        data->serverCapacityCor.push_back(
            ServerCapacityCordinate(x_position, CONTENT_END_Y + 20 - (data->currentCapacity) * serverCapacityLabelDis));

        al_unlock_mutex(data->mutex);

        al_rest(WAIT_FACTOR);
    }

    return NULL;
}

static void* SchedularFunc(ALLEGRO_THREAD* thr, void* arg) {

    DATA* data = (DATA*)arg;

    al_rest(INITAL_WAIT);

    while (!al_get_thread_should_stop(thr)) {

        al_lock_mutex(data->mutex);
        int newPrio = 99;

        // Check for tasks wanting CPU
        for (size_t taskCount = 0;
            taskCount != data->threads.size();
            taskCount++) {
            if (data->threads[taskCount].wantCPU && data->threads[taskCount].pr < newPrio) {
                newPrio = data->threads[taskCount].pr;
            }
        }

        // Check server
        if (data->serverWantCPU && data->ps < newPrio) {
            newPrio = data->ps;
        }

        // Update current executing priority
        if (newPrio != data->currentExc) {
            data->currentExc = newPrio;
        }

        al_unlock_mutex(data->mutex);
        al_rest(WAIT_FACTOR);
    }

    return NULL;
}

static void* AperiodicTaskFunc(ALLEGRO_THREAD* thr, void* arg) {
    DATA* data = (DATA*)arg;
    vector<Replenish> replenish;

    for (int i = 0; i < LOOP_TILL; i++) {
        if (i == 0) {
            al_rest(INITAL_WAIT);
        }

        al_lock_mutex(data->mutex);
        float simulationTime = i * WAIT_FACTOR;

        // Check for replenishments
        for (size_t index = 0; index < replenish.size(); index++) {
            if (replenish[index].t <= simulationTime) {
                data->currentCapacity += replenish[index].c;
                replenish.erase(replenish.begin() + index);
                index--;
            }
        }

        // Handle aperiodic tasks
        for (size_t taskCount = 0; taskCount < data->aperiodicTask.size(); taskCount++) {
            AperiodicTask& task = data->aperiodicTask[taskCount];

            if (simulationTime >= task.a && task.completed < task.c) {

                if (data->currentExc == data->ps && data->currentCapacity > 0) {
                    // Schedule replenishment when task starts executing
                    if (task.completed == 0) {
                        replenish.push_back(Replenish(task.c, simulationTime + data->ts));
                    }

                    // Execute aperiodic task
                    task.completed += WAIT_FACTOR;
                    data->currentCapacity -= WAIT_FACTOR;

                    float x_execution = CONTENT_START_X + simulationTime * (CONTENT_END_X - CONTENT_START_X) / TOTAL_TIME;
                    task.excecuting.push_back(x_execution);

                    // Check if task has finished execution
                    if (task.completed >= task.c) {
                        task.finishTime = simulationTime;
                        float responseTime = task.finishTime - task.a;
                        std::cout << "Aperiodic Task finished execution at time: " << task.finishTime
                                  << ", response time: " << responseTime << "\n";
                    }

                } else {
                    // Request CPU if not already requested
                    data->serverWantCPU = true;
                }
            }
        }

        // Release CPU if no tasks are pending
        bool pendingAperiodic = false;
        for (const auto& task : data->aperiodicTask) {
            if (task.completed < task.c && simulationTime >= task.a) {
                pendingAperiodic = true;
                break;
            }
        }
        if (!pendingAperiodic) {
            data->serverWantCPU = false;
        }

        al_unlock_mutex(data->mutex);
        al_rest(WAIT_FACTOR);
    }

    return NULL;
}

static void* PeriodicTaskFunc(ALLEGRO_THREAD* thr, void* arg) {

    DATA* data = (DATA*)arg;

    // get unacquired thread
    PeriodicTask threadData;
    int taskIndex = -1;
    for (size_t processCount = 0;
        processCount != data->threads.size();
        processCount++) {
        if (!data->threads[processCount].isAquired) {
            al_lock_mutex(data->mutex);
            data->threads[processCount].isAquired = true;
            al_unlock_mutex(data->mutex);
            threadData = data->threads[processCount];
            taskIndex = processCount;
            break;
        }
    }

    if (taskIndex == -1) {
        printf("No task found for this thread, returning\n");
        return NULL;
    }

    int doneWithSec = -1;
    for (int i = 0; i < LOOP_TILL; i++) {
        if (i == 0) {
            al_rest(INITAL_WAIT);
        }

        al_lock_mutex(data->mutex);

        float simulationTime = i * WAIT_FACTOR;
        int currentTime = (int)simulationTime;

        if (doneWithSec != currentTime) {
            if (currentTime % (int)threadData.t == threadData.a) {
                // new instance arrived
                // push to queue
                data->threads[taskIndex].tasks
                    .push_back(Task(threadData.c, simulationTime, threadData.d + simulationTime));
                float x_position = CONTENT_START_X + simulationTime * (CONTENT_END_X - CONTENT_START_X) / TOTAL_TIME;
                data->threads[taskIndex].verLines
                    .push_back(x_position);
                data->threads[taskIndex].wantCPU = true;
                
                if(utilizationFactor>1){
                  continue;
                }
                cout << threadData.name << ": new instance arrived c:" << threadData.c << ", arrived at:" << simulationTime << ", absolute deadline:" << threadData.d + simulationTime << "\n";

            }
        }

        int temp = 0;
        for (size_t instanceCount = 0;
            instanceCount != data->threads[taskIndex].tasks.size();
            instanceCount++) {
            if (simulationTime < data->threads[taskIndex].tasks[instanceCount].a) {
                // do not consider future instances
                continue;
            }

            // if task not completed execute it
            if (data->threads[taskIndex].tasks[instanceCount].c - data->threads[taskIndex].tasks[instanceCount].completed > 0.05
                && simulationTime >= data->threads[taskIndex].tasks[instanceCount].a) {
                temp++;
                if (data->currentExc == data->threads[taskIndex].pr) {
                    // execute task
                    data->threads[taskIndex].tasks[instanceCount].completed += WAIT_FACTOR;
                    float x_execution = CONTENT_START_X + simulationTime * (CONTENT_END_X - CONTENT_START_X) / TOTAL_TIME;
                    data->threads[taskIndex].tasks[instanceCount].excecuting.push_back(x_execution);
                }
                else {
                    // request for CPU if already not requested
                    data->threads[taskIndex].wantCPU = true;
                }
            }
        }
        if (temp == 0) {
            data->threads[taskIndex].wantCPU = false;
        }
        al_unlock_mutex(data->mutex);
        al_rest(WAIT_FACTOR);
        doneWithSec = currentTime;
    }

  return NULL;
}
