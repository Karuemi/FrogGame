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
	FINISH_COLOR,
	FROG_COLOR,
	CAR_COLOR,
	STREET_COLOR,
	ROCK_COLOR
};

enum LEVELS {
	EASY = 0,
	MEDIUM,
	HARD,
	REPLAY
};

enum LANES {
	ROCKS = 0,
	CARS,
	NONE
};

enum CAR_TYPES {
	NORMAL_CAR = 0,
	STOP_CAR,
	FRIEND_CAR,
	HOSTILE_CAR
};

enum LEVEL_1 {
	NONE_RATIO_1 = 1,
	CAR_RATIO_1 = 2,
	ROCK_RATIO_1 = 2,
	ROCK_DENSITY_1 = 20
};

enum MAP {
	WIDTH = 15,
	HEIGHT = 12,
	STATUS_HEIGHT = 5,
	INIT_CARS = 5,
	MAX_CARS = 64,
	MAX_ROCKS = 128,
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
	char** shape;
	int x[MAX_ROCKS];
	int y[MAX_ROCKS];
	int w;
	int h;
	int amount;
} Rocks;

typedef struct {
	char** leftShape;
	char** rightShape;
	double x[MAX_CARS];
	double y[MAX_CARS];
	char side[MAX_CARS];
	enum CAR_TYPES type[MAX_CARS];
	int w;
	int h;
	int amount;
} Cars;

typedef struct {
	double x;
	double y;
	int w;
	int h;
	double speed;
	char** shape;
} Frog;

typedef struct {
	Frog frog;
	Cars cars;
	Rocks rocks;
	int lanes[HEIGHT];
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
	deallocShape(objects->cars.leftShape, objects->cars.h);
	deallocShape(objects->cars.rightShape, objects->cars.h);
	deallocShape(objects->rocks.shape, objects->rocks.h);
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

void initFrogShape(Frog* frog, FILE* file, int w, int h) {
	frog->shape = allocShape(w, h);
	frog->w = w;
	frog->h = h;
	char temp;

	for (int i = 0; i < h; ++i) {
		for (int j = 0; j < w; ++j) {
			fscanf(file, "%c", &frog->shape[i][j]);
		}
		fscanf(file, "%c", &temp);
	}
}

void initRocksShape(Rocks* rocks, FILE* file, int w, int h) {
	rocks->shape = allocShape(w, h);
	rocks->w = w;
	rocks->h = h;
	char temp;

	for (int i = 0; i < h; ++i) {
		for (int j = 0; j < w; ++j) {
			fscanf(file, "%c", &rocks->shape[i][j]);
		}
		fscanf(file, "%c", &temp);
	}
}

void initCarsShape(Cars* cars, FILE* file, const int w, const int h, const char side) {
	cars->w = w;
	cars->h = h;
	char temp;

	if (side == 'R') {
		cars->rightShape = allocShape(w, h);
		for (int i = 0; i < h; ++i) {
			for (int j = 0; j < w; ++j) {
				fscanf(file, "%c", &cars->rightShape[i][j]);
			}
		fscanf(file, "%c", &temp);
	}

	} else if (side == 'L') {
		cars->leftShape = allocShape(w, h);
		for (int i = 0; i < h; ++i) {
			for (int j = 0; j < w; ++j) {
				fscanf(file, "%c", &cars->leftShape[i][j]);
			}
		fscanf(file, "%c", &temp);
		}
	}
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
			initFrogShape(&objects->frog, file, w, h);

		} else if (strcmp(buffer, "carShapeL") == 0) {
			initCarsShape(&objects->cars, file, w, h, 'L');

		} else if (strcmp(buffer, "carShapeR") == 0) {
			initCarsShape(&objects->cars, file, w, h, 'R');

		} else if (strcmp(buffer, "rockShape") == 0) {
			initRocksShape(&objects->rocks, file, w, h);

		} else {
			for (int i = 0; i < h; ++i) {
				fscanf(file, "%s", buffer);
			}
		}
		
	}

	fclose(file);
}

void initLanes(int lanes[HEIGHT], int noneRatio, int carsRatio, int rocksRatio) {
	lanes[0] = NONE;
	lanes[HEIGHT - 1] = NONE;

	int all = noneRatio + carsRatio + rocksRatio;
	double r;

	for (int i = 1; i < HEIGHT - 1; ++i) {
		while (true) {
			r = ((double)rand()) / RAND_MAX;

			if (r <= noneRatio / (double)all) {
				lanes[i] = NONE;
			} else if (r <= (carsRatio + noneRatio) / (double)all) {
				lanes[i] = CARS;
			} else if (r <= (rocksRatio + carsRatio + noneRatio) / (double)all) {
				lanes[i] = ROCKS;
			}

			if (lanes[i - 1] == ROCKS && lanes[i] == ROCKS ) {
				continue;
			} else {
				break;
			}
		}

	}
}

void initRocks(Objects* objects, const double FACTOR) {
	objects->rocks.amount = 0;
	double r;

	for (int i = 0; i < HEIGHT; ++i) {
		if (objects->lanes[i] != ROCKS) {
			continue;
		}

		for (int j = 0; j < WIDTH; ++j) {
			r = (((double)rand()) / RAND_MAX ) * 100;

			if (r <= ROCK_DENSITY_1) {
				objects->rocks.x[objects->rocks.amount] = j;
				objects->rocks.y[objects->rocks.amount] = i;
				objects->rocks.amount++;
			}
		}
	}
}

void initCars(Objects* objects) {

}

void initFrog(Objects* objects) {
	objects->frog.x = WIDTH / 2.0f;
	objects->frog.y = HEIGHT - 1;
}

void initObjects(Objects* objects) { /// easy
	initFrog(objects);

	initLanes(objects->lanes, NONE_RATIO_1, CAR_RATIO_1, ROCK_RATIO_1);

	initRocks(objects, ROCK_DENSITY_1);
	initCars(objects);

	initShapes(objects);
}

void putStreetLines(Map* map) {
	for (int i = 0; i < HEIGHT; ++i) {
		if (map->objects.lanes[i] == CARS) {
			for (int j = 0; j < map->renderW; ++j) {
				if (j % 10 == 0 || j % 10 == 1 || j % 10 == 2) {
					map->tiles[i * map->objects.frog.h + 2][j].v = '-';
					map->tiles[i * map->objects.frog.h + 2][j].c = STREET_COLOR;
				}
			}
		}
	}
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

	putStreetLines(map);

	// rocks
	for (int i = 0; i < map->objects.rocks.amount; ++i) {
		Rocks* rocks = &map->objects.rocks;
		putShape(map, rocks->x[i], rocks->y[i], rocks->w, rocks->h, ROCK_COLOR, rocks->shape);
	}

	// cars
	char** carShape;
	for (int i = 0; i < map->objects.cars.amount; ++i) {
		Cars* cars = &map->objects.cars;

		if (cars->side[i] == 'R') {
			carShape = cars->rightShape;
		} else {
			carShape = cars->rightShape;
		}

		putShape(map, cars->x[i], cars->y[i], cars->w, cars->h, CAR_COLOR, carShape);
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
	if (frog->y != 0) {
		return;
	}

	char text[] = "Congratulations You Won!";
	Size size = {40, 5};
	Window won = initBasicWindow(&size);

	mvwprintw(won.val, won.H / 2, won.W / 2 - strlen(text) / 2, "%s", text);
	keypad(won.val, TRUE);
	nodelay(won.val, FALSE);

	int key;
	do {
		key = wgetch(won.val);
	} while (key == ERR || key == KEY_UP);

	exit(0);
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
	checkWin(&map->objects.frog);

	movePlayer(&map->objects.frog, KEY);
	fillMap(map);
}

/// RENDER

void renderBorder(Window* mainWindow) {
	wattron(mainWindow->val,COLOR_PAIR(BORDER_COLOR));
	wborder(mainWindow->val, '|', '|', '-', '-', '+', '+', '+', '+');
	wattroff(mainWindow->val,COLOR_PAIR(BORDER_COLOR));
}

void renderFinishLine(Window* mainWindow, Map* map) {
	wattron(mainWindow->val, COLOR_PAIR(FINISH_COLOR));
	for (int i = 0; i < map->renderW; ++i) {
		if (map->tiles[2][i].c == EMPTY_COLOR) {
			mvwprintw(mainWindow->val, 2, i + 1, "=");
		}	
	}
	wattroff(mainWindow->val, COLOR_PAIR(FINISH_COLOR));
}

void renderMainWindow(Window* mainWindow, Map* map) {
	renderBorder(mainWindow);
	
	for (int i = 0; i < map->renderH; ++i) {
		for (int j = 0; j < map->renderW; ++j) {
			wattron(mainWindow->val, COLOR_PAIR(map->tiles[i][j].c));
			mvwprintw(mainWindow->val, i + 1, j + 1, "%c", map->tiles[i][j].v);
			wattroff(mainWindow->val, COLOR_PAIR(map->tiles[i][j].c));
		}
	}

	renderFinishLine(mainWindow, map);
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
	init_pair(BORDER_COLOR, COLOR_BLACK, COLOR_BLUE); // border
	init_pair(FROG_COLOR, COLOR_GREEN, COLOR_BLACK); // frog
	init_pair(CAR_COLOR, COLOR_WHITE, COLOR_BLACK); // car
	init_pair(FINISH_COLOR, COLOR_GREEN, COLOR_BLACK); // finish line
	init_pair(STREET_COLOR, COLOR_WHITE, COLOR_BLACK); // street line
	init_pair(ROCK_COLOR, COLOR_WHITE, COLOR_BLACK); // rock color
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
		} else {
			for (int i = 0; i < h; ++i) {
				fscanf(file, "%s", buffer);
			}
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