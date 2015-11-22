#include "utils.h"
#include <math.h>
#include <stdlib.h>

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
// calcola la distanza fra due punti
//---------------------------------------------------------------------


float distance(float x_s,float y_s, float x_d, float y_d)
{
float distance_x, distance_y, distance;

	distance_x = ((x_s - x_d) * (x_s - x_d));
	distance_y = ((y_s - y_d) * (y_s - y_d));
	distance = sqrt(distance_y + distance_x);
	return distance;
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


