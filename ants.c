#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <semaphore.h>
#include <math.h>
#include "utils.h"

#include "ptask.h"

#include <allegro.h>

//---------------------------------------------------------------------------
//GLOBAL CONSTANTS
//---------------------------------------------------------------------------

#define WINDOW_HEIGHT			600
#define WINDOW_WIDTH			1200
#define BACKGROUND_WIDTH		0.8	//[m]
#define BACKGROUND_HEIGHT		0.6	//[m]

#define FOOD_BASE_ODOR			(FOOD_DETECTION_RADIUS * FOOD_DETECTION_RADIUS)

#define SCALE 					1000.0	//fattore di scala che converte metri in pixel

#define MAX_FOOD_NUM			5	//numero massimo di pile di cibo
#define MAX_FOOD_QUANTITY		10	//numero massimo di cibo per pila
#define FOOD_SCALE              0.008   //radius = quantity * scale	

#define MAX_NUM_ANTS			20		//numero di formiche worker
#define DELTA_ANGLE				5		//max angle deviation
#define ANT_PERIOD				0.02
#define ANT_SPEED				0.02 
#define ANT_RADIUS				0.008

#define NEST_RADIUS				0.040

#define MAX_PHEROMONE_INTENSITY 15
#define PHEROMONE_INTENSITY     0.2
#define PHEROMONE_TASK_PERIOD  	5000

#define CELL_SIDE				0.01
#define X_NUM_CELL				(80)
#define Y_NUM_CELL				(60)

#define	MAX_NUM_SCOUTS			10

#define ANT_TYPE_SCOUT          0
#define ANT_TYPE_WORKER         1

//---------------------------------------------------------------------------
//TYPE DEFINITIONS
//---------------------------------------------------------------------------
enum state_t
{
	ANT_IDLE,
	ANT_AWAKING,
	ANT_TOWARDS_FOOD, 
	ANT_TOWARDS_HOME_NO_FOOD, 
	ANT_TOWARDS_HOME_WITH_FOOD,
	ANT_TOWARDS_UNKNOWN,
	ANT_RANDOM_MOVEMENT
};


struct food_t
{
	double 	x;						//coordinate del centro
	double 	y;						//coordinate del centro
	int 	quantity;

};

struct ant_t
{
	int     type;

	double 	x, y;
	double 	speed;
	double	angle;
	double  pheromone_intensity;

	enum state_t state;

	bool 	carrying_food, following_trail;
	bool 	inside_nest;

	struct timespec last_release;
};


struct nest_t
{
	double	x;
	double	y;
};

struct cell_t 
{
	double x;			//center of the cell
	double y;
	double odor_intensity;
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
void draw_scouts(void);
void draw_interface(void);

void * ant_task(void *);
void * gfx_task(void *);
void * pheromone_task(void *);
void * scout_task(void *);

void put_nest(void);

void bounce(struct ant_t *);

bool look_for_food(struct ant_t *);
bool look_for_trail(struct ant_t *);

bool sense_nest(struct ant_t *);
bool check_nest(struct ant_t *);

void setup_grid(void);

void release_pheromone(struct ant_t *);

void draw_pheromone(void);

bool follow_trail(struct ant_t *);

bool sense_food(struct ant_t *);

double angle_towards(struct ant_t *, double, double);
void head_towards(struct ant_t * , double, double);


//---------------------------------------------------------------------------
//GLOBAL VARIABLES
//---------------------------------------------------------------------------

BITMAP *buffer; 			//buffer per il double buffering
int		mouse_prev = 0;		//valore precedente del mouse

double 				food_x = 0;					//food center coord
double 				food_y = 0;
bool 				should_put_food = false;
struct food_t 		food_list[MAX_FOOD_NUM] = {{0}};		
int 				n_food = 0;

struct ant_t 		ant_list[MAX_NUM_ANTS] = {{0}};
int 				nAnts = 0;

struct nest_t		nest;

pthread_t 			tid[MAX_NUM_ANTS];
struct task_par		tp[MAX_NUM_ANTS];
pthread_attr_t		attr[MAX_NUM_ANTS];

pthread_t 			scouts_tid[MAX_NUM_SCOUTS];
struct task_par		scouts_tp[MAX_NUM_SCOUTS];
pthread_attr_t		scouts_attr[MAX_NUM_SCOUTS];
struct ant_t 		scout_list[MAX_NUM_SCOUTS] = {{0}};
int 				nScouts = 0;

bool				running = true;			//simulazione attiva

pthread_t           gfx_tid;
struct task_par     gfx_tp;

pthread_t           ph_tid;
struct task_par     ph_tp;

struct cell_t 		grid[X_NUM_CELL][Y_NUM_CELL];	//griglia dello sfondo


struct timespec 	global_t;

int 				deadline_miss_num = 0;			//conta i deadline miss 

pthread_mutex_t		grid_mux = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t		food_mux = PTHREAD_MUTEX_INITIALIZER;

int 				ant_inside_nest = 0;


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

//------------------------------------------------------------------------
// inizializza lo scenario
//------------------------------------------------------------------------

void setup(void)
{
int j;
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

	// creazione thread grafico
	gfx_tp.arg = 0;
	gfx_tp.period = 60;
	gfx_tp.deadline = 60;
	gfx_tp.priority = 10;

	gfx_tid = task_create(gfx_task, &gfx_tp);

	//creazione thread che decrementa i feromoni
	ph_tp.arg = 0;
	ph_tp.period = PHEROMONE_TASK_PERIOD;
	ph_tp.deadline = PHEROMONE_TASK_PERIOD;
	ph_tp.priority = 10;

	ph_tid = task_create(pheromone_task, &ph_tp);

	put_nest();

	pthread_mutex_lock(&food_mux);

	for (j = 0; j < MAX_FOOD_NUM; j++)
	{
		food_list[j].x = -1;
		food_list[j].y = -1;
	}

	pthread_mutex_unlock(&food_mux);



	
}

//---------------------------------------------------------------------
// fa il setup della griglia
//---------------------------------------------------------------------


void setup_grid()
{
int i, j;

	pthread_mutex_lock(&grid_mux);

	for (i = 0; i < X_NUM_CELL; i++)
		for (j = 0; j < Y_NUM_CELL; j++)
		{
			grid[i][j].x = i * CELL_SIDE + CELL_SIDE / 2;
			grid[i][j].y = j * CELL_SIDE + CELL_SIDE / 2;
			grid[i][j].odor_intensity = 0;
		}
	pthread_mutex_unlock(&grid_mux);
}


//---------------------------------------------------------------------
// puts ants nest on the environment
//---------------------------------------------------------------------

void put_nest(void)
{
	nest.x = BACKGROUND_WIDTH / 2.0;
	nest.y = BACKGROUND_HEIGHT / 2.0;

}

//---------------------------------------------------------------------
// processa gli input da mouse e tastiera
//---------------------------------------------------------------------

void process_inputs(void)
{
char	  scan;
const int FOOD_BASE_RADIUS = MAX_FOOD_QUANTITY * FOOD_SCALE;
int i;

	// salva le coordinate del click del mouse

	food_x = (double)mouse_x / SCALE;
	food_y = (double)mouse_y / SCALE;
	if (
		(food_x < (BACKGROUND_WIDTH - (FOOD_BASE_RADIUS / 2)) && food_x > (FOOD_BASE_RADIUS / 2)) && 
		(food_y < (BACKGROUND_HEIGHT - (FOOD_BASE_RADIUS / 2)) && food_y > (FOOD_BASE_RADIUS / 2))
		)
	{
		should_put_food = (!(mouse_prev & 1) && (mouse_b & 1));
	}

	mouse_prev = mouse_b;

	// input da tastiera
	scan = get_scan_code();

	switch(scan)
	{
		case KEY_ESC:
			running = false;
			break;
		case KEY_S:
		{	
				if(nScouts < MAX_NUM_SCOUTS)	
				{
					scouts_tp[nScouts].arg = nScouts;
					scouts_tp[nScouts].period = ANT_PERIOD * 1000;
					scouts_tp[nScouts].deadline = ANT_PERIOD * 1000;
					scouts_tp[nScouts].priority = 10;

					scouts_tid[nScouts] = task_create(scout_task, &scouts_tp[nScouts]);

					nScouts++;
				}
			 	
			break;
		}

		case KEY_W:
		{
			if(nAnts < MAX_NUM_ANTS)
			{
				tp[nAnts].arg = nAnts;
				tp[nAnts].period = ANT_PERIOD * 1000;
				tp[nAnts].deadline = ANT_PERIOD * 1000;
				tp[nAnts].priority = 10;

				tid[nAnts] = task_create(ant_task, &tp[nAnts]);

				nAnts++;	
			}

		}

		default: 
			break;
	}
}

//-----------------------------------------------------------------------
//crea il cibo nel punto in cui l'utente clicka col mouse
//-----------------------------------------------------------------------


void put_food(void)
{
int i;

	if (should_put_food)
	{
		pthread_mutex_lock(&food_mux);

		for (i = 0; i < MAX_FOOD_NUM; i++)
		{
			if (food_list[i].quantity == 0)
			{
				food_list[i].x = food_x;
				food_list[i].y = food_y;
				food_list[i].quantity = MAX_FOOD_QUANTITY;
				n_food++;
				break;
			}
		}

		pthread_mutex_unlock(&food_mux);
	}
}



char get_scan_code(void)
{
	if (keypressed())
		return readkey() >> 8;
	else 
		return 0;
}

//---------------------------------------------------------------------
// update ants
//---------------------------------------------------------------------


void * ant_task(void * arg)
{
double 	vx, vy;					// componenti del vettore velocità

struct task_par * tp = (struct task_par *) arg;
struct ant_t * ant = &ant_list[tp->arg];

struct timespec awake_after, t;

	ant->type = ANT_TYPE_WORKER;
	ant->state = ANT_IDLE;
	ant->x = nest.x; 
	ant->y = nest.y; 
	ant->speed = ANT_SPEED;
	ant->angle = deg_to_rad(frand(0,360));
	ant->pheromone_intensity = PHEROMONE_INTENSITY;
	ant->last_release.tv_sec = 0;

	ant->inside_nest = true;
	ant->following_trail = false;
	ant->carrying_food = false;

	set_period(tp);


	while(1)
	{
		printf("%d\n",ant_inside_nest );
		switch (ant->state)
		{
			case ANT_IDLE:
			ant_inside_nest++;
				
				//la formica è nel nido, aspetta di sentire una traccia
				if (follow_trail(ant))
				{
					if (ant_inside_nest != 1)
					{
						
						clock_gettime(CLOCK_MONOTONIC, &awake_after);
						time_add_ms(&awake_after, tp->arg * 1000);

						ant->state = ANT_AWAKING;
					}

					else 
					{
						ant_inside_nest--;
						ant->state = ANT_TOWARDS_FOOD;
					}
				}

				break;

			case ANT_AWAKING:

				clock_gettime(CLOCK_MONOTONIC, &t);

				//se è passato un tempo sufficiente esce dal nido

				if (time_cmp(awake_after, t) < 0)
				{
					ant->state = ANT_TOWARDS_FOOD;
					ant_inside_nest--;
				}

				break;

			case ANT_TOWARDS_FOOD:
			{
				//la formica sta andando verso il cibo
				//se è vicino al cibo ma non ci sono feromoni si muove random
				if (!sense_food(ant))
				{
					if(!follow_trail(ant))
						ant->state = ANT_RANDOM_MOVEMENT;
				}

				//altrimenti se ha trovato il cibo si gira

				else if (look_for_food(ant))
				{
					ant->angle += M_PI;

					follow_trail(ant);

					//se ritrova una traccia torna al nido, altrimenti si muove random

					if (ant->following_trail)
						ant->state = ANT_TOWARDS_HOME_WITH_FOOD;
					else
						ant->state = ANT_RANDOM_MOVEMENT;
				}

				break;
			}

			case ANT_TOWARDS_HOME_NO_FOOD:
			{
				//se la formica è al nido senza cibo, torna in idle
				if (sense_nest(ant) && check_nest(ant))
					ant->state = ANT_IDLE;

				else 
				{
					//se non è ancora al nido segue i feromoni
					follow_trail(ant);
					//se non ci sono feromoni si muove random
					if(!ant->following_trail)
					{
						ant->state = ANT_RANDOM_MOVEMENT;
					}
				}

				break;
			}

			case ANT_TOWARDS_HOME_WITH_FOOD:
			{
				//rinforza la scia di feromoni
				release_pheromone(ant);

				//fino a che non arriva al nido segue la traccia
				if (!sense_nest(ant))
				{
					follow_trail(ant);
				}
				else if (check_nest(ant))
				{
					//quando arriva al nido posa il cibo e, se presente, continua a seguire la traccia
					ant->carrying_food = false;

					follow_trail(ant);

					if (ant->following_trail)
						ant->state = ANT_TOWARDS_FOOD;
					else
						ant->state = ANT_RANDOM_MOVEMENT;
				}

				break;
			}

			case ANT_TOWARDS_UNKNOWN:
			{
				follow_trail(ant);

				if (sense_nest(ant) && check_nest(ant))
				{
					ant->carrying_food = false;
					if(follow_trail(ant))
						ant->state = ANT_TOWARDS_FOOD;
					else
						ant->state = ANT_IDLE;
				}
				else if (sense_food(ant) && !ant->carrying_food)
				{
					ant->state = ANT_TOWARDS_FOOD;
				}

				else if (sense_food(ant) && ant->carrying_food && ant->following_trail)
				{
					ant->state = ANT_TOWARDS_HOME_WITH_FOOD;
				}

				break;
			}	

			case ANT_RANDOM_MOVEMENT:

				if (follow_trail(ant))
					ant->state = ANT_TOWARDS_UNKNOWN;
				else if (sense_nest(ant) && check_nest(ant))
				{
					ant->carrying_food = false;
					ant->state = ANT_IDLE;
				}

				else if (!ant->carrying_food)
				{
					if(sense_food(ant))
					{
						look_for_food(ant);
					}
				}

				ant->angle += deg_to_rad(frand(-DELTA_ANGLE, DELTA_ANGLE));
							

				break;
			default:
				break;
		}

		if (ant->state != ANT_IDLE && ant->state != ANT_AWAKING)
		{
			bounce(ant);

			// aggiorna velocità e posizione
			vx = ant->speed * cos(ant->angle);
			vy = ant->speed * sin(ant->angle);

			ant->x += vx * ANT_PERIOD;
			ant->y += vy * ANT_PERIOD;

			
		}

		if (deadline_miss(tp))
		{
			printf("deadline miss ant\n");
			deadline_miss_num++;
		}	
		wait_for_period(tp);
	}

} 

//---------------------------------------------------------------------
// corpo del thread che fa muovere le scout
//---------------------------------------------------------------------

void * scout_task(void * arg)
{
double vx, vy, da;
int i;


struct task_par * tp = (struct task_par *) arg;
struct ant_t * scout = &scout_list[tp->arg];

	scout->type = ANT_TYPE_SCOUT;
	scout->x = nest.x; 
	scout->y = nest.y; 
	scout->speed = ANT_SPEED;
	scout->angle = deg_to_rad(frand(0,360));
	scout->pheromone_intensity = PHEROMONE_INTENSITY;
	scout->state = ANT_RANDOM_MOVEMENT;

	scout->following_trail = false;
	scout->carrying_food = false;

	set_period(tp);


	while(1)
	{

		switch (scout->state)
		{
			case ANT_RANDOM_MOVEMENT:
			{
				da = deg_to_rad(frand(-DELTA_ANGLE, DELTA_ANGLE));
                scout->angle += da;	

                if(sense_food(scout) && look_for_food(scout))
                	scout->state = ANT_TOWARDS_HOME_WITH_FOOD;

               	break;
            }

            case ANT_TOWARDS_HOME_WITH_FOOD:
            {
            	release_pheromone(scout);
            	head_towards(scout, nest.x, nest.y);
            	if(check_nest(scout))
            	{
            		scout->state = ANT_TOWARDS_FOOD;
            		scout->carrying_food = false;
            	}
            	break;
            }

            case ANT_TOWARDS_FOOD:
            {
				if (!sense_food(scout))
				{
					follow_trail(scout);

					if(!scout->following_trail)
					{
						scout->state = ANT_RANDOM_MOVEMENT;
					}
				}

				else if (look_for_food(scout))
				{
					scout->state = ANT_TOWARDS_HOME_WITH_FOOD;
				}

				break;
			}

			default: break;			
		
		}
		
		// aggiorna velocità e posizione
		vx = scout->speed * cos(scout->angle);
		vy = scout->speed * sin(scout->angle);

		scout->x += vx * ANT_PERIOD;
		scout->y += vy * ANT_PERIOD;

		bounce(scout);

		if (deadline_miss(tp)) 
		{
			printf("deadline miss scout\n");
			deadline_miss_num++;
		}
		wait_for_period(tp);
	}


}

//---------------------------------------------------------------------
// controlla se la formica ha trovato cibo
//---------------------------------------------------------------------


bool look_for_food(struct ant_t * ant)
{
int i;

	pthread_mutex_lock(&food_mux);

	for (i = 0; i < MAX_FOOD_NUM; i++)
	{

		double dist = distance(ant->x, ant->y, food_list[i].x, food_list[i].y);

		if (food_list[i].quantity > 0 && 
			dist < (food_list[i].quantity * FOOD_SCALE / 2)
		   )
		{			
			pthread_mutex_unlock(&food_mux);
			ant->following_trail = true;
			ant->carrying_food = true;
			food_list[i].quantity--;

			return true;
		}

		pthread_mutex_unlock(&food_mux);
	}

	return false;
}

//---------------------------------------------------------------------
// funzione che permette alle formiche di vedere il cibo
//---------------------------------------------------------------------

bool sense_food(struct ant_t * ant)
{
	int i;

	pthread_mutex_lock(&food_mux);

	for (i = 0; i < MAX_FOOD_NUM; i++)
	{
	double dist = distance(ant->x,ant->y, food_list[i].x, food_list[i].y);

		if (food_list[i].quantity > 0 && 
			dist < ((food_list[i].quantity * FOOD_SCALE / 2) + 0.020)
		   )
		{
			head_towards(ant, food_list[i].x , food_list[i].y);
			pthread_mutex_unlock(&food_mux);
			return true;
		}
	}

	pthread_mutex_unlock(&food_mux);

	return false;
}

//---------------------------------------------------------------------
// permette alle formiche di vedere il nido e dirigersi verso di esso
//---------------------------------------------------------------------

bool sense_nest(struct ant_t * ant)
{
	if (distance(ant->x, ant->y, nest.x, nest.y) < 0.040)
	{
		head_towards(ant, nest.x, nest.y);
		return true;
	}

	return false;
}

//---------------------------------------------------------------------
// controlla se la formica si trova nel nido
//---------------------------------------------------------------------

bool check_nest(struct ant_t * ant)
{
	if (distance(ant->x, ant->y, nest.x, nest.y) < 0.005)
		return true;

	return false;
}

//---------------------------------------------------------------------
// funzione che rilascia i feromoni
//---------------------------------------------------------------------

void release_pheromone(struct ant_t * ant)
{
int x, y;
struct timespec t;

	x = ant->x / CELL_SIDE;
	y = ant->y / CELL_SIDE;

	clock_gettime(CLOCK_MONOTONIC, &t);

	if ((t.tv_sec - ant->last_release.tv_sec) > -1)
	{	
		pthread_mutex_lock(&grid_mux);

		if (grid[x][y].odor_intensity < MAX_PHEROMONE_INTENSITY)
			grid[x][y].odor_intensity += ant->pheromone_intensity;

		pthread_mutex_unlock(&grid_mux);

		ant->last_release = t;

	}
	
}

//---------------------------------------------------------------------
// funzione che fa evaporare i feromoni
//---------------------------------------------------------------------

void * pheromone_task(void * arg)
{
int i, j;
struct task_par *tp = (struct task_par *) arg;

	set_period(tp);

	while(1)
	{
		pthread_mutex_lock(&grid_mux);

		for (i = 0; i < X_NUM_CELL; i++)
			for (j = 0; j < Y_NUM_CELL; j++)
				if (grid[i][j].odor_intensity > 0)
					grid[i][j].odor_intensity -= PHEROMONE_INTENSITY;

		pthread_mutex_unlock(&grid_mux);

		if (deadline_miss(tp)) 
		{
			printf("deadline miss pheromone\n");
			deadline_miss_num++;
		}
		wait_for_period(tp);

	}
}

//---------------------------------------------------------------------
// funzione che fa seguire la scia alle formiche
//---------------------------------------------------------------------

bool follow_trail(struct ant_t * ant)
{
int dx, dy, x, y;
double start_angle, curr_diff;

	x = ant->x / CELL_SIDE;
	y = ant->y / CELL_SIDE;

	start_angle = ant->angle;
	curr_diff = M_PI * 2;

	ant->following_trail = false;


	for (dx = x - 3; dx <= x + 3; dx++)
	{
		if(dx * CELL_SIDE > 0 && dx * CELL_SIDE < BACKGROUND_WIDTH)
		{
			for (dy = y - 3; dy <= y + 3; dy++)
			{
				if(dy * CELL_SIDE > 0 && dy * CELL_SIDE < BACKGROUND_HEIGHT)
				{
					if (dx == x && dy == y)
						continue;

					pthread_mutex_lock(&grid_mux);

					if (grid[dx][dy].odor_intensity > 0)
					{
						double angle = angle_towards(ant, grid[dx][dy].x, grid[dx][dy].y);

						double diff = fabs(atan2(sin(angle - start_angle), cos(angle - start_angle)));

						if (diff < curr_diff)
						{
							curr_diff = diff;
							ant->angle = angle;
							ant->following_trail = true;
						}
					}

					pthread_mutex_unlock(&grid_mux);

				}
		}	}
	}

	return ant->following_trail;
}

//---------------------------------------------------------------------
// bouncing on edges
//---------------------------------------------------------------------

void bounce(struct ant_t * ant)
{
	if (ant->x < ANT_RADIUS)
	{
		ant->angle +=deg_to_rad(90);
	}

	if (ant->x > (BACKGROUND_WIDTH - ANT_RADIUS))
	{
		ant->angle -= deg_to_rad(90);
	}

	if (ant->y < ANT_RADIUS)
	{
		ant->angle += deg_to_rad(90);
	}

	if (ant->y > (BACKGROUND_HEIGHT - ANT_RADIUS))
	{
		ant->angle -= deg_to_rad(90);
	}


}



double angle_towards(struct ant_t * ant, double x, double y)
{
double dx, dy;

	dx = (x - ant->x);
	dy = (y - ant->y);
	return atan2(dy,dx);
}

void head_towards(struct ant_t * ant, double x, double y)
{
	ant->angle = angle_towards(ant, x, y);
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

		draw_sprite(buffer, ground, 0, 0);

		draw_pheromone();

		draw_food();

		draw_sprite(buffer, nest_image, (nest.x - NEST_RADIUS) * SCALE, (nest.y - NEST_RADIUS) * SCALE);

		draw_ants();

		draw_scouts();

		draw_interface();

		blit(buffer, screen, 0, 0, 0, 0, buffer->w, buffer->h);

		unscare_mouse();

		if (deadline_miss(tp)) 
		{
			printf("deadline miss gfx\n");
			deadline_miss_num++;
		}
		wait_for_period(tp);
	}
}


//---------------------------------------------------------------------
// funzione per disegnare il cibo
//---------------------------------------------------------------------

void draw_food(void)
{
int i;
double radius;

	BITMAP * food;

	food = load_bitmap("sugar.bmp", NULL);
	if (food == NULL)
	{
		printf("errore food \n");
		exit(1);
	}

	pthread_mutex_lock(&food_mux);

	for (i = 0; i < MAX_FOOD_NUM; i++)
		if (food_list[i].quantity > 0)
		{
			radius = (food_list[i].quantity * FOOD_SCALE) / 2;

			stretch_sprite(buffer, food, 
				(food_list[i].x - radius) * SCALE, 
				(food_list[i].y - radius) * SCALE,
				radius * 2 * SCALE, radius * 2 * SCALE);	
		}

	pthread_mutex_unlock(&food_mux);
			
				
}

//---------------------------------------------------------------------
// funzione per disegnare le formiche
//---------------------------------------------------------------------

void draw_ants(void)
{
int i;
BITMAP * ant;
BITMAP * ant_food;
double angle;

	ant = load_bitmap("ant.bmp", NULL);

	if (ant == NULL)
	{
		printf("errore ant\n");
		exit(1);
	}

	ant_food = load_bitmap("ant_food.bmp", NULL);

	if (ant_food == NULL)
	{
		printf("errore ant_food \n");
		exit(1);
	}

	for (i = 0; i < MAX_NUM_ANTS; i++)
		{
			if (ant_list[i].state != ANT_IDLE && ant_list[i].state!= ANT_AWAKING)
			{
				//converting degrees in allegro-degrees
				angle = ((rad_to_deg(ant_list[i].angle + M_PI_2) * 256 / 360));

				if  (!ant_list[i].carrying_food)
					rotate_sprite(buffer, ant, (ant_list[i].x - ANT_RADIUS) * SCALE, 
					              (ant_list[i].y - ANT_RADIUS) * SCALE, ftofix(angle));
				else
					rotate_sprite(buffer, ant_food, (ant_list[i].x - ANT_RADIUS) * SCALE, 
					              (ant_list[i].y - ANT_RADIUS) * SCALE, ftofix(angle));
			}


		}
}



//---------------------------------------------------------------------
// funzione che disegna i feromoni
//---------------------------------------------------------------------

void draw_pheromone(void)
{
int i, j;
int col = makecol(246, 240, 127);

	pthread_mutex_lock(&grid_mux);

	for (i = 0; i < X_NUM_CELL; i++)
		for (j = 0; j < Y_NUM_CELL; j++)
			if (grid[i][j].odor_intensity > 0)
				circlefill(buffer, grid[i][j].x * SCALE, grid[i][j].y * SCALE, 
					       (grid[i][j].odor_intensity + 1) / 2, col);

	pthread_mutex_unlock(&grid_mux);
}


//---------------------------------------------------------------------
// funzione che disegna le scout
//---------------------------------------------------------------------

void draw_scouts(void)
{
int i;
BITMAP * scout;
BITMAP * scout_food;
double angle;

	scout = load_bitmap("scout.bmp", NULL);
	scout_food = load_bitmap("scout_food.bmp", NULL);

	if (scout == NULL)
	{
		printf("errore scout \n");
		exit(1);
	}

	if (scout_food == NULL)
	{
		printf("errore scout_food \n");
		exit(1);
	}

	for (i = 0; i < nScouts; i++)
	{
		if(scout_list[i].state != ANT_IDLE)
		{
			//converting degrees in allegro-degrees

			angle = ((rad_to_deg(scout_list[i].angle + M_PI_2) * 256 / 360));

			if(!scout_list[i].carrying_food)
			{
				rotate_sprite(buffer, scout, (scout_list[i].x - ANT_RADIUS)* SCALE, 
					        (scout_list[i].y - ANT_RADIUS) * SCALE, ftofix(angle));
			}
			else
			{
				rotate_sprite(buffer, scout_food, (scout_list[i].x - ANT_RADIUS)* SCALE, 
					        (scout_list[i].y - ANT_RADIUS) * SCALE, ftofix(angle));

			}
		}
	}
}



void draw_interface()
{
int i;
char buf[60];
int blue = makecol(217, 241, 247);
int red = makecol(247, 91, 137);
int green = makecol(102, 192, 146);
int orange = makecol(255, 102, 0);
int last_deadline_text = 330;


	textout_ex(buffer, font, "TO ADD FOOD CLICK ON THE ENVIRONMENT", 830, 25, blue, -1);
	textout_ex(buffer, font, "TO ADD A SCOUT ANT PRESS S", 830, 40, blue, -1);
	textout_ex(buffer, font, "TO ADD A WORKER ANT PRESS W", 830, 55, blue, -1);
	sprintf(buf, "scouts num: %d", nScouts);
	textout_ex(buffer, font, buf, 830, 70, blue, -1);
	sprintf(buf, "workers num: %d", nAnts);
	textout_ex(buffer, font, buf, 830, 85, blue, -1);
	textout_ex(buffer, font, "FOOD MAP", 950, 130, red, -1);

	rect(buffer, 900, 275, 1060, 145, blue); 

	pthread_mutex_lock(&food_mux);

	for (i = 0; i < MAX_FOOD_NUM; i++)
	{
		if (food_list[i].quantity != 0)
		{
			sprintf(buf, "%d", food_list[i].quantity);
			textout_ex(buffer, font, buf, (food_list[i].x / 5 * SCALE) + 900, (food_list[i].y / 5 * SCALE) + 130, blue, -1);
		}

	}

	pthread_mutex_unlock(&food_mux);

	textout_ex(buffer, font, "DEADLINE MISS:", 930, 300, red, -1);
	if (deadline_miss_num == 0)
	{
		textout_ex(buffer, font, "no deadline miss (yet)", 900, 330, green, -1);
	}
	else
	{
		for (i = 0; i < MAX_NUM_SCOUTS; i++)
		{
			if (scouts_tp[i].dmiss > 0)
			{
				sprintf(buf,"scout %d missed its deadline %d times", scouts_tp[i].arg, scouts_tp[i].dmiss);
				textout_ex(buffer, font, buf, 850, last_deadline_text, orange, -1);
				last_deadline_text+=10;
			}
		}

		for (i = 0; i < MAX_NUM_ANTS; i++)
		{
			if (tp[i].dmiss > 0)
			{
				sprintf(buf,"ant %d missed its deadline %d times", tp[i].arg, tp[i].dmiss);
				textout_ex(buffer, font, buf, 850, last_deadline_text, orange, -1);
				last_deadline_text+=10;
			}
		}

		if (gfx_tp.dmiss > 0)
		{
			sprintf(buf,"graphic thread missed its deadline %d times",gfx_tp.dmiss);
			textout_ex(buffer, font, buf, 850, last_deadline_text, orange, -1);
			last_deadline_text+=10;
		}

		if (ph_tp.dmiss > 0)
		{
			sprintf(buf,"graphic thread missed its deadline %d times",ph_tp.dmiss);
			textout_ex(buffer, font, buf, 850, last_deadline_text, orange, -1);
			last_deadline_text+=10;
		}

	}

}