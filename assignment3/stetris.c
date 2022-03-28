#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <poll.h>
#include <glob.h>
#include <dirent.h>


// The game state can be used to detect what happens on the playfield
#define GAMEOVER   0
#define ACTIVE     (1 << 0)
#define ROW_CLEAR  (1 << 1)
#define TILE_ADDED (1 << 2)

// If you extend this structure, either avoid pointers or adjust
// the game logic allocate/deallocate and reset the memory
typedef struct {
  bool occupied;
} tile;

typedef struct {
  unsigned int x;
  unsigned int y;
} coord;

typedef struct {
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile;                       // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;



gameConfig game = {
                   .grid = {8, 8},
                   .uSecTickTime = 10000,
                   .rowsPerLevel = 2,
                   .initNextGameTick = 50,
};

// File descriptors for the framebuffer and the joystick
int fb_fd;
int js_fd;

// Variables for getting the screen info
struct fb_var_screeninfo fb_var_info;
struct fb_fix_screeninfo fb_fix_info;

// Variables for mapping the framebuffer
void* fbp;
int screensize; 

// Variables for the color mapping
int currentColor = 0; 
int colorMap[64];

// Function for mapping numbers 0-7 to colors
uint16_t getColor(int value) {
	uint16_t result = 0x0000; 
	switch(value)
	{
		case 0:
			result = 0xFFFF; // White
			break;
		case 1: 
			result = 0xFB00; // Red
			break;
		case 2:
			result = 0x004F; // Blue
			break;
		case 3:
			result = 0x0BB0; // Green
			break;
		case 4:
			result = 0xFF80; // Yellow
			break;
		case 5:
			result = 0x08FF; // Cyan
			break;
		case 6:
			result = 0xF00F; // Magenta
			break;
		case 7:
			result = 0x0000; // Black 
			break;
	}
	return result; 

	
}

// Function for adding a tile to the color map (so it can get drawn later)
void addTileToMap(coord tile) {
	currentColor = (currentColor + 1) % 7; 
	colorMap[tile.x + game.grid.x*tile.y] = currentColor; 
}

// Function for moving a tile's position in the color map
void moveTileInMap(coord prevPos, coord newPos) {
	int prevLoc = prevPos.x + game.grid.x*prevPos.y;
	int newLoc = newPos.x + game.grid.x*newPos.y; 
	int prevCol = colorMap[prevLoc]; 
	colorMap[newLoc] = prevCol; 
	colorMap[prevLoc] = 7; 
}

// Function for removing the bottom row from the color map
void removeBottomRow() {

	// Clearing the bottom row
	for (int i = 8 * 7; i < 8*8; i++) {
		colorMap[i] = 7; // Setting it to black	
	}
	
	// Moving the rest of the board one down
	for (int y = 6; y > -1; y--) {
		for (int x = 0; x < 8; x++) {
			int prevLoc = x + y*8;
			int newLoc = x + (y+1)*8;
			int prevCol = colorMap[prevLoc]; 
			colorMap[newLoc] = prevCol; 
			colorMap[prevLoc] = 7; 
		}	
	}
}

// Function for setting the color of all the tiles to black
void clearScreen() {
	memset(colorMap, 7, sizeof(colorMap)); 
}

// Function used to filter directory entries. The first two letters have to be "fb"
int filterFrameBuffer(const struct dirent *entry) {
	return (0 == strncmp(entry->d_name, "fb", 2)); 
}

// Function used to filter directory entries. The first five letters have to be "event"
int filterJoystick(const struct dirent *entry) {
	return (0 == strncmp(entry->d_name, "event", 5)); 
}

// This function is called on the start of your application
// Here you can initialize what ever you need for your task
// return false if something fails, else true
bool initializeSenseHat() {
	
	// Declaring variables for going through the directory
	struct dirent **namelist; 
	int num_fb; 


	// Scanning the directory entries with the given filter
	const char* dirname = "/dev/"; 
	num_fb = scandir(dirname, &namelist, &filterFrameBuffer, alphasort); 
	if (num_fb == -1) {
		printf("Scandir failed at %s\n", dirname); 	
		return false; 
	}
	
	int foundFrameBuffer = 0; 

	// Going through all the values to see if we can find a value with the correct identifcation
	for (int i = 0; i < num_fb; i++) {

		// Creating the path so we can open it
		static char path[256]; 
		snprintf(path, sizeof(path), "%s%s", dirname, namelist[i]->d_name); 

		fb_fd = open(path, O_RDWR); 
		if (fb_fd < 0) {
			printf("Failed to pen framebuffer with path: %s\n", path); 
			return false; 
		}
		// Getting the fixed screen info
		if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix_info) < 0) {
			printf("Failed to get fixed screen info from framebuffer\n");
			return false; 
		}

		// Checking the identification
		if (strcmp(fb_fix_info.id, "RPi-Sense FB") != 0) {
			continue; 
		}

		// Getting the variable screen info
		if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var_info) < 0) {
			printf("Failed to get variable screen info from framebuffer\n"); 
			return false; 
		}
		foundFrameBuffer = 1;
		break; 
	}

	if (!foundFrameBuffer) return false; 

	// Freeing up memory
	for (int i = 0; i < num_fb; i++) {
		free(namelist[i]); 	
	}
	free(namelist); 

	// Map the device to memory
	screensize = fb_fix_info.line_length * fb_var_info.yres; 
	fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0); 
	close(fb_fd); // Close file descriptor as we've mapped the fb to memory
	if (fbp == NULL) 
	{	
		printf("Could not map the framebuffer to memory\n"); 
		return false; 
	}

	// Finding the joystick with the same method as above
	int num_events; 
	struct dirent **eventlist; 

	// Scanning the directory for events with the format "eventx" where x is a number
	const char* event_dirname = "/dev/input/"; 
	num_events = scandir(event_dirname, &eventlist, &filterJoystick, alphasort); 
	if (num_events == -1) {
		printf("Scandir failed in %s\n", event_dirname); 
		return false; 
	}

	int foundJoystick = 0; 

	for (int i = 0; i < num_events; i++) {
		// Getting the path and opening the file
		static char path[256]; 
		snprintf(path, sizeof(path), "%s%s", event_dirname, eventlist[i]->d_name); 
		js_fd = open(path, O_RDWR); 
		if (js_fd < 0) {
			continue; 
		}
		// Getting the name of the joystick
		char joystick_name[256];  
		if (ioctl(js_fd, EVIOCGNAME(sizeof(joystick_name)), joystick_name) < 0) {
			continue; 
		}
		if (strcmp(joystick_name, "Raspberry Pi Sense HAT Joystick") != 0) {
			continue; 
		}
		foundJoystick = 1;
		break; 
	}

	if (!foundJoystick) {
		printf("Failed to find joystick\n"); 	
		return false;
	} 


	// Freeing up memory after scandir
	for (int i = 0; i < num_events; i++) {
		free(eventlist[i]); 	
	}
	free(eventlist); 

	// Setting the color map to all black
	memset(colorMap, 7, sizeof(colorMap)); 

  return true;
}


// This function is called when the application exits
// Here you can free up everything that you might have opened/allocated
void freeSenseHat() {

	// Freeing up the frame buffer and the joystick
	munmap(fbp, screensize); 
	close(js_fd);
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed you MUST return 0 !!!
int readSenseHatJoystick() {
	// Creating variables that can store information from the poll
	struct pollfd evpoll = {
		.events = POLLIN,
		.fd = js_fd 
	}; 
	struct input_event ev[1]; 

	// Emptying the queue until a significant keypress is found
	while (poll(&evpoll, 1, 0) > 0) {
		int rd = read(evpoll.fd, ev, sizeof(struct input_event)); 
		if (rd <= 0) return 0; 
		if (ev->type == EV_KEY && ev->value != 0) {
			return ev->code; 
		}
	}

	return 0;

}


// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged) {
  (void) playfieldChanged;
  if (!playfieldChanged) return; 

  // Going through all the coordinates
  for (int x_cor = 0; x_cor < fb_var_info.xres; x_cor++) {
	  for (int y_cor = 0; y_cor < fb_var_info.yres; y_cor++) {
		// Finding the memory position
 		int location = (x_cor + fb_var_info.xoffset) * (fb_var_info.bits_per_pixel / 8) + (y_cor + fb_var_info.yoffset) * fb_fix_info.line_length; 
		uint16_t *pixel = (uint16_t*)(fbp + location);

		// Setting the pixel to the respective value in the color map
		int position = x_cor + 8*y_cor; 
		int value = colorMap[position]; 
		*pixel = getColor(value); 
     }
  }
}


// The game logic uses only the following functions to interact with the playfield.
// if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface

static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
}

static inline void copyTile(coord const to, coord const from) {
  memcpy((void *) &game.playfield[to.y][to.x], (void *) &game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  memcpy((void *) &game.playfield[to][0], (void *) &game.playfield[from][0], sizeof(tile) * game.grid.x);

}

static inline void resetTile(coord const target) {
  memset((void *) &game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *) &game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}


static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}

// Below here comes the game logic. Keep in mind: You are not allowed to change how the game works!
// that means no changes are necessary below this line! And if you choose to change something
// keep it compatible with what was provided to you!

bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  addTileToMap(game.activeTile); // Updating the color map
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    moveTileInMap(game.activeTile, newTile); // Updating the color map
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    moveTileInMap(game.activeTile, newTile);  // Updating the color map
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    moveTileInMap(game.activeTile, newTile);  // Updating the color map
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    for (unsigned int y = game.grid.y - 1; y > 0; y--) {
      copyRow(y, y - 1);
    }
    resetRow(0);
    removeBottomRow(); // Updating the color map
    return true;
  }
  return false;
}

void advanceLevel() {
  game.level++;
  switch(game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
  clearScreen(); 
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}


bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch(key) {
      case KEY_LEFT:
        moveLeft();
        break;
      case KEY_RIGHT:
        moveRight();
        break;
      case KEY_DOWN:
        while (moveDown()) {};
        game.tick = 0;
        break;
      default:
        playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
        } else {
          gameOver();
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
       .fd = STDIN_FILENO,
       .events = POLLIN
  };
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
 exit:
    switch (lkey) {
      case 10: return KEY_ENTER;
      case 65: return KEY_UP;
      case 66: return KEY_DOWN;
      case 67: return KEY_RIGHT;
      case 68: return KEY_LEFT;
    }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x ++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
      case 0:
        fprintf(stdout, "| Tiles: %10u\n", game.tiles);
        break;
      case 1:
        fprintf(stdout, "| Rows:  %10u\n", game.rows);
        break;
      case 2:
        fprintf(stdout, "| Score: %10u\n", game.score);
        break;
      case 4:
        fprintf(stdout, "| Level: %10u\n", game.level);
        break;
      case 7:
        fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
        break;
    default:
        fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}


inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  }

  // Allocate the playing field structure
  game.rawPlayfield = (tile *) malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile**) malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  // Start with gameOver
  gameOver();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  };

  // Clear console, render first time
  fprintf(stdout, "\033[H\033[J");
  renderConsole(true);
  renderSenseHatMatrix(true);

  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick();
    if (!key)
      key = readKeyboard();
    if (key == KEY_ENTER)
      break;

    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    renderSenseHatMatrix(playfieldChanged);

    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }

  freeSenseHat();
  free(game.playfield);
  free(game.rawPlayfield);

  return 0;
}
