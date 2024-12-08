#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>

enum TIMER {
	TIMER_PRECISION = (int)1e6,
    FPS = 60,
    TPS = 5
};

enum MAP {
	WIDTH = 15,
	HEIGHT = 10,
	STATUS_HEIGHT = 10,
	INIT_CARS = 5,
	MAX_CARS = 64,
	BUFFER_SIZE = 128
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
	double x;
	double y;
	int w;
	int h;
	double speed;
	char** shape;
} Frog;

typedef struct {
	char** shape;
	double x;
	double y;
	int w;
	int h;
} Car;

typedef struct {
	Frog frog;
	Car cars[MAX_CARS];
	int carsAmount;
} Objects;

typedef struct {
	int w;
	int h;
} Size;

typedef struct {
	char** v;
	Objects objects;
	int renderW;
	int renderH;
} Map;

/// INITIALIZATION

Window initWindow(const int WHOLE_HEIGHT, const int H, const int W, const int offsetX, const int offsetY) {
	WINDOW* cursesWindow = newwin(H, W, (LINES - WHOLE_HEIGHT) / 2 + offsetY, (COLS - (W - 2)) / 2 + offsetX);

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

Map* allocMap(Size* size) {
	Map* map = (Map*)malloc(sizeof(Map));

	map->v = (char**)malloc(sizeof(char*) * size->h);

	for (int i = 0; i < size->h; ++i) {
		map->v[i] = (char*)malloc(sizeof(char) * size->w);
	}

	return map;
}

void deallocShape(char** in, const int H) {
	for (int i = 0; i < H; ++i) {
		free(in[i]);
	}

	free(in);
}

void deallocObjects(Objects* objects) {
	deallocShape(objects->frog.shape, objects->frog.h);
	
	for (int i = 0; i < objects->carsAmount; ++i) {
		deallocShape(objects->cars[i].shape, objects->cars[i].h);
	}
}

void deallocMap(Map* map) {
	deallocObjects(&map->objects);
	for (int i = 0; i < map->renderH; ++i) {
		free(map->v[i]);
	}
	free(map->v);
	free(map);
}

char** allocShape(const int W, const int H) {
	char** out = (char**)malloc(sizeof(char*) * H);

	for (int i = 0; i < H; ++i) {
		out[i] = (char*)malloc(sizeof(char) * W);
	}

	return out;
}

void initShapes(Objects* objects) {
	FILE* file = fopen("data.csv", "r");
	char buffer[BUFFER_SIZE];
	int w, h;
	char temp;

	while (!feof(file)) {
		fscanf(file, "%s", buffer);
		fscanf(file, "%d", &w);
		fscanf(file, "%d", &h);
		fscanf(file, "%c", &temp);

		if (strcmp(buffer, "frogShape") == 0) {
			objects->frog.shape = allocShape(w, h);
			objects->frog.w = w;
			objects->frog.h = h;

			for (int i = 0; i < h; ++i) {
				for (int j = 0; j < w; ++j) {
					fscanf(file, "%c", &objects->frog.shape[i][j]);
				}
				fscanf(file, "%c", &temp);
			}
		}

		if (strcmp(buffer, "carShape") == 0) {
			char** tempShape = allocShape(w, h);

			for (int i = 0; i < h; ++i) {
				for (int j = 0; j < w; ++j) {
					fscanf(file, "%c", &tempShape[i][j]);
				}
				fscanf(file, "%c", &temp);
			}
			for (int k = 0; k < objects->carsAmount; ++k) {
				objects->cars[k].shape = allocShape(w, h);
				objects->cars[k].w = w;
				objects->cars[k].h = h;

				for (int i = 0; i < h; ++i) {
					for (int j = 0; j < w; ++j) {
						objects->cars[k].shape[i][j] = tempShape[i][j];
					}
				}
			}
			
			deallocShape(tempShape,  h);
		}
		
	}

	fclose(file);
}

void initObjects(Objects* objects) {	
	objects->frog.x = WIDTH / 2.0f;
	objects->frog.y = HEIGHT - 1;

	for (int i = 0; i < INIT_CARS; ++i) {
		objects->cars[i].x = rand() % WIDTH; // change to double possibly
		objects->cars[i].y = rand() % HEIGHT;
	}
	objects->carsAmount = INIT_CARS;

	initShapes(objects);
}

void putShape(Map* map, const int X, const int Y, const int W, const int H, char** shape) {
	for (int i = 0; i < H; ++i) {
		for (int j = 0; j < W; ++j) {
			map->v[Y * H + i][X * W + j] = shape[i][j];
		}
	}
}

void fillMap(Map* map) {
	for (int i = 0; i < map->renderH; ++i) {
		for (int j = 0; j < map->renderW; ++j) {
			map->v[i][j] = ' ';
		}
	}

	for (int i = 0; i < map->objects.carsAmount; ++i) {
		Car* car = &map->objects.cars[i];
		putShape(map, car->x, car->y, car->w, car->h, car->shape);
	}

	Frog* frog = &map->objects.frog;
	putShape(map, frog->x, frog->y, frog->w, frog->h, frog->shape);
}

Map* initMap(Size* size) {
	Map* map = allocMap(size);

	// get from file here
	map->objects.frog.speed = 1.0f;
	map->renderW = size->w;
	map->renderH = size->h;

	initObjects(&map->objects);
	fillMap(map);

	return map;
}

/// UPDATE TICK

void movePlayer(Frog* frog, const int KEY) {
	if (KEY == ERR) {
		return;
	}

	switch(KEY) {
		case KEY_UP:
			frog->y -= frog->speed;
		break;

		case KEY_DOWN:
			frog->y += frog->speed;
		break;

		case KEY_LEFT:
			frog->x -= frog->speed;
		break;

		case KEY_RIGHT:
			frog->x += frog->speed;
		break;
	}

	// border collision
	if (frog->x < 0) {
		frog->x = 0;
	}
	if (frog->x >= WIDTH) {
		frog->x = WIDTH - 1;
	}
	if (frog->y < 0) {
		frog->y = 0;
	}
	if (frog->y >= HEIGHT) {
		frog->y = HEIGHT - 1;
	}
}

void updateTick(Map* map, const int KEY) {
	movePlayer(&map->objects.frog, KEY);
	fillMap(map);
}

/// RENDER

void renderMainWindow(Window* mainWindow, Map* map) {
	wborder(mainWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');
	for (int i = 0; i < map->renderH; ++i) {
		for (int j = 0; j < map->renderW; ++j) {
			mvwprintw(mainWindow->val, i + 1, j + 1, "%c", map->v[i][j]);
		}
	}
	wrefresh(mainWindow->val);
}

void renderStatusWindow(Window* statusWindow, StatusInfo* info) {
	wborder(statusWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');
	mvwprintw(statusWindow->val, statusWindow->H / 2.0f, 2, "Time: %.02lf", info->timePassed);
	wrefresh(statusWindow->val);
}

void renderFrame(Window* mainWindow, Window* statusWindow, Map* map, StatusInfo* statusInfo) {
	renderMainWindow(mainWindow, map);
	renderStatusWindow(statusWindow, statusInfo);
	refresh();
}

void updateStatusInfo(StatusInfo* info, Timer* timer) {
	info->timePassed = timer->sinceStart / (double)TIMER_PRECISION;
}

Size* initRenderSize() {
	FILE* file = fopen("data.csv", "r");

	Size* size = (Size*)malloc(sizeof(Size));
	char buffer[BUFFER_SIZE];

	while (!feof(file)) {
		fscanf(file, "%s", buffer);
 
		if (strcmp(buffer, "frogSize") == 0) {
			fscanf(file, "%d", &size->w);
			fscanf(file, "%d", &size->h);
		}
	}
	
	fclose(file);

	size->w *= WIDTH;
	size->h *= HEIGHT;

	size->w = 105;
	size->h = 30;

	return size;
}

/// MAIN LOOP

void runGame() {
	srand(time(NULL));
	
    Timer* timer = initTimer();
    u_int8_t continueLoop = true;

	Size* renderSize = initRenderSize();
	
	Window mainWindow = initWindow(renderSize->h + STATUS_HEIGHT, renderSize->h + 2, renderSize->w + 2, 0, 0);

	Window statusWindow = initWindow(renderSize->h + STATUS_HEIGHT, STATUS_HEIGHT, renderSize->w + 2, 0, renderSize->h + 1);
	StatusInfo* statusInfo = (StatusInfo*)malloc(sizeof(StatusInfo));

	Map* map = initMap(renderSize);
	int key = 0;
	int noerrKey = 0;

    while (continueLoop) {
		updateTimer(timer);
		updateStatusInfo(statusInfo, timer);
		key = getch();

		if (key != ERR) {
			noerrKey = key;
		}

		if (timer->updateTick) {
			timer->updateTick = 0;
			updateTick(map, noerrKey);
			noerrKey = ERR;
		}

		if (timer->renderFrame) {
			timer->renderFrame = 0;
			renderFrame(&mainWindow, &statusWindow, map, statusInfo);
		}
    }

	deallocMap(map);

	free(renderSize);
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
	curs_set(0);

	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);

    //splashScreen(gameWindow);
	runGame();

    endwin();

    return 0;
}