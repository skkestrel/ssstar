/* Copyright (c) 2015 Kevin Zhang <sudo@pt-get.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <Windows.h>
#include <GL/gl.h>

enum EIGHT_DLType
{
	DL_SPINNER_OUTER = 0,
	DL_SPINNER_INNER,
	DL_BASE,
	DL_STAR,
	DL_END
};

enum EIGHT_CamState
{
	CAM_NONE = 0,
	CAM_SHAKE,
	CAM_SWEEP,
	CAM_NORMAL,
	CAM_COOL
};

enum EIGHT_TexType
{
	TEX_WOOD = 0,
	TEX_TOP,
	TEX_STAR,
	TEX_END
};

struct EIGHT_StarInfo
{
	float x, y, z;
	float dx, dy, dz;
	float r, g, b;
	float rot;
	int lifetime;
};

struct EIGHT_State
{
	GLuint dls[DL_END];
	GLuint pcounts[DL_END];
	GLuint textures[TEX_END];

	EIGHT_StarInfo stars[100];

	double inner_theta, inner_omega;
	double outer_theta, outer_omega;
	double outer_target_theta;
	double base_theta;
	HDC hdc;

	int width, height;

	int time;

	EIGHT_CamState cam_state;
	int cam_ticks;
};

extern void EIGHT_Reshape(EIGHT_State *st);

extern void EIGHT_Init(EIGHT_State *st);

extern void EIGHT_Draw(EIGHT_State *st);

extern void EIGHT_Release(EIGHT_State *st);
