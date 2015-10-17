#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "ptask.h"

#include <allegro.h>

//---------------------------------------------------------------------------
//GLOBAL CONSTANTS
//---------------------------------------------------------------------------

#define WINDOW_HEIGHT		600
#define WINDOW_WIDTH		800

#define FOOD_BASE_RADIUS		20
#define FOOD_DETECTION_RADIUS	(FOOD_BASE_RADIUS + 20)
#define FOOD_BASE_ODOR			(FOOD_DETECTION_RADIUS * FOOD_DETECTION_RADIUS)
#define MAX_FOOD_NUM			5

#define ANT_COLOR				12
#define MAX_ANTS				10

// formula: odor = (base_odor * odor_scale)/ant_distance_from_food^2


//---------------------------------------------------------------------------
//TYPE DEFINITIONS
//---------------------------------------------------------------------------

struct food_t
{
	int 	x;
	int 	y;
	int 	quantity;
};

struct ant_t
{
	float 	x;
	float 	y;
	float 	speed;
	float	angle;
};

//---------------------------------------------------------------------------
//FUNCTION DECLARATIONS
//---------------------------------------------------------------------------

void put_food(void);
void setup(void);
void process_inputs(void);
void setup_food(void);
char get_scan_code(void);

void * ant_task(void *);


//---------------------------------------------------------------------------
//GLOBAL VARIABLES
//---------------------------------------------------------------------------

BITMAP *buffer; 			//buffer for double buffering
int		mouse_prev = 0;

int 			food_x = 0;
int 			food_y = 0;
bool 			should_put_food = false;
struct food_t 	food_list[MAX_FOOD_NUM] = {{0}};
struct ant_t 	ant_list[MAX_ANTS] = {{0}};
int 			nAnts = 0;

//---------------------------------------------------------------------------
//FUNCTION DEFINITIONS
//---------------------------------------------------------------------------

int main(int argc, char * argv[])
{
pthread_t	tid;
bool		running = true;
char		scan;

struct task_par tp;

	setup();

	tp.arg = 0;
	tp.period = 20;
	tp.deadline = 20;
	tp.priority = 10;

	tid = task_create(ant_task, &tp);

	while (running)
	{
		process_inputs();

		put_food();


		if (key[KEY_ESC])
			running = false;
	}
	
	allegro_exit();
	return 0;
}


void process_inputs(void)
{
	// mouse
	should_put_food = (!(mouse_prev & 1) && (mouse_b & 1));
	food_x = mouse_x;
	food_y = mouse_y;

	mouse_prev = mouse_b;
}


void put_food(void)
{
int 	i;

	if (should_put_food)
	{	
		scare_mouse();
		circlefill(buffer, food_x, food_y, FOOD_BASE_RADIUS, 10);
		blit(buffer, screen, 0, 0, 0, 0, buffer->w, buffer->h);
		unscare_mouse();

		for (i = 0; i < MAX_FOOD_NUM; i++)
		{
			if (food_list[i].quantity == 0)
			{
				food_list[i].x = food_x;
				food_list[i].y = food_y;
				food_list[i].quantity = FOOD_BASE_RADIUS / 2;
				break;
			}
		}
	}

}

void setup(void)
{
	allegro_init();
	install_keyboard();
	install_mouse();

	set_color_depth(8);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, WINDOW_WIDTH, WINDOW_HEIGHT,0,0);

	show_mouse(screen);

	buffer = create_bitmap(WINDOW_WIDTH, WINDOW_HEIGHT);
	clear_bitmap(buffer);
	clear_to_color(buffer, 0);
}

char get_scan_code(void)
{
	if (keypressed())
		return readkey() >> 8;
	else 
		return 0;
}


void * ant_task(void * arg)
{
struct task_par * tp = (struct task_par *) arg;
struct ant_t * ant = &ant_list[tp->arg];

	printf("qui1\n");

	set_period(tp);

	printf("qui2\n");

	while(1)
	{
		printf("!\n");

		ant->x += 0.1;
		ant->y = 100;

		scare_mouse();
		circlefill(buffer, ant->x, ant->y, 5, 12);
		blit(buffer, screen, 0, 0, 0, 0, buffer->w, buffer->h);
		unscare_mouse();

		if (deadline_miss(tp)) exit(-1);
		wait_for_period(tp);
	}
} 

