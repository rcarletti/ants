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

#define WINDOW_HEIGHT			768
#define WINDOW_WIDTH			1024
#define BACKGROUND_WIDTH		800
#define BACKGROUND_HEIGHT		600

#define FOOD_BASE_RADIUS		40										//grandezza base dell'immagine della pila di cibo
#define FOOD_DETECTION_RADIUS	(FOOD_BASE_RADIUS + 20)
#define FOOD_BASE_ODOR			(FOOD_DETECTION_RADIUS * FOOD_DETECTION_RADIUS)
#define MAX_FOOD_NUM			5										//numero massimo di pile di cibo
#define MAX_FOOD_QUANTITY		10										//numero massimo di cibo per pila	

#define MAX_ANTS				20		//max numero di formiche
#define	DELTA_ANGLE				5		//max angle deviation
#define	DELTA_SPEED				0.1		//max_speed deviation
#define	ANT_PERIOD				0.02
#define ANT_SPEED				20
#define ANT_RADIUS				8

#define NEST_RADIUS				40

#define PHEROMONE_RADIUS		20													//raggio dei feromoni
#define	MAX_PHEROMONE_INTENSITY	10													//intensità dei feromoni
#define X_NUM_CELL				(int)BACKGROUND_WIDTH / MAX_PHEROMONE_INTENSITY		//numero di celle sull'asse x
#define	Y_NUM_CELL				(int)BACKGROUND_HEIGHT / MAX_PHEROMONE_INTENSITY	//numero di celle sull'asse y




// formula: odor = (base_odor * odor_scale)/ant_distance_from_food^2


//---------------------------------------------------------------------------
//TYPE DEFINITIONS
//---------------------------------------------------------------------------

struct food_t
{
	int 	x;						//center coord
	int 	y;						//center coord
	int 	quantity;
	int 	radius;

};

struct ant_t
{
	float 	x;
	float 	y;
	float 	speed;
	float	angle;
	bool 	has_food;
};

struct nest_t
{
	float	x;
	float	y;
};

struct cell_t 
{
	float x;			//center of the cell
	float y;
	int odor_intensity;
};
//---------------------------------------------------------------------------
//FUNCTION DECLARATIONS
//---------------------------------------------------------------------------

void put_food(void);
void draw_food(void);
void setup(void);
void process_inputs(void);
void setup_food(void);
char get_scan_code(void);

void draw_ants(void);

void * ant_task(void *);
void * gfx_task(void *);
void * pheromone_task(void *);

float frand(float, float);
void put_nest(void);

void bounce(struct ant_t *);

float deg_to_rad(float);
float rad_to_deg(float);

void check_for_food(struct ant_t *);
float distance(struct ant_t *, float, float);

void head_to_the_nest(struct ant_t *);
void check_nest(struct ant_t *);

void setup_grid(void);

void release_pheromone(struct ant_t *);

void draw_pheromone(void);


//---------------------------------------------------------------------------
//GLOBAL VARIABLES
//---------------------------------------------------------------------------

BITMAP *buffer; 			//buffer for double buffering
int		mouse_prev = 0;		//previous mouse value

int 				food_x = 0;					//food center coord
int 				food_y = 0;
bool 				should_put_food = false;
struct food_t 		food_list[MAX_FOOD_NUM] = {{0}};		
int 				n_food = 0;

struct ant_t 		ant_list[MAX_ANTS] = {{0}};
int 				nAnts = 0;

struct nest_t		nest;

pthread_t 			tid[MAX_ANTS];
struct task_par		tp[MAX_ANTS];
pthread_attr_t		attr[MAX_ANTS];

bool				running = true;

pthread_t           gfx_tid;
struct task_par     gfx_tp;

pthread_t           ph_tid;
struct task_par     ph_tp;

struct cell_t 		grid[X_NUM_CELL][Y_NUM_CELL];			//griglia dello sfondo


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

	food_x = mouse_x;
	food_y = mouse_y;
	if(food_x < (BACKGROUND_WIDTH - FOOD_BASE_RADIUS) && 
		food_y > (WINDOW_HEIGHT - BACKGROUND_HEIGHT + FOOD_BASE_RADIUS) &&
		food_x > FOOD_BASE_RADIUS && 
		food_y < (WINDOW_HEIGHT - FOOD_BASE_RADIUS))
			should_put_food = (!(mouse_prev & 1) && (mouse_b & 1));

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
			if (nAnts < MAX_ANTS)
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
				food_list[i].quantity = MAX_FOOD_QUANTITY;
				food_list[i].radius = FOOD_BASE_RADIUS;
				n_food++;
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

	set_color_depth(24);

	set_color_conversion(COLORCONV_8_TO_24);

	set_gfx_mode(GFX_AUTODETECT_WINDOWED, WINDOW_WIDTH, WINDOW_HEIGHT,0,0);

	show_mouse(screen);

	//double buffering

	buffer = create_bitmap(WINDOW_WIDTH, WINDOW_HEIGHT);
	clear_bitmap(buffer);
	clear_to_color(buffer, 0);

	setup_grid();

	srand(time(NULL));

	// create graphics task
	gfx_tp.arg = 0;
	gfx_tp.period = 80;
	gfx_tp.deadline = 80;
	gfx_tp.priority = 10;

	gfx_tid = task_create(gfx_task, &gfx_tp);

	ph_tp.arg = 0;
	ph_tp.period = 800;
	ph_tp.deadline = 800;
	ph_tp.priority = 10;

	ph_tid = task_create(pheromone_task, &ph_tp);


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
	ant->angle = deg_to_rad(frand(0,360));
	ant->has_food = false;

	set_period(tp);

	while(1)
	{
		if (!ant->has_food)
		{
			da = deg_to_rad(frand(-DELTA_ANGLE, DELTA_ANGLE));
			ant->angle += da;	

			check_for_food(ant);
		}

		else if (ant->has_food)
		{
			head_to_the_nest(ant);
			release_pheromone(ant);
		}

		vx = ant->speed * cos(ant->angle);
		vy = ant->speed * sin(ant->angle);

		ant->x += vx * ANT_PERIOD;
		ant->y += vy * ANT_PERIOD;

		bounce(ant);
		check_nest(ant);

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

BITMAP * ground;

BITMAP * nest_image;

	ground = load_bitmap("ground2.bmp", NULL);
	if(ground == NULL)
		{
			printf("errore ground \n");
			exit(1);
		}

	nest_image = load_bitmap("nest3.bmp", NULL);
	if(nest_image == NULL)
	{
		printf("errore nest \n");
		exit(1);
	}

	set_period(tp);

	while(1)
	{
		scare_mouse();

		clear_to_color(buffer, 0);

		draw_sprite(buffer, ground, 0, WINDOW_HEIGHT - BACKGROUND_HEIGHT);						//draw ground

		draw_sprite(buffer, nest_image, nest.x - NEST_RADIUS, nest.y - NEST_RADIUS);			//draw nest

		draw_food();

		draw_pheromone();

		draw_ants();
	
		blit(buffer, screen, 0, 0, 0, 0, buffer->w, buffer->h);

		unscare_mouse();

		if (deadline_miss(tp)) printf("deadline miss gfx\n");
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

	nest.x = frand(NEST_RADIUS * 2, BACKGROUND_WIDTH - NEST_RADIUS * 2);
	nest.y = frand(WINDOW_HEIGHT - BACKGROUND_HEIGHT + (NEST_RADIUS * 2),WINDOW_HEIGHT - (NEST_RADIUS * 2));
}

//---------------------------------------------------------------------
// controlla se la formica raggiunge la fine dello sfondo e la fa
// tornare indietro
//---------------------------------------------------------------------

void bounce(struct ant_t * ant)
{
	if(ant->x <= ANT_RADIUS)									//left side
	{
		ant->angle +=deg_to_rad(90);
	}

	if(ant->x > (BACKGROUND_WIDTH - ANT_RADIUS))				//right side
	{
		ant->angle -= deg_to_rad(90);
	}

	if(ant->y < (WINDOW_HEIGHT - BACKGROUND_HEIGHT + ANT_RADIUS))			//up side?????
	{
		ant->angle += deg_to_rad(90);
	}

	if(ant->y > WINDOW_HEIGHT - ANT_RADIUS * 2)					//bottom side
	{
		ant->angle -= deg_to_rad(90);
	}


}

//---------------------------------------------------------------------
// converte i gradi in radianti
//---------------------------------------------------------------------

float deg_to_rad(float angle)
{
	angle = angle * M_PI/180;
	return angle;
}

//---------------------------------------------------------------------
// converte i radianti in gradi
//---------------------------------------------------------------------

float rad_to_deg(float angle)
{
	angle = angle * 180 / M_PI;
	return angle;
}


//---------------------------------------------------------------------
// controlla se la formica ha trovato cibo
//---------------------------------------------------------------------


void check_for_food(struct ant_t * ant)
{
int i;

	for(i = 0; i < n_food; i++)
	{
		if(distance(ant, food_list[i].x, food_list[i].y) < FOOD_BASE_RADIUS)
		{
			ant->has_food = true;
			food_list[i].quantity--;
		}
	}
}

//---------------------------------------------------------------------
// calcola la distanza fra la formica e il cibo
//---------------------------------------------------------------------


float distance(struct ant_t * ant, float x, float y)
{
int distance_x, distance_y, distance;

	distance_x = ((ant->x - x) * (ant->x - x));
	distance_y = ((ant->y - y) * (ant->y - y));
	distance = sqrt(distance_y + distance_x);
	return distance;
}

void head_to_the_nest(struct ant_t * ant)
{
float x, y, alpha;

		x = (nest.x - ant->x);
		y = (nest.y - ant->y);
		alpha = atan2(y,x) ;
		ant->angle = alpha;
}

void check_nest(struct ant_t * ant)
{
	if(distance(ant, nest.x, nest.y) < 2)
			ant->has_food = false;	
}

void draw_food(void)
{
int i;

	BITMAP * food;

	food = load_bitmap("sugar.bmp", NULL);
	if(food == NULL)
	{
		printf("errore food \n");
		exit(1);
	}

	for (i = 0; i < MAX_FOOD_NUM; i++)
			if (food_list[i].quantity > 0)
			{
				stretch_sprite(buffer, food, 
				food_list[i].x - FOOD_BASE_RADIUS,
				food_list[i].y - FOOD_BASE_RADIUS,
				(float)food_list[i].quantity / MAX_FOOD_QUANTITY * FOOD_BASE_RADIUS * 2,
				(float)food_list[i].quantity / MAX_FOOD_QUANTITY * FOOD_BASE_RADIUS * 2);	
			}
			
				
}

void draw_ants(void)
{
int i;
BITMAP * ant;
float angle;

	ant = load_bitmap("ant.bmp", NULL);

	if(ant == NULL)
	{
		printf("errore ant \n");
		exit(1);
	}

	for (i = 0; i < nAnts; i++)
		{
			angle = ((rad_to_deg(ant_list[i].angle + M_PI_2) * 256 / 360));				//converting degrees in allegro-degrees

			rotate_sprite(buffer, ant, ant_list[i].x - ANT_RADIUS, ant_list[i].y - ANT_RADIUS, ftofix(angle));			
		}
}

void setup_grid()
{
int i, j;

	for (i = 0; i < X_NUM_CELL; i++)
		for (j = 0; j < Y_NUM_CELL; j++)
		{
			grid[i][j].x = i * PHEROMONE_RADIUS + PHEROMONE_RADIUS;
			grid[i][j].y = j * PHEROMONE_RADIUS + PHEROMONE_RADIUS;
			grid[i][j].odor_intensity = 0;
		}
}

void release_pheromone(struct ant_t * ant)
{
int x, y;
	
	x = ant->x / PHEROMONE_RADIUS;
	y = ant->y / PHEROMONE_RADIUS;
	grid[x][y].odor_intensity = MAX_PHEROMONE_INTENSITY;
}

void draw_pheromone(void)
{
int i, j;
BITMAP * trail;
int yellow = makecol(238,240,39);

	trail = load_bitmap("scia.bmp", NULL);

	for (i = 0; i < X_NUM_CELL; i++)
		for (j = 0; j < Y_NUM_CELL; j++)
			if (grid[i][j].odor_intensity > 0)
				stretch_sprite(buffer, trail, grid[i-1][j-1].x, grid[i-1][j-1].y, PHEROMONE_RADIUS * 2, PHEROMONE_RADIUS * 2);

}

void * pheromone_task(void * arg)
{
int i, j;
struct task_par *tp = (struct task_par *) arg;

	set_period(tp);

	while(1)
	{
		for (i = 0; i < X_NUM_CELL; i++)
			for (j = 0; j < Y_NUM_CELL; j++)
				if (grid[i][j].odor_intensity > 0)
					grid[i][j].odor_intensity--;

	if (deadline_miss(tp)) printf("deadline miss gfx\n");
		wait_for_period(tp);
	}

}