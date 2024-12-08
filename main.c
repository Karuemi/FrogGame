#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/time.h>
#include <math.h>

enum TIMER {
	TIMER_PRECISION = (int)1e6,
    FPS = 60,
    TPS = 20
};

enum MAP {
	WIDTH = 60,
	HEIGHT = 30,
	STATUS_HEIGHT = 15 // in percents
};

enum TILE {
	NO_TILE,
	CAR_TILE,
	FROG_TILE,
	STREET_TILE
};

typedef struct {
	WINDOW* val;
	const int W;
	const int H;
} Window;

typedef struct {
	uint64_t start;
	uint64_t lastLoop;
	uint64_t sinceLastFrame;
	uint64_t sinceLastTick;
	uint64_t sinceStart;

	uint8_t renderFrame;
	uint8_t updateTick;
} Timer;

typedef struct {
	double timePassed;
} StatusInfo;

typedef struct {
	enum TILE** v;
} Map;

/// INITIALIZATION

Window initWindow(const int H, const int W, const int offsetX, const int offsetY) {
	WINDOW* cursesWindow = newwin(H, W, (LINES - H) / 2 + offsetY, (COLS - W) / 2 + offsetX);

	wborder(cursesWindow, '|', '|', '-', '-', '+', '+', '+', '+');
	wrefresh(cursesWindow);

	Window window = {cursesWindow, W, H};
	return window;
}

// TODO 

void splashScreen(WINDOW* win) {
	mvwaddstr(win, 1, 1, "Do you want to play a game?");
	mvwaddstr(win, 2, 1, "Press any key to continue..");
	wgetch(win);
	wclear(win);
	wrefresh(win);
}

/// TIMER

uint64_t getCurrentTime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * TIMER_PRECISION + currentTime.tv_usec;
}

Timer* initTimer() {
	Timer* timer = (Timer*)malloc(sizeof(Timer));

	timer->start = getCurrentTime();
	timer->lastLoop = getCurrentTime();
	timer->sinceLastFrame = 0;
	timer->sinceLastTick = 0;

	return timer;
}

void updateTimer(Timer* timer) {
	uint64_t now = getCurrentTime();

	uint64_t passed = now - timer->lastLoop;
	timer->sinceLastFrame += passed;
	timer->sinceLastTick += passed;
	timer->sinceStart = now - timer->start;

	if (timer->sinceLastTick >= (1.0f / (double)TPS) * TIMER_PRECISION) {
		timer->sinceLastTick -= (1.0f / (double)TPS) * TIMER_PRECISION;
		timer->updateTick = 1;
	}

	if (timer->sinceLastFrame >= (1.0f / (double)FPS) * TIMER_PRECISION) {
		timer->sinceLastFrame -= (1.0f / (double)FPS) * TIMER_PRECISION;
		timer->renderFrame = 1;
	}

	timer->lastLoop = now;
}

/// INIT MAP

Map* allocMap() {
	Map* map = NULL;

	map->v = (enum TILE**)malloc(sizeof(enum TILE*) * HEIGHT);

	for (int i = 0; i < HEIGHT; ++i) {
		map->v[i] = (enum TILE*)malloc(sizeof(enum TILE) * WIDTH);
	}

	return map;
}

void deallocMap(Map* map) {
	for (int i = 0; i < HEIGHT; ++i) {
		free(map->v[i]);
	}
	free(map->v);
	map->v = NULL;
}

Map* initMap() {
	Map* map = allocMap();

	for (int i = 0; i < HEIGHT; ++i) {
		for (int j = 0; j < WIDTH; ++j) {
			map->v[i][j] = NO_TILE;
		}
	}

	return map;
}

/// UPDATE GAME

void updateTick() {

}

/// RENDER

void renderMainWindow(Window* mainWindow) {
	wborder(mainWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');
	wrefresh(mainWindow->val);
}

void renderStatusWindow(Window* statusWindow, StatusInfo* info) {
	wborder(statusWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');
	mvwprintw(statusWindow->val, statusWindow->H / 2.0f, 2, "Time: %.02lf", info->timePassed);
	wrefresh(statusWindow->val);
}

void renderFrame(Window* mainWindow, Window* statusWindow, StatusInfo* statusInfo) {
	renderMainWindow(mainWindow);
	renderStatusWindow(statusWindow, statusInfo);
	refresh();
}

void updateStatusInfo(StatusInfo* info, Timer* timer) {
	info->timePassed = timer->sinceStart / (double)TIMER_PRECISION;
}

/// FILE

// void getData(struct Config* config) {
// 	FILE* file = fopen("data.txt", "r");

// 	char buffer[1024];

// 	while (!feof(file)) {
// 		fscanf(file, "%s", buffer);

// 		if (strcmp(buffer, "frog_size")) {
// 			fscanf(file, "%d", &config->frog_size);
// 		}
// 		if (strcmp(buffer, "height")) {
// 			fscanf(file, "%d", &config->height);
// 		}
// 		if (strcmp(buffer, "width")) {
// 			fscanf(file, "%d", &config->width);
// 		}
// 	}
	
// 	fclose(file);
// }

/// MAIN LOOP

void runGame() {
    Timer* timer = initTimer();
    u_int8_t continueLoop = true;

	Window mainWindow = initWindow(HEIGHT, WIDTH, 0, 0);

	const int WINDOW_STATUS_HEIGHT = ceil(HEIGHT * STATUS_HEIGHT * 0.01f);
	const int WINDOW_STATUS_OFFSETY = ceil((HEIGHT / 2.0f) * (100 - STATUS_HEIGHT) * 0.01f);
	Window statusWindow = initWindow(WINDOW_STATUS_HEIGHT, WIDTH, 0, WINDOW_STATUS_OFFSETY);
	StatusInfo* statusInfo = (StatusInfo*)malloc(sizeof(StatusInfo));

    while (continueLoop) {
		updateTimer(timer);
		updateStatusInfo(statusInfo, timer);

		if (timer->updateTick) {
			timer->updateTick = 0;
			updateTick();
		}

		if (timer->renderFrame) {
			timer->renderFrame = 0;
			renderFrame(&mainWindow, &statusWindow, statusInfo);
		}
    }

	free(statusInfo);
	free(timer);

	delwin(mainWindow.val);
	delwin(statusWindow.val);
}


int main() {
	initscr();
	//setlocale(LC_ALL, "");
	start_color();
    noecho();
	cbreak();

    //splashScreen(gameWindow);
	runGame();

    endwin();

    return 0;
}