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

#define FOOD_DETECTION_RADIUS	(FOOD_BASE_RADIUS + 20)
#define FOOD_BASE_ODOR			(FOOD_DETECTION_RADIUS * FOOD_DETECTION_RADIUS)

#define MAX_FOOD_NUM			5	//numero massimo di pile di cibo
#define MAX_FOOD_QUANTITY		10	//numero massimo di cibo per pila
#define FOOD_SCALE              8   // radius = quantity * scale	

#define NUM_ANTS				10		//numero di formiche normali
#define DELTA_ANGLE				5		//max angle deviation
#define DELTA_SPEED				0.1		//max_speed deviation
#define ANT_PERIOD				0.02
#define ANT_SPEED				20 
#define ANT_RADIUS				8

#define NEST_RADIUS				40

#define MAX_PHEROMONE_INTENSITY 15
#define PHEROMONE_INTENSITY     3
#define PHEROMONE_TASK_PERIOD  	5000

#define CELL_SIDE				10
#define X_NUM_CELL				(int)BACKGROUND_WIDTH / CELL_SIDE
#define Y_NUM_CELL				(int)BACKGROUND_HEIGHT / CELL_SIDE

#define	NUM_SCOUTS				5

#define ANT_TYPE_SCOUT          0
#define ANT_TYPE_WORKER         1

#define MAX_NEST_SCENT			50



// formula: odor = (base_odor * odor_scale)/ant_distance_from_food^2


//---------------------------------------------------------------------------
//TYPE DEFINITIONS
//---------------------------------------------------------------------------
enum state_t
{
	ANT_IDLE, 
	ANT_TOWARDS_FOOD, 
	ANT_TOWARDS_HOME_NO_FOOD, 
	ANT_TOWARDS_HOME_WITH_FOOD,
	ANT_TOWARDS_UNKNOWN,
	ANT_RANDOM_MOVEMENT
};


struct food_t
{
	float 	x;						//center coord
	float 	y;						//center coord
	int 	quantity;

};

struct ant_t
{
	int     type;

	float 	x, y;
	float 	speed;
	float	angle;
	float   pheromone_intensity;

	enum state_t state;

	bool 	found_food, carrying_food, following_trail;
	float   food_x, food_y;
	bool 	inside_nest;

	struct timespec last_release;
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
	float odor_intensity;
	float nest_scent;
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

void * ant_task(void *);
void * gfx_task(void *);
void * pheromone_task(void *);
void * scout_task(void *);

float frand(float, float);
void put_nest(void);

void bounce(struct ant_t *);

float deg_to_rad(float);
float rad_to_deg(float);

bool look_for_food(struct ant_t *);
bool look_for_trail(struct ant_t *);
float distance(float, float, float, float);

void head_towards(struct ant_t *, float, float);
bool sense_nest(struct ant_t *);
bool check_nest(struct ant_t *);

void setup_grid(void);

void release_pheromone(struct ant_t *);

void draw_pheromone(void);

void follow_trail(struct ant_t *);

bool sense_food(struct ant_t *);


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

struct ant_t 		ant_list[NUM_ANTS] = {{0}};
int 				nAnts = 0;

struct nest_t		nest;

pthread_t 			tid[NUM_ANTS];
struct task_par		tp[NUM_ANTS];
pthread_attr_t		attr[NUM_ANTS];

pthread_t 			scouts_tid[NUM_SCOUTS];
struct task_par		scouts_tp[NUM_SCOUTS];
pthread_attr_t		scouts_attr[NUM_SCOUTS];
struct ant_t 		scout_list[NUM_SCOUTS] = {{0}};
int 				nScouts = 0;

bool				running = true;

pthread_t           gfx_tid;
struct task_par     gfx_tp;

pthread_t           ph_tid;
struct task_par     ph_tp;

struct cell_t 		grid[X_NUM_CELL][Y_NUM_CELL];	//griglia dello sfondo

pthread_mutex_t		scout_mutex;
pthread_cond_t		scout_condition;


int 				counter_food_found = 0;			//cibi trovati non ancora finiti


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
char	  scan;
const int FOOD_BASE_RADIUS = MAX_FOOD_QUANTITY * FOOD_SCALE;
int i;

	// mouse

	food_x = mouse_x;
	food_y = mouse_y;
	if (
		(food_x < (BACKGROUND_WIDTH - FOOD_BASE_RADIUS) && food_x > FOOD_BASE_RADIUS) && 
		(food_y < (BACKGROUND_HEIGHT - FOOD_BASE_RADIUS) && food_y > FOOD_BASE_RADIUS)
		)
	{
		should_put_food = (!(mouse_prev & 1) && (mouse_b & 1));
	}

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

			for(i = 0; i < NUM_SCOUTS; i++)
			{
				scouts_tp[nScouts].arg = nScouts;
				scouts_tp[nScouts].period = 20;
				scouts_tp[nScouts].deadline = 60;
				scouts_tp[nScouts].priority = 10;

				scouts_tid[nScouts] = task_create(scout_task, &scouts_tp[nScouts]);

				nScouts++;
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
int j;
	//allegro setup

	pthread_mutex_init(&scout_mutex, NULL);
	pthread_cond_init(&scout_condition, NULL);

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
	ph_tp.period = PHEROMONE_TASK_PERIOD;
	ph_tp.deadline = 800;
	ph_tp.priority = 10;

	ph_tid = task_create(pheromone_task, &ph_tp);

	put_nest();

	for(j = 0; j < NUM_ANTS; j++)
	{
		tp[nAnts].arg = nAnts;
		tp[nAnts].period = 20;
		tp[nAnts].deadline = 100;
		tp[nAnts].priority = 10;

		tid[nAnts] = task_create(ant_task, &tp[nAnts]);

		nAnts++;		
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
// UPDATE ANTS
//---------------------------------------------------------------------


void * ant_task(void * arg)
{
float	da, vx, vy, i;
struct task_par * tp = (struct task_par *) arg;
struct ant_t * ant = &ant_list[tp->arg];

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

	// caso 1: fine della scia mentre cerchi cibo

	while(1)
	{
		switch (ant->state)
		{
		case ANT_IDLE:
			// e ci sono feromoni accanto
			follow_trail(ant);

			if (ant->following_trail) 
				ant->state = ANT_TOWARDS_FOOD;

			break;

		case ANT_TOWARDS_FOOD:
			if (!sense_food(ant))
			{
				if (0 /* sono dove dovrebbe esserci dle cibo */)
				{
					/* fai qualcosa */
				}
				else
				{
					follow_trail(ant);
				}
			}
			else if (look_for_food(ant))
			{
				follow_trail(ant);

				if (ant->following_trail)
					ant->state = ANT_TOWARDS_HOME_WITH_FOOD;
				else
					ant->state = ANT_RANDOM_MOVEMENT;
			}

			break;

		case ANT_TOWARDS_HOME_NO_FOOD:

			break;

		case ANT_TOWARDS_HOME_WITH_FOOD:
			release_pheromone(ant);

			if (!sense_nest(ant))
			{
				follow_trail(ant);
			}
			else if (check_nest(ant))
			{
				ant->carrying_food = false;

				follow_trail(ant);

				if (ant->following_trail)
					ant->state = ANT_TOWARDS_FOOD;
				else
					ant->state = ANT_RANDOM_MOVEMENT;
			}

			break;

		case ANT_TOWARDS_UNKNOWN:
			if (sense_nest(ant) && check_nest(ant))
			{
				ant->carrying_food = false;
				ant->state = ANT_IDLE;
			}
			else if (sense_food(ant))
				ant->state = ANT_TOWARDS_FOOD;

			break;

		case ANT_RANDOM_MOVEMENT:
			ant->angle += deg_to_rad(frand(-DELTA_ANGLE, DELTA_ANGLE));

			follow_trail(ant);

			if (ant->following_trail)
				ant->state = ANT_TOWARDS_UNKNOWN;
			else if (sense_nest(ant) && check_nest(ant))
			{
				ant->carrying_food = false;
				ant->state = ANT_IDLE;
			}

			break;
		}

		if (ant->state != ANT_IDLE)
		{
			// aggiorna velocità e posizione
			vx = ant->speed * cos(ant->angle);
			vy = ant->speed * sin(ant->angle);

			ant->x += vx * ANT_PERIOD;
			ant->y += vy * ANT_PERIOD;

			bounce(ant);
		}

		if (deadline_miss(tp)) printf("deadline miss ant\n");
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

		draw_sprite(buffer, ground, 0, 0);

		draw_pheromone();

		draw_food();

		draw_sprite(buffer, nest_image, nest.x - NEST_RADIUS, nest.y - NEST_RADIUS);

		draw_ants();

		draw_scouts();
	
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
int i,j;
	nest.x = BACKGROUND_WIDTH / 2;
	nest.y = BACKGROUND_HEIGHT / 2;
	for (i = 0; i < X_NUM_CELL; i++)
		for(j = 0; j <  Y_NUM_CELL; j++)
			grid[i][j].nest_scent = MAX_NEST_SCENT - distance(nest.x, nest.y, grid[i][j].x, grid[i][j].y);
}

//---------------------------------------------------------------------
// controlla se la formica raggiunge la fine dello sfondo e la fa
// tornare indietro
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


bool look_for_food(struct ant_t * ant)
{
int i;

	for (i = 0; i < MAX_FOOD_NUM; i++)
	{
	float dist = distance(ant->x, ant->y, food_list[i].x, food_list[i].y);

		if (food_list[i].quantity > 0 && 
			dist < (food_list[i].quantity * FOOD_SCALE / 2)
		   )
		{
			ant->following_trail = true;
			ant->carrying_food = true;

			ant->food_x = food_list[i].x;
			ant->food_y = food_list[i].y;

			food_list[i].quantity--;

			return true;
		}
	}

	return false;
}



//---------------------------------------------------------------------
//
//---------------------------------------------------------------------


float distance(float x_s,float y_s, float x_d, float y_d)
{
int distance_x, distance_y, distance;

	distance_x = ((x_s - x_d) * (x_s - x_d));
	distance_y = ((y_s - y_d) * (y_s - y_d));
	distance = sqrt(distance_y + distance_x);
	return distance;
}

float angle_towards(struct ant_t * ant, float x, float y)
{
float dx, dy;

	dx = (x - ant->x);
	dy = (y - ant->y);
	return atan2(dy,dx);
}

void head_towards(struct ant_t * ant, float x, float y)
{
	ant->angle = angle_towards(ant, x, y);
}

bool sense_nest(struct ant_t * ant)
{
	if (distance(ant->x, ant->y, nest.x, nest.y) < 40)
	{
		head_towards(ant, nest.x, nest.y);
		return true;
	}

	return false;
}

bool check_nest(struct ant_t * ant)
{
	if (distance(ant->x, ant->y, nest.x, nest.y) < 5)
		return true;

	return false;
}

void draw_food(void)
{
int i;
int radius;

	BITMAP * food;

	food = load_bitmap("sugar.bmp", NULL);
	if (food == NULL)
	{
		printf("errore food \n");
		exit(1);
	}

	for (i = 0; i < MAX_FOOD_NUM; i++)
		if (food_list[i].quantity > 0)
		{
			radius = (food_list[i].quantity * FOOD_SCALE) / 2;

			stretch_sprite(buffer, food, 
				food_list[i].x - radius, food_list[i].y - radius,
				radius * 2, radius * 2);	
		}
			
				
}

void draw_ants(void)
{
int i;
BITMAP * ant;
BITMAP * ant_food;
float angle;

	ant = load_bitmap("ant.bmp", NULL);

	if (ant == NULL)
	{
		printf("errore ant \n");
		exit(1);
	}

	ant_food = load_bitmap("ant_food.bmp", NULL);

	if (ant_food == NULL)
	{
		printf("errore ant_food \n");
		exit(1);
	}

	for (i = 0; i < NUM_ANTS; i++)
		{
			//converting degrees in allegro-degrees
			angle = ((rad_to_deg(ant_list[i].angle + M_PI_2) * 256 / 360));

			if  (!ant_list[i].carrying_food)
				rotate_sprite(buffer, ant, ant_list[i].x - ANT_RADIUS, 
				              ant_list[i].y - ANT_RADIUS, ftofix(angle));
			else
				rotate_sprite(buffer, ant_food, ant_list[i].x - ANT_RADIUS, 
				              ant_list[i].y - ANT_RADIUS, ftofix(angle));


		}
}

void setup_grid()
{
int i, j;

	for (i = 0; i < X_NUM_CELL; i++)
		for (j = 0; j < Y_NUM_CELL; j++)
		{
			grid[i][j].x = i * CELL_SIDE + CELL_SIDE / 2;
			grid[i][j].y = j * CELL_SIDE + CELL_SIDE / 2;
			grid[i][j].odor_intensity = 0;
		}
}

void release_pheromone(struct ant_t * ant)
{
int x, y;
struct timespec t;

	x = ant->x / CELL_SIDE;
	y = ant->y / CELL_SIDE;

	clock_gettime(CLOCK_MONOTONIC, &t);

	if ((t.tv_sec - ant->last_release.tv_sec) > -1)
	{
		if (grid[x][y].odor_intensity < MAX_PHEROMONE_INTENSITY)
			grid[x][y].odor_intensity += ant->pheromone_intensity;

		ant->last_release = t;
	}
	
}

void draw_pheromone(void)
{
int i, j;
BITMAP * trail;
int col = makecol(246, 240, 127);

	trail = load_bitmap("scia.bmp", NULL);

	for (i = 0; i < X_NUM_CELL; i++)
		for (j = 0; j < Y_NUM_CELL; j++)
			if (grid[i][j].odor_intensity > 0)
				circlefill(buffer, grid[i][j].x, grid[i][j].y, 
					       (grid[i][j].odor_intensity + 1) / 2, col);
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
					grid[i][j].odor_intensity -= 0.2;

	if (deadline_miss(tp)) printf("deadline miss gfx\n");
		wait_for_period(tp);
	}
}

void follow_trail(struct ant_t * ant)
{
int cx, cy, dx, dy, x, y, i;
float start_angle, curr_diff;

	x = ant->x / CELL_SIDE;
	y = ant->y / CELL_SIDE;

	start_angle = ant->angle;
	curr_diff = M_PI * 2;

	ant->following_trail = false;

	if (ant->type == ANT_TYPE_WORKER)
	{

	printf("START\n");
	printf("start_angle: %f\n", start_angle);
	}

	for (dx = x - 3; dx <= x + 3; dx++)
		for (dy = y - 3; dy <= y + 3; dy++)
		{
			if (dx == x && dy == y)
				continue;

			if (grid[dx][dy].odor_intensity > 0)
			{
				float angle = angle_towards(ant, grid[dx][dy].x, grid[dx][dy].y);

				float diff = fabs(atan2(sin(angle - start_angle), cos(angle - start_angle)));

				if (ant->type == ANT_TYPE_WORKER)
				{
				printf("found: %f %f\n", grid[dx][dy].x, grid[dx][dy].y);
				printf("angle: %f diff: %f\n", angle, diff);

				}

				if (diff < curr_diff)
				{
					curr_diff = diff;
					ant->angle = angle;
					ant->following_trail = true;
				}

			}
		}

if (ant->type == ANT_TYPE_WORKER)
{
	printf("%d\n", ant->following_trail);
	printf("END\n");
}
}

void * scout_task(void * arg)
{
float vx, vy, da;
bool found_food = false;


struct task_par * tp = (struct task_par *) arg;
struct ant_t * scout = &scout_list[tp->arg];

	scout->type = ANT_TYPE_SCOUT;
	scout->x = nest.x; 
	scout->y = nest.y; 
	scout->speed = ANT_SPEED;
	scout->angle = deg_to_rad(frand(0,360));
	scout->pheromone_intensity = PHEROMONE_INTENSITY;

	scout->following_trail = false;
	scout->carrying_food = false;

	set_period(tp);

	while(1)
	{
		bool at_home = check_nest(scout);

        // Caso iniziale: sto girando liberamente senza cibo
		if(!scout->carrying_food && !scout->following_trail)	
		{
		    // L'obiettivo primario è trovare cibo e portarlo a casa
		    
		    // Se rilevo cibo vicino
		    if (sense_food(scout))
		    {
		        // Se ci sono sopra
    		    if (look_for_food(scout))
                {
                    // Dirigiti verso il nido rilasciando feromone
                    head_towards(scout, nest.x, nest.y);
                    found_food = true;
                }
		    }
            // Se non ho cibo attorno a me, per il momento cerco a caso
            else
            {
                da = deg_to_rad(frand(-DELTA_ANGLE, DELTA_ANGLE));
                scout->angle += da;
                
                // Poi dopo si vede se considerare una scia
            }
		}

		// Caso successivo: ho cibo in spalla, e sto tornando a casa
		if (scout->carrying_food)
		{
		    // Se sono arrivato, poso la roba e torno al cibo
		    if (check_nest(scout))
		    {
    			// deposita il cibo
    			scout->carrying_food = false;
    
    			// girati
    			scout->angle += M_PI;
    
    			// segui la scia verso il cibo
    			follow_trail(scout);
		    }
    		// Se non sono arrivato, seguo la scia fino a casa, rilasciando feromone
		    else
		    {
				head_towards(scout, nest.x, nest.y);
				release_pheromone(scout);
		    }
		}

        // Penultimo caso: sto tornando verso il cibo seguendo la scia
        if (!scout->carrying_food && scout->following_trail)
        {

        	sense_food(scout);
            // Controllo se sono arrivato al cibo
            if (look_for_food(scout))
            {
                // Se lo trovo, mi giro e torno a casa
    			scout->angle += M_PI;
            }

            // In ogni caso, continuo a seguire la scia
    		follow_trail(scout);
    		
    		// Se la scia si è finita, e non ho trovato niente, vado a caso
    		if (!scout->following_trail)
    		{
    		    // TODO: qui ci va la cosa per far capire alle altre che ho finito
    		}
        }
        
		// aggiorna velocità e posizione
		vx = scout->speed * cos(scout->angle);
		vy = scout->speed * sin(scout->angle);

		scout->x += vx * ANT_PERIOD;
		scout->y += vy * ANT_PERIOD;

		bounce(scout);

		if (deadline_miss(tp)) printf("deadline miss scout\n");
		wait_for_period(tp);
	}
}

void draw_scouts(void)
{
int i;
BITMAP * ant;
BITMAP * ant_food;
float angle;

	ant = load_bitmap("scout.bmp", NULL);
	ant_food = load_bitmap("scout.bmp", NULL);

	if (ant == NULL)
	{
		printf("errore ant \n");
		exit(1);
	}

	for (i = 0; i < nScouts; i++)
	{
		//converting degrees in allegro-degrees

		angle = ((rad_to_deg(scout_list[i].angle + M_PI_2) * 256 / 360));

		if(!scout_list[i].carrying_food)
		{
			rotate_sprite(buffer, ant, scout_list[i].x - ANT_RADIUS, 
				        scout_list[i].y - ANT_RADIUS, ftofix(angle));
		}
		else
		{
			rotate_sprite(buffer, ant_food, scout_list[i].x - ANT_RADIUS, 
				        scout_list[i].y - ANT_RADIUS, ftofix(angle));
		}
	
	}
}

bool sense_food(struct ant_t * ant)
{
	int i;

	for (i = 0; i < MAX_FOOD_NUM; i++)
	{
	float dist = distance(ant->x,ant->y, food_list[i].x, food_list[i].y);

		if (food_list[i].quantity > 0 && 
			dist < ((food_list[i].quantity * FOOD_SCALE / 2) + 20)
		   )
		{

			ant->food_x = food_list[i].x;
			ant->food_y = food_list[i].y;

			head_towards(ant, ant->food_x , ant->food_y);
			return true;
		}
	}

	return false;
}

void follow_scent(struct ant_t * ant)
{
int x, y, i, j;
int dx, dy;

	x = ant->x / CELL_SIDE;
	y = ant->y / CELL_SIDE;

	for (dx =  x - i; dx < x + i; i++)
		for(dy = y - i; dy < y + i; dy++)
			if(grid[dx][dy].nest_scent > grid[x][y].nest_scent)
				head_towards(ant, grid[x][y].x, grid[x][y].y);


}

	