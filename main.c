/*
Pawel Richert 203693

SOURCES:
https://developer.mozilla.org/en-US/docs/Games/Techniques/2D_collision_detection
https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/
https://www.geeksforgeeks.org/
https://www.gnu.org/software/guile-ncurses/manual/html_node/Getting-characters-from-the-keyboard.html
http://allaboutfrogs.org/gallery/frogstuff/ascii.html
https://stackoverflow.com/
*/

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/time.h>
#include <time.h>

enum TIMER {
	TIMER_PRECISION = (int)1e6,
    FPS = 60,
    TPS = 20
};

enum COLORS {
	EMPTY_COLOR = 0,
	BORDER_COLOR,
	FINISH_COLOR,
	FROG_COLOR,
	CAR_COLOR,
	STREET_COLOR,
	ROCK_COLOR,
	NORMAL_CAR_COLOR,
	STOP_CAR_COLOR,
	FRIEND_CAR_COLOR,
	HOSTILE_CAR_COLOR
};

enum LEVELS {
	EASY = 0,
	MEDIUM,
	HARD,
	REPLAY
};

enum LANES {
	ROCKS = 0,
	LEFT_CARS,
	RIGHT_CARS,
	NONE
};

enum LANE_TYPES {
	NORMAL_CAR = 0,
	STOP_CAR,
	FRIEND_CAR,
	HOSTILE_CAR,
	WARP_CAR,
	OTHER
};

enum LEVEL_1 {
	NONE_RATIO_1 = 1,
	CAR_RATIO_1 = 2,
	ROCK_RATIO_1 = 2,
	ROCK_DENSITY_1 = 20,
	CAR_DENSITY_1 = 10,
	CAR_PLACER_1 = 5,
	IS_NORMAL_1 = 1,
	IS_STOP_1 = 1,
	IS_FRIEND_1 = 1,
	IS_HOSTILE_1 = 1,
	MIN_SPEED_1 = 100,
	MAX_SPEED_1 = 500,
	SPEED_UP_1 = 20,
	REAPPEAR_DIST_1 = 50
};

enum MAP {
	WIDTH = 15,
	HEIGHT = 12,
	STATUS_HEIGHT = 5,
	INIT_CARS = 5,
	MAX_CARS = 64,
	MIN_CARS = 5,
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
	char level[BUFFER_SIZE];
	double points;
	int jumps;
} StatusInfo;

typedef struct {
	int noneRatio;
	int carRatio;
	int rockRatio;
	int rockDensity;
	int carDensity;
	int carPlacerSpacing;
	uint8_t isNormal;
	uint8_t isStop;
	uint8_t isFriend;
	uint8_t isHostile;
	int maxSpeed;
	int minSpeed;
	int speedUp;
} LevelInfo;

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
	int y[MAX_CARS];
	int w;
	int h;
	int amount;
	int speedUp;
} Cars;

typedef struct {
	double xX;
	double yY;
	int x;
	int y;
	int w;
	int h;
	char** shape;
	int jumps;
	uint8_t queueEnding;
} Frog;

typedef struct {
	enum LANES v[HEIGHT];
	enum LANE_TYPES type[HEIGHT];
	double speed[HEIGHT];
	double setSpeed[HEIGHT];
	int minSpeed;
	int maxSpeed;
	int reappearDist;
} Lanes;

typedef struct {
	Frog frog;
	Cars cars;
	Rocks rocks;
	Lanes lanes;
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
	frog->jumps = 0;
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

void initCarsShape(Cars* cars, FILE* file, const int w, const int h, enum LANES side) {
	cars->w = w;
	cars->h = h;
	char temp;

	if (side == RIGHT_CARS) {
		cars->rightShape = allocShape(w, h);
		for (int i = 0; i < h; ++i) {
			for (int j = 0; j < w; ++j) {
				fscanf(file, "%c", &cars->rightShape[i][j]);
			}
		fscanf(file, "%c", &temp);
	}

	} else if (side == LEFT_CARS) {
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
			initCarsShape(&objects->cars, file, w, h, LEFT_CARS);

		} else if (strcmp(buffer, "carShapeR") == 0) {
			initCarsShape(&objects->cars, file, w, h, RIGHT_CARS);

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

void initBasicLanes(Lanes* lanes) {
	lanes->v[0] = NONE;
	lanes->v[1] = RIGHT_CARS;
	lanes->v[HEIGHT - 1] = NONE;

	for (int i = 0; i < HEIGHT; ++i) {
		lanes->type[i] = OTHER;
	}
}

void initLanesTypes(Lanes* lanes, uint8_t isNormal, uint8_t isStop, uint8_t isFriend, uint8_t isHostile) {
	lanes->type[1] = WARP_CAR;
	lanes->speed[1] = (rand() % (lanes->maxSpeed - lanes->minSpeed) + lanes->minSpeed) * 0.01f;
	lanes->setSpeed[1] = lanes->speed[1];
	uint8_t loop = 1;
	int r;

	for (int i = 2; i < HEIGHT - 1; ++i) {
		lanes->speed[i] = (rand() % (lanes->maxSpeed - lanes->minSpeed) + lanes->minSpeed) * 0.01f;
		lanes->setSpeed[i] = lanes->speed[i];
		loop = 1;

		while (loop) {
			r =  rand() % 4;

			switch (r) {
				case NORMAL_CAR:
					if (isNormal) {
						lanes->type[i] = NORMAL_CAR;
						loop = 0;
					}
				break;

				case STOP_CAR:
					if (isStop) {
						lanes->type[i] = STOP_CAR;
						loop = 0;
					}
				break;

				case FRIEND_CAR:
					if (isFriend) {
						lanes->type[i] = FRIEND_CAR;
						loop = 0;
					}
				break;

				case HOSTILE_CAR:
					if (isHostile) {
						lanes->type[i] = HOSTILE_CAR;
						loop = 0;
					}
				break;
			}
		}
	}
}

void initLanes(Lanes* lanes, int noneRatio, int carsRatio, int rocksRatio, int minSpeed, int maxSpeed, int reappearDist) {
	initBasicLanes(lanes);

	int all = noneRatio + carsRatio + rocksRatio;
	double r, r2;

	lanes->minSpeed = minSpeed;
	lanes->maxSpeed = maxSpeed;
	lanes->reappearDist = reappearDist;

	for (int i = 2; i < HEIGHT - 1; ++i) {
		while (true) {
			r = ((double)rand()) / RAND_MAX;

			if (r <= noneRatio / (double)all) {
				lanes->v[i] = NONE;
				
			} else if (r <= (carsRatio + noneRatio) / (double)all) {
				r2 = ((double)rand()) / RAND_MAX;

				if (r2 <= 0.5f) {
					lanes->v[i] = LEFT_CARS;
				} else {
					lanes->v[i] = RIGHT_CARS;
				}
			} else if (r <= (rocksRatio + carsRatio + noneRatio) / (double)all) {
				lanes->v[i] = ROCKS;
			}

			if (lanes->v[i - 1] == ROCKS && lanes->v[i] == ROCKS ) {
				continue;
			} else {
				break;
			}
		}

	}
}

void initRocks(Objects* objects, const int ROCK_DENSITY) {
	objects->rocks.amount = 0;
	double r;

	for (int i = 0; i < HEIGHT; ++i) {
		if (objects->lanes.v[i] != ROCKS) {
			continue;
		}

		for (int j = 0; j < WIDTH; ++j) {
			r = (((double)rand()) / RAND_MAX ) * 100;

			if (r <= ROCK_DENSITY) {
				objects->rocks.x[objects->rocks.amount] = j;
				objects->rocks.y[objects->rocks.amount] = i;
				objects->rocks.amount++;
			}
		}
	}
}

void initCars(Map* map, Objects* objects, const int CAR_DENSITY, const int CAR_PLACER, const int SPEED_UP) {
	double r;
	int placer = 0;
	Cars* cars = &objects->cars;
	cars->speedUp = SPEED_UP;

	int* amount = &cars->amount;
	*amount = 0;
	
	for (int i = 0; i < HEIGHT; ++i) {

		if (objects->lanes.type[i] == WARP_CAR) {
			placer = map->renderW / 2;

			for (int j = 0; j < MIN_CARS; ++j) {
				cars->y[*amount] = i;
				cars->x[*amount] = placer;
				(*amount)++;

				placer += cars->w + 1;
			}
			continue;
		}

		if (objects->lanes.v[i] != LEFT_CARS && objects->lanes.v[i] != RIGHT_CARS ) {
			continue;
		}

		placer = -cars->w + 1;
		while (placer < map->renderW + cars->w) {
			if (*amount == MAX_CARS - 1) {
				break;
			}

			r = (((double)rand()) / RAND_MAX ) * 100.0f;

			if (r <= CAR_DENSITY) { 
				cars->y[*amount] = i;
				cars->x[*amount] = placer;
				(*amount)++;

				placer += cars->w + 1;
			} else {
				placer += CAR_PLACER;
			}
		}
	}
}

void initFrog(Objects* objects) {
	objects->frog.x = WIDTH / 2.0f;
	objects->frog.y = HEIGHT - 1;
	objects->frog.queueEnding = 0;
}

void initObjects(Map* map) {
	initShapes(&map->objects);

	initFrog(&map->objects);

	initLanes(&map->objects.lanes, NONE_RATIO_1, CAR_RATIO_1, ROCK_RATIO_1, MIN_SPEED_1, MAX_SPEED_1, REAPPEAR_DIST_1);
	initLanesTypes(&map->objects.lanes, IS_NORMAL_1, IS_STOP_1, IS_FRIEND_1, IS_HOSTILE_1);

	initRocks(&map->objects, ROCK_DENSITY_1);
	initCars(map, &map->objects, CAR_DENSITY_1, CAR_PLACER_1, SPEED_UP_1);
}

void putStreetLines(Map* map) {
	for (int i = 0; i < HEIGHT; ++i) {
		if (map->objects.lanes.v[i] == LEFT_CARS || map->objects.lanes.v[i] == RIGHT_CARS) {
			for (int j = 0; j < map->renderW; ++j) {
				if (i % 2 == 0) {
					if (j % 10 == 0 || j % 10 == 1 || j % 10 == 2) {
						map->tiles[i * map->objects.frog.h + 2][j].v = '-';
						map->tiles[i * map->objects.frog.h + 2][j].c = STREET_COLOR;
					}
				} else {
					if (j % 10 == 5 || j % 10 == 6 || j % 10 == 7) {
						map->tiles[i * map->objects.frog.h + 2][j].v = '-';
						map->tiles[i * map->objects.frog.h + 2][j].c = STREET_COLOR;
					}
				}
			}
		}
	}
}

void putShape(Map* map, const int X, const int Y, const int W, const int H, enum COLORS c, char** shape) {
	Frog* frog = &map->objects.frog;

	for (int i = 0; i < H; ++i) {
		for (int j = 0; j < W; ++j) {
			if (Y + i >= map->renderH || X + j >= map->renderW || 
				Y + i < 0 ||  X + j < 0 ) {
				continue;
			}
			map->tiles[Y + i][X + j].v = shape[i][j];
			map->tiles[Y + i][X + j].c = c;
		}
	}
}

void fillEmpty(Map* map) {
	for (int i = 0; i < map->renderH; ++i) {
		for (int j = 0; j < map->renderW; ++j) {
			map->tiles[i][j].v = ' ';
			map->tiles[i][j].c = EMPTY_COLOR;
		}
	}
}

void fillRocks(Map* map) {
	Frog* frog = &map->objects.frog;

	for (int i = 0; i < map->objects.rocks.amount; ++i) {
		Rocks* rocks = &map->objects.rocks;
		putShape(map, rocks->x[i] * frog->w, rocks->y[i] * frog->h, rocks->w, rocks->h, ROCK_COLOR, rocks->shape);
	}
}

void fillCars(Map* map) {
	Frog* frog = &map->objects.frog;

	Cars* cars = &map->objects.cars;
	char** carShape;
	enum COLORS color;

	for (int i = 0; i < cars->amount; ++i) {
		if (map->objects.lanes.v[cars->y[i]] == RIGHT_CARS) {
			carShape = cars->rightShape;
		} else {
			carShape = cars->leftShape;
		}

		switch (map->objects.lanes.type[cars->y[i]]) {
			case NORMAL_CAR:
				color = NORMAL_CAR_COLOR;
			break;

			case STOP_CAR:
				color = STOP_CAR_COLOR;
			break;

			case FRIEND_CAR:
				color = FRIEND_CAR_COLOR;
			break;

			case HOSTILE_CAR:
				color = HOSTILE_CAR_COLOR;
			break;

			case WARP_CAR:
				color = NORMAL_CAR_COLOR;
			break;

			case OTHER:
				color = NORMAL_CAR_COLOR;
			break;
		}

		putShape(map, (int)cars->x[i], cars->y[i] * frog->h, cars->w, cars->h, color, carShape);
	}
}

void fillMap(Map* map) {
	Frog* frog = &map->objects.frog;

	fillEmpty(map);
	putStreetLines(map);
	fillRocks(map);
	fillCars(map);

	putShape(map, frog->x * frog->w, frog->y * frog->h, frog->w, frog->h, FROG_COLOR, frog->shape);
}

Map* initMap(Size* size) {
	Map* map = allocMap(size);

	map->renderW = size->w;
	map->renderH = size->h;

	initObjects(map);
	fillMap(map);

	return map;
}

/// UPDATE TICK

void frogBorderCollision(Frog* frog) {
	if (frog->x < 0) {
		frog->x = 0;
		frog->jumps--;
	}
	if (frog->x >= WIDTH) {
		frog->x = WIDTH - 1;
		frog->jumps--;
	}
	if (frog->y < 0) {
		frog->y = 0;
		frog->jumps--;
	}
	if (frog->y >= HEIGHT) {
		frog->y = HEIGHT - 1;
		frog->jumps--;
	}
}

void popupWindow(char text[]) {
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

void checkEnding(Frog* frog) {
	if (frog->y == 0) {
		char text[] = "Congratulations You Won!";
		popupWindow(text);
	}
	if (frog->queueEnding) {
		char text[] = "You Lost the Game :(";
		popupWindow(text);
	}
}

uint8_t checkCarCollision(Frog* frog, Cars* cars, int offX, int offY, int i) {
	if (frog->x * frog->w < cars->x[i] + cars->w + offX &&
		frog->x * frog->w + frog->w + offX > cars->x[i] &&
		frog->y * frog->h < cars->y[i] * frog->h + cars->h + offY &&
		frog->y * frog->h + frog->h + offY > cars->y[i] * frog->h) {
		return 1;
	}
	
	return 0;
}

void handleCars(Lanes* lanes, Frog* frog, Cars* cars) {
	int blockLaneSpeed = -1;
	int increaseLaneSpeed = -1;

	for (int i = 0; i < cars->amount; ++i) {
		if (checkCarCollision(frog, cars, 0, 0, i)) {
			if (lanes->type[cars->y[i]] != FRIEND_CAR) {
				frog->queueEnding = 1;
			} else {
				if (lanes->v[cars->y[i]] == LEFT_CARS) {
					frog->x = (cars->x[i] + cars->w) / (double)frog->w;
				} else {
					frog->x = cars->x[i] / (double)frog->w;
				}

				frogBorderCollision(frog);
			}
		}

		if (lanes->type[cars->y[i]] == STOP_CAR) {
			if (checkCarCollision(frog, cars, 5, 0, i)) {
				blockLaneSpeed = cars->y[i];
			} else {
				lanes->speed[cars->y[i]] += cars->speedUp * 0.01f;
				if (lanes->speed[cars->y[i]] >= lanes->setSpeed[cars->y[i]]) {
					lanes->speed[cars->y[i]] = lanes->setSpeed[cars->y[i]];
				}
			}
		}

		if (lanes->type[cars->y[i]] == HOSTILE_CAR) {
			if (checkCarCollision(frog, cars, WIDTH * frog->w, 0, i)) {
				increaseLaneSpeed = cars->y[i];
			} else {
				lanes->speed[cars->y[i]] -= cars->speedUp * 0.01f;

				if (lanes->speed[cars->y[i]] <= lanes->setSpeed[cars->y[i]]) {
					lanes->speed[cars->y[i]] = lanes->setSpeed[cars->y[i]];
				}
			}
		}
	}

	for (int i = 0; i < HEIGHT; ++i) {
		if (lanes->v[i] != LEFT_CARS && lanes->v[i] != RIGHT_CARS) {
			continue;
		}

		if (blockLaneSpeed != -1) {
			lanes->speed[blockLaneSpeed] = 0;
		}

		if (increaseLaneSpeed != -1) {
			lanes->speed[increaseLaneSpeed] += cars->speedUp * 0.01f;
		}
	}
}

uint8_t rockCollision(Frog* frog, Rocks* rocks) {
	for (int i = 0; i < rocks->amount; ++i) {
		if (rocks->x[i] == frog->x && rocks->y[i] == frog->y ) {
			return 1;
		}
	}
	return 0;
}

void movePlayer(Frog* frog, Rocks* rocks, const int KEY) {
	if (KEY == ERR) {
		return;
	}

	switch(KEY) {
		case KEY_UP:
			frog->y--;
			if (rockCollision(frog, rocks)) {
				frog->y++;
			} else {
				frog->jumps++;
			}
		break;

		case KEY_DOWN:
			frog->y++;
			if (rockCollision(frog, rocks)) {
				frog->y--;
			} else {
				frog->jumps++;
			}
		break;

		case KEY_LEFT:
			frog->x--;
			if (rockCollision(frog, rocks)) {
				frog->x++;
			} else {
				frog->jumps++;
			}
		break;

		case KEY_RIGHT:
			frog->x++;
			if (rockCollision(frog, rocks)) {
				frog->x--;
			} else {
				frog->jumps++;
			}
		break;
	}

	frogBorderCollision(frog);
}

void moveCars(Map* map) {
	Cars* cars = &map->objects.cars;
	Lanes* lanes = &map->objects.lanes;
	int reverse = 1;
	int left = 1;

	for (int i = 0; i < cars->amount; ++i) {
		if (lanes->v[cars->y[i]] == LEFT_CARS) {
			reverse = 1;
			left = 1;
		} else {
			reverse = -1;
			left = 0;
		}

		cars->x[i] += lanes->speed[cars->y[i]] * reverse;

		if (map->objects.lanes.type[cars->y[i]] == WARP_CAR) {
			if (cars->x[i] < -cars->w && !left) {
				cars->x[i] = map->renderW + cars->w;
			} else if (cars->x[i] > map->renderW + cars->w && left) {
				cars->x[i] = -cars->w;
			}
		} else { // change car lanes
			if (cars->x[i] < -cars->w && !left || cars->x[i] > map->renderW + cars->w && left) {
				while (true) {
					int r = rand() % (HEIGHT - 1) + 2;
					int r2 = 0;

					if (lanes->v[r] == LEFT_CARS || lanes->v[r] == RIGHT_CARS) {
						r2 = rand() % lanes->reappearDist;
						cars->y[i] = r;
					}
					
					if (lanes->v[r] == LEFT_CARS) {
						cars->x[i] = -r2 -cars->w;	
						break;
					} else if (lanes->v[r] == RIGHT_CARS) {
						cars->x[i] = r2 + map->renderW + cars->w;
						break;
					}
				}
			}
		}
	}
}

void updateTick(Map* map, const int KEY) {
	checkEnding(&map->objects.frog);

	moveCars(map);

	movePlayer(&map->objects.frog, &map->objects.rocks, KEY);

	handleCars(&map->objects.lanes, &map->objects.frog, &map->objects.cars);

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

	const int WH2 = statusWindow->H / 2.0f;
	mvwprintw(statusWindow->val, WH2, statusWindow->W * 0.05f, "Time: %.02lf", info->timePassed);
	mvwprintw(statusWindow->val, WH2, statusWindow->W * 0.2f, "Jumps: %d", info->jumps);
	mvwprintw(statusWindow->val, WH2, statusWindow->W * 0.35f, "Points: %.02lf", info->points);
	mvwprintw(statusWindow->val, WH2, statusWindow->W * 0.58f, "%s", info->level);
	mvwprintw(statusWindow->val, WH2, statusWindow->W * 0.75f, "Pawel Richert 203693");
	wattroff(statusWindow->val, COLOR_PAIR(BORDER_COLOR));
	wrefresh(statusWindow->val);
}

void renderFrame(Window* mainWindow, Window* statusWindow, Map* map, StatusInfo* statusInfo) {
	renderMainWindow(mainWindow, map);
	renderStatusWindow(statusWindow, statusInfo);
	refresh();
}

void updateStatusInfo(StatusInfo* info, enum LEVELS level, Timer* timer, Frog* frog) {
	info->timePassed = timer->sinceStart / (double)TIMER_PRECISION;

	switch (level) {
		case EASY:
			strcpy(info->level, "Easy Level");
		break;

		case MEDIUM:
			strcpy(info->level, "Medium Level");
		break;

		case HARD:
			strcpy(info->level, "Hard Level");
		break;

		case REPLAY:
			strcpy(info->level, "Replay Mode");
		break;
	}

	info->jumps = frog->jumps;
	info->points = 1000.0f - ((int)info->timePassed * frog->jumps);
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
	init_pair(EMPTY_COLOR, COLOR_RED, COLOR_BLACK);
	init_pair(BORDER_COLOR, COLOR_BLACK, COLOR_BLUE);
	init_pair(FROG_COLOR, COLOR_GREEN, COLOR_BLACK);
	init_pair(FINISH_COLOR, COLOR_GREEN, COLOR_BLACK);
	init_pair(STREET_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(ROCK_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(NORMAL_CAR_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(STOP_CAR_COLOR, COLOR_YELLOW, COLOR_BLACK);
	init_pair(FRIEND_CAR_COLOR, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(HOSTILE_CAR_COLOR, COLOR_RED, COLOR_BLACK);
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

	enum LEVELS level;
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
		updateStatusInfo(statusInfo, level, timer, &map->objects.frog);
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