#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <semaphore.h>
#include <math.h>

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
#define	DELTA_ANGLE				10		//max angle deviation
#define	DELTA_SPEED				0.1		//max_speed deviation
#define	ANT_PERIOD				0.02
#define ANT_SPEED				30
#define ANT_RADIUS				5

#define NEST_RADIUS				40
#define NEST_COLOR				13



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

struct nest_t
{
	float	x;
	float	y;
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
void * gfx_task(void *);

float frand(float, float);
void put_nest(void);

void bounce(struct ant_t *);


//---------------------------------------------------------------------------
//GLOBAL VARIABLES
//---------------------------------------------------------------------------

BITMAP *buffer; 			//buffer for double buffering
int		mouse_prev = 0;

int 				food_x = 0;
int 				food_y = 0;
bool 				should_put_food = false;
struct food_t 		food_list[MAX_FOOD_NUM] = {{0}};

struct ant_t 		ant_list[MAX_ANTS] = {{0}};
int 				nAnts = 0;

struct nest_t		nest;

pthread_t 			tid[MAX_ANTS];
struct task_par		tp[MAX_ANTS];
pthread_attr_t		attr[MAX_ANTS];

bool				running = true;

pthread_t           gfx_tid;
struct task_par     gfx_tp;



//---------------------------------------------------------------------------
//FUNCTION DEFINITIONS
//---------------------------------------------------------------------------

int main(int argc, char * argv[])
{
	setup();

	while (running)
	{
		process_inputs();

		put_food();
	}
	
	allegro_exit();
	return 0;
}


void process_inputs(void)
{
char	scan;

	// mouse
	should_put_food = (!(mouse_prev & 1) && (mouse_b & 1));
	food_x = mouse_x;
	food_y = mouse_y;

	mouse_prev = mouse_b;

	// keyboard
	scan = get_scan_code();

	switch(scan)
	{
		case KEY_ESC:
			running = false;
			break;
		case KEY_SPACE:
		{
			if(nAnts < MAX_ANTS)
			{
				tp[nAnts].arg = nAnts;
				tp[nAnts].period = 20;
				tp[nAnts].deadline = 60;
				tp[nAnts].priority = 10;

				tid[nAnts] = task_create(ant_task, &tp[nAnts]);

				nAnts++;
			}	
			break;
		}

		default: 
			break;
	}
			
}

//-----------------------------------------------------------------------
//PUT FOOD ON MOUSE CLICK
//-----------------------------------------------------------------------


void put_food(void)
{
int 	i;

	if (should_put_food)
	{
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

//------------------------------------------------------------------------
// SETUP THE ENVIRONMENT
//------------------------------------------------------------------------

void setup(void)
{
	//allegro setup

	allegro_init();
	install_keyboard();
	install_mouse();

	set_color_depth(8);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, WINDOW_WIDTH, WINDOW_HEIGHT,0,0);

	show_mouse(screen);

	//double buffering

	buffer = create_bitmap(WINDOW_WIDTH, WINDOW_HEIGHT);
	clear_bitmap(buffer);
	clear_to_color(buffer, 0);

	srand(time(NULL));

	// create graphics task
	gfx_tp.arg = 0;
	gfx_tp.period = 20;
	gfx_tp.deadline = 50;
	gfx_tp.priority = 10;

	gfx_tid = task_create(gfx_task, &gfx_tp);

	//create nest

	put_nest();
}

char get_scan_code(void)
{
	if (keypressed())
		return readkey() >> 8;
	else 
		return 0;
}

//---------------------------------------------------------------------
// UPDATE ANTS
//---------------------------------------------------------------------


void * ant_task(void * arg)
{
float	da, vx, vy;
struct task_par * tp = (struct task_par *) arg;
struct ant_t * ant = &ant_list[tp->arg];

	ant->x = nest.x; 
	ant->y = nest.y; 
	ant->speed = ANT_SPEED;
	ant->angle = (frand(0,360)) * M_PI/180;

	set_period(tp);

	while(1)
	{
		da = frand(-DELTA_ANGLE, DELTA_ANGLE) * M_PI/180;

		ant->angle += da;

		vx = ant->speed * cos(ant->angle);
		vy = ant->speed * sin(ant->angle);

		ant->x += vx * ANT_PERIOD;
		ant->y += vy * ANT_PERIOD;

		bounce(ant);

		if (deadline_miss(tp)) printf("deadline miss\n");
		wait_for_period(tp);
	}
} 


//----------------------------------------------------------------------
// UPDATE GRAPHIC
//----------------------------------------------------------------------


void * gfx_task(void * arg)
{
struct task_par *tp = (struct task_par *) arg;

int i;

	set_period(tp);

	while(1)
	{
		scare_mouse();

		clear_to_color(buffer, 0);

		//draw nest

		circlefill(buffer, nest.x, nest.y, NEST_RADIUS, 10);

		//draw ants on buffer

		for (i = 0; i < nAnts; i++)
			circlefill(buffer, ant_list[i].x, ant_list[i].y, ANT_RADIUS, 12);

		//draw food on buffer

		for (i = 0; i < MAX_FOOD_NUM; i++)
			if (food_list[i].quantity > 0)
				circlefill(buffer, food_list[i].x, food_list[i].y, FOOD_BASE_RADIUS, 10);

		//put buffer on the screen

		blit(buffer, screen, 0, 0, 0, 0, buffer->w, buffer->h);

		unscare_mouse();

		if (deadline_miss(tp)) exit(-1);
		wait_for_period(tp);
	}
}

//--------------------------------------------------------------------
// returns a random float in [xmi, xma)
//--------------------------------------------------------------------

float frand(float xmi, float xma)
{
float 	r;
	
	r = (float)rand()/(float)RAND_MAX;
	return xmi + (xma - xmi) * r;
}


//---------------------------------------------------------------------
// puts ants nest on the environment
//---------------------------------------------------------------------

void put_nest(void)
{
	nest.x = frand(NEST_RADIUS * 2, WINDOW_HEIGHT - NEST_RADIUS);
	nest.y = frand(NEST_RADIUS * 2, WINDOW_WIDTH - NEST_RADIUS);
}

void bounce(struct ant_t * ant)
{
	if(ant->x <= ANT_RADIUS)
	{
		ant->angle += (90 * (M_PI/180));
	}

	if(ant->x > (WINDOW_WIDTH - ANT_RADIUS))
	{
		ant->angle -= (90 * (M_PI/180));
	}

	if(ant->y > (WINDOW_HEIGHT - ANT_RADIUS))
	{
		ant->angle += (90 * (M_PI/180));
	}

	if(ant->y < ANT_RADIUS)
	{
		ant->angle -= (90 * (M_PI/180));
	}


}