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

enum COLORS {
	EMPTY_COLOR = 0,
	BORDER_COLOR,
	FROG_COLOR,
	CAR_COLOR
};

enum LEVELS {
	EASY = 0,
	MEDIUM,
	HARD,
	REPLAY
};

enum MAP {
	WIDTH = 15,
	HEIGHT = 10,
	STATUS_HEIGHT = 5,
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
	enum COLORS c;
	char v;
} Tile;

typedef struct {
	Tile** tiles;
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

Window initBasicWindow(Size* size) {
	Window window = initWindow(size->h + STATUS_HEIGHT, size->h  + STATUS_HEIGHT + 1, size->w + 2, 0, 0);
	return window;
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

	map->tiles = (Tile**)malloc(sizeof(Tile*) * size->h);

	for (int i = 0; i < size->h; ++i) {
		map->tiles[i] = (Tile*)malloc(sizeof(Tile) * size->w);
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
		free(map->tiles[i]);
	}
	free(map->tiles);
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

void putShape(Map* map, const int X, const int Y, const int W, const int H, enum COLORS c, char** shape) {
	for (int i = 0; i < H; ++i) {
		for (int j = 0; j < W; ++j) {
			if (Y * H + i >= map->renderH || X * W + j >= map->renderW) {
				continue;
			}
			map->tiles[Y * H + i][X * W + j].v = shape[i][j];
			map->tiles[Y * H + i][X * W + j].c = c;
		}
	}
}

void fillMap(Map* map) {
	for (int i = 0; i < map->renderH; ++i) {
		for (int j = 0; j < map->renderW; ++j) {
			map->tiles[i][j].v = ' ';
			map->tiles[i][j].c = EMPTY_COLOR;
		}
	}

	for (int i = 0; i < map->objects.carsAmount; ++i) {
		Car* car = &map->objects.cars[i];
		putShape(map, car->x, car->y, car->w, car->h, CAR_COLOR, car->shape);
	}

	Frog* frog = &map->objects.frog;
	putShape(map, frog->x, frog->y, frog->w, frog->h, FROG_COLOR, frog->shape);
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

void checkWin(Frog* frog) {
	if (frog->y == 0) {
		exit(0);
	}
}

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

	checkWin(&map->objects.frog);
}

/// RENDER

void renderMainWindow(Window* mainWindow, Map* map) {
	wattron(mainWindow->val,COLOR_PAIR(BORDER_COLOR));
	wborder(mainWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');
	wattroff(mainWindow->val,COLOR_PAIR(BORDER_COLOR));
	
	for (int i = 0; i < map->renderH; ++i) {
		for (int j = 0; j < map->renderW; ++j) {
			wattron(mainWindow->val, COLOR_PAIR(map->tiles[i][j].c));
			mvwprintw(mainWindow->val, i + 1, j + 1, "%c", map->tiles[i][j].v);
			wattroff(mainWindow->val, COLOR_PAIR(map->tiles[i][j].c));
		}
	}
	wrefresh(mainWindow->val);
}

void renderStatusWindow(Window* statusWindow, StatusInfo* info) {
	wattron(statusWindow->val, COLOR_PAIR(BORDER_COLOR));
	wborder(statusWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');

	for (int i = 1; i < statusWindow->H - 1; ++i) {
		for (int j = 1; j < statusWindow->W - 1; ++j) {
			mvwprintw(statusWindow->val, i, j, " ");
		}
	}

	mvwprintw(statusWindow->val, statusWindow->H / 2.0f, 2, "Time: %.02lf", info->timePassed);
	wattroff(statusWindow->val, COLOR_PAIR(BORDER_COLOR));
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
 
		if (strcmp(buffer, "frogShape") == 0) {
			fscanf(file, "%d", &size->w);
			fscanf(file, "%d", &size->h);
		}
	}
	
	fclose(file);

	size->w *= WIDTH;
	size->h *= HEIGHT;

	return size;
}

void initColors() {
	init_pair(EMPTY_COLOR, COLOR_RED, COLOR_BLACK); // none
	init_pair(BORDER_COLOR, COLOR_BLACK, COLOR_CYAN); // border
	init_pair(FROG_COLOR, COLOR_CYAN, COLOR_BLACK); // frog
	init_pair(CAR_COLOR, COLOR_WHITE, COLOR_BLACK); // car
}

/// MAIN LOOP

void printFrog(Window* menu) {
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
			char** shape = allocShape(w, h);

			for (int i = 0; i < h; ++i) {
				for (int j = 0; j < w; ++j) {
					fscanf(file, "%c", &shape[i][j]);
				}
				fscanf(file, "%c", &temp);
			}

			for (int i = 0; i < h; ++i) {
				for (int j = 0; j < w; ++j) {
					mvwprintw(menu->val, menu->H * 0.75f + i, menu->W /2 + j - w / 2, "%c", shape[i][j]);
				}
			}

			deallocShape(shape, h);
		}
	}
	fclose(file);
}

void showMenu(Size* size, enum LEVELS* level) {
	Window menu = initBasicWindow(size);
	char* content[5];
	for (int i = 0; i < 4; ++i) {
		content[i] = (char*)malloc(sizeof(char) * 128 );
	}
	strcpy(content[0], "Level Easy");
	strcpy(content[1], "Level Medium");
	strcpy(content[2], "Level Hard");
	strcpy(content[3], "Replay Last Session");
	strcpy(content[4], "Game made by Pawel Richert s203693");
	
	keypad(menu.val, TRUE);
	nodelay(menu.val, TRUE);

	int cursorPos = 0;
	int key;
	char temp;

	do {
		key = wgetch(menu.val);

		if (key == KEY_DOWN) {
			if (cursorPos < 3) {
				cursorPos++;
			}
		} else  if (key == KEY_UP) {
			if (cursorPos > 0) {
				cursorPos--;
			}
		}

		for (int i = 0; i < 4; ++i) {
			if (cursorPos == i) {
				temp = '>';
			} else {
				temp = ' ';
			}

			mvwprintw(menu.val, menu.H / 2 + i * 2 - 8, menu.W / 2 - strlen(content[i]) / 2 - 3, "%c", temp);
			mvwprintw(menu.val, menu.H / 2 + i * 2 - 8, menu.W / 2 - strlen(content[i]) / 2, "%s", content[i]);
		}

		mvwprintw(menu.val, menu.H  - 3, menu.W / 2 - strlen(content[4]) / 2, "%s", content[4]);
		printFrog(&menu);
		wrefresh(menu.val);
	} while (key != '\n') ;

	*level = cursorPos;
	
	for (int i = 0; i < 4; ++i) {
		free(content[i]);
	}

	delwin(menu.val);
}

void runGame() {
	srand(time(NULL));
	
    Timer* timer = initTimer();
    u_int8_t continueLoop = true;

	Size* renderSize = initRenderSize();

	enum LEVELS level = EASY;
	showMenu(renderSize, &level);
	
	Window mainWindow = initWindow(renderSize->h + STATUS_HEIGHT, renderSize->h + 2, renderSize->w + 2, 0, 0);

	Window statusWindow = initWindow(renderSize->h + STATUS_HEIGHT, STATUS_HEIGHT, renderSize->w + 2, 0, renderSize->h + 1);
	StatusInfo* statusInfo = (StatusInfo*)malloc(sizeof(StatusInfo));

	initColors();
	bkgd(COLOR_PAIR(EMPTY_COLOR));

	Map* map = initMap(renderSize);
	int key = 0;
	int noerrKey = 0;

	timer->start = getCurrentTime();
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
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	start_color();
    noecho();
	cbreak();
	curs_set(0);

	runGame();

    endwin();

    return 0;
}