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

/* Animates a curious eight-sided device of possibly Oriental origin.
 *
 * TODO:
 *  - Find a better function to simulate the "spring" of the ticking.
 *  - Maybe we can make some cool glowing effects when the stars come out.
 *  - The polygon count is not really accurate.
 *
 * Note to the poor reader: This file was made into Win32 directly from
 * my xscreensaver source. msvc sucks absolute ass at compiling C code so this
 * is actually a C++ source masquerading as a C file.
 * Thanks for bearing with me!
 */

#include <Windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include "../win32/glext.h"

#include <stdlib.h>
#include "eight.h"
#include "eight_tex.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define PRINTMAT(x) do { int i; for (i = 0; i < 16; i += 4) \
	fprintf(stderr, "%f %f %f %f\n", x[i], x[i + 1], x[i + 2], x[i + 3]); } while (0)


#undef countof
#define countof(x) (sizeof((x)) / sizeof((*x)))

#define OCT_OFFSET ((float) (0.58578643762690495119831F / 2.0F)) /* 1 - sqrt(2)/2 */
#define OCT_SIDE ((float) 0.414213562373095048801F) /* sqrt2 - 1 */

static const float SIZE_YY = 1.0F / 3.0F;
static const float H_TRI = 0.47F;
static const float H_TOP = 0.45F;
static const float H_MID = 0.35F;
static const float H_BOT = 0.0F;
static const float A_TRI_SPAN = 0.35F;
static const float W_TRI_SPLIT = 0.04F;
static const float R_TRI_INNER = 0.5F;
static const float R_TRI_WIDTH = 0.10F;
static const float R_TRI_SPACE = 0.15F;

static const int TRI_CONFIG[] =
{
	3, 7, 6, 2, 4, 0, 1, 5
};

static const float OCT_POINTS[] = 
{
	1, 1 - OCT_OFFSET,
	1 - OCT_OFFSET, 1,
	OCT_OFFSET, 1,
	0, 1 - OCT_OFFSET,
	0, OCT_OFFSET,
	OCT_OFFSET, 0,
	1 - OCT_OFFSET, 0,
	1, OCT_OFFSET
};

static const GLfloat star_mat[] =
{
	0.4F, 0.4F, 0.4F, 1.0F,      /* ambient */
	0.0F, 0.0F, 0.0F, 1.0F,      /* specular */
	0.8F, 0.8F, 0.8F, 1.0F,      /* diffuse */
	0                        /* shininess */
};
static const GLfloat tri_mat[] =
{
	0.1F, 0.1F, 0.1F, 1.0F,      /* ambient */
	0.50F, 0.50F, 0.50F, 1.00F,  /* specular */
	0.20F, 0.20F, 0.20F, 1.00F,  /* diffuse */
	100                      /* shininess */
};
static const GLfloat base_mat[] =
{
	0.80F, 0.82F, 0.83F, 1.00F, /* ambient */
	0.60F, 0.67F, 0.61F, 1.00F, /* specular */
	0.99F, 0.91F, 0.81F, 1.00F, /* diffuse */
	80.80F                   /* shininess */
};
static const GLfloat top_mat[] =
{
	0.50F, 0.50F, 0.50F, 1.00F, /* ambient */
	0.85F, 0.85F, 0.95F, 1.00F, /* specular */
	0.99F, 0.99F, 0.99F, 1.00F, /* diffuse */
	80.80F                   /* shininess */
};

static EIGHT_State *sts = NULL;

/* shamelessly stolen from Lament */
static void ScaleForWindow(int width, int height)
{
	GLfloat target_size = 1.4F * 512;
	GLfloat size = width < height ? width : height;
	GLfloat scale;

	/* Make it take up roughly the full width of the window. */
	scale = 25;

	/* But if the window is wider than tall, make it only take up the
		 height of the window instead.
	 */
	if (width > height)
	{
		scale /= width / (GLfloat) height;
	}

	{
		GLfloat max = 500;	/* 3" on my screen... */
		if (target_size > max) target_size = max;
	}

	if (size > target_size) scale *= target_size / size;

	glScalef(scale, scale, scale);
}

static void ClearRotation(void)
{
	float mat[16], scale[3];
	int i, j;
	glGetFloatv(GL_MODELVIEW_MATRIX, mat);

	for (i = 0; i < 3; i++)
	{
		scale[i] = 0;
		for (j = 0; j < 3; j++)
		{
			scale[i] += mat[j] * mat[j];
		}
		scale[i] = sqrt(scale[i]);
	}

	glLoadIdentity();
	glTranslatef(mat[12], mat[13], mat[14]);
	glScalef(scale[0], scale[1], scale[2]);
}

static void ApplyMat(const GLfloat *specs)
{
	glMaterialfv(GL_FRONT, GL_AMBIENT, specs);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, specs + 4);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specs + 8);
	glMaterialfv(GL_FRONT, GL_SHININESS, specs + 12);
}

static int DrawStar(EIGHT_State *st, EIGHT_StarInfo *inf)
{
	if (inf->lifetime == 0) return 0;

	glPushMatrix();

	glTranslatef(inf->x, inf->y, inf->z);
	glColor4f(inf->r, inf->g, inf->b, 0.2F);
	ClearRotation();
	glRotatef(inf->rot / M_PI * 180, 0, 0, 1);

	glCallList(st->dls[DL_STAR]);

	glPopMatrix();
	return st->pcounts[DL_STAR];
}

static int DrawStars(EIGHT_State *st)
{
	int i, j, polys = 0;

	if (rand() % 16 == 0)
	{
		for (j = 0; j < countof(st->stars); j++)
		{
			EIGHT_StarInfo *inf = &st->stars[j];
			if (!inf->lifetime)
			{
				float theta, r;
				inf->lifetime = 128;
				inf->x = 0;
				inf->y = 0;
				inf->z = 0;
				inf->r = rand() / (float) RAND_MAX;
				inf->g = rand() / (float) RAND_MAX;
				inf->b = rand() / (float) RAND_MAX;

				inf->dy = 0.1F + rand() / (float) RAND_MAX * 0.1F;

				theta = rand() / (float) RAND_MAX * M_PI * 2;
				r = 0.08F + rand() / (float) RAND_MAX * 0.03F;
				inf->dx = r * cos(theta);
				inf->dz = r * sin(theta);

				inf->rot = (rand() / (float) RAND_MAX) * M_PI * 2;
				break;
			}
		}
	}

	glPushMatrix();
	ApplyMat(star_mat);

	glTranslatef(0, H_TOP + 0.15F, 0);
	glScalef(0.2F, 0.2F, 0.2F);

	glDisable(GL_TEXTURE_2D);
	for (i = 0; i < countof(st->stars); i++)
	{
		EIGHT_StarInfo *inf = &st->stars[i];

		polys += DrawStar(st, inf);

		if (inf->lifetime)
		{
			inf->x += inf->dx;
			inf->y += inf->dy;
			inf->z += inf->dz;
			inf->dy -= 0.005F;
			inf->lifetime--;
		}
	}
	glEnable(GL_TEXTURE_2D);

	glPopMatrix();
	return polys;
}

static void AnimateTick(EIGHT_State *st)
{
	if (st->time++ > 80)
	{
		st->time = 0;

		st->outer_target_theta -= M_PI_4;
	}

	st->outer_omega += (st->outer_target_theta - st->outer_theta) * 0.06F; /* spring constant */
	st->outer_omega *= 0.90F; /* damping */
	/* calculating the frequency of oscillation is left as an exercise to the reader */

	st->inner_omega = 0.02F;

	st->outer_theta += st->outer_omega;
	st->inner_theta += st->inner_omega;
	st->base_theta -= 0.005F;

	if (st->outer_theta < -M_PI * 2 && st->outer_target_theta < -M_PI * 2)
	{
		st->outer_theta += M_PI * 2;
		st->outer_target_theta += M_PI * 2;
	}

	if (st->inner_theta > M_PI * 2)
	{
		st->inner_theta -= M_PI * 2;
	}
}

static void Animate(EIGHT_State *st)
{
	float rx, ry, rz;
	float tx, ty, tz;

	glPushMatrix();

	tx = ty = tz = 0;
	rx = M_PI_2 / 5;
	ry = 0;
	rz = 0;

#define SMOOTH(x, xmax) ((sin((x) / (xmax) * 2 * M_PI_2 - M_PI_2) + 1) / 2.0F)

	switch (st->cam_state)
	{
		case CAM_NONE:
			if (st->cam_ticks > -100) break; /* cool down */
			if (rand() % 1500 == 0)
			{
				st->cam_ticks = 250;
				st->cam_state = CAM_SHAKE;
			}
			else if (rand() % 1500 == 0)
			{
				st->cam_ticks = 400;
				st->cam_state = CAM_SWEEP;
			}
			else if (rand() % 1500 == 0)
			{
				st->cam_ticks = 600;
				st->cam_state = CAM_COOL;
			}
			else if (rand() % 1500 == 0)
			{
				st->cam_ticks = 800;
				st->cam_state = CAM_NORMAL;
			}
			break;
		case CAM_SHAKE:
			rz = sin(st->cam_ticks / 5.0F) * sin(st->cam_ticks / 250.0F * M_PI) * 0.1F;
			break;
		case CAM_NORMAL:
			{
				float stage;
				if (st->cam_ticks > 750)
				{
					stage = SMOOTH(800 - st->cam_ticks, 50.0F);
				}
				else if (st->cam_ticks < 50)
				{
					stage = 1 - SMOOTH(50 - st->cam_ticks, 50.0F);
				}
				else stage = 1;

				ty = 7 * stage;

				rx = M_PI_2 / 5 + stage * (M_PI_2 - M_PI_2 / 5);
			}
			break;
		case CAM_COOL:
			{
				float stage;
				if (st->cam_ticks > 550)
				{
					stage = SMOOTH(600 - st->cam_ticks, 50.0F);
				}
				else if (st->cam_ticks < 50)
				{
					stage = 1 - SMOOTH(50 - st->cam_ticks, 50.0F);
				}
				else stage = 1;

				tx = stage * 4;
				ty = stage * 0.2F;

				rx = M_PI_2 / 5 + stage * (M_PI_2 / 3 - M_PI_2 / 5);
			}
			break;
		case CAM_SWEEP:
			ry = -SMOOTH(400 - st->cam_ticks, 400.0F) * M_PI * 4;
			break;
	}

#undef SMOOTH

	if (st->cam_ticks-- < 0) st->cam_state = CAM_NONE;

	glTranslatef(tx, ty, tz);
	glRotatef(rx / M_PI * 180, 1, 0, 0);
	glRotatef(ry / M_PI * 180, 0, 1, 0);
	glRotatef(rz / M_PI * 180, 0, 0, 1);

	ScaleForWindow(st->width, st->height);
	AnimateTick(st);

	glEnable(GL_LIGHTING);
	glPushMatrix();

	glRotatef(st->base_theta / M_PI * 180, 0, 1, 0);
	glCallList(st->dls[DL_BASE]);

#if 0
	//coord axes
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINES);
	glColor4f(1, 0, 0, 1);
	glVertex3f(0, 0, 0);
	glVertex3f(2, 0, 0);
	glColor4f(0, 1, 0, 1);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 2, 0);
	glColor4f(0, 0, 1, 1);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 0, 2);
	glEnd();
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
#endif
	
	glPushMatrix();
	glRotatef(st->outer_theta / M_PI * 180, 0, 1, 0);
	glCallList(st->dls[DL_SPINNER_OUTER]);
	glPopMatrix();

	glPushMatrix();
	glRotatef(st->inner_theta / M_PI * 180, 0, 1, 0);
	glCallList(st->dls[DL_SPINNER_INNER]);
	glPopMatrix();

	glPopMatrix();

	glDisable(GL_LIGHTING);
	DrawStars(st);

	glPopMatrix();
}

#undef GEN_INNER_CIRCLE
#define GEN_INNER_CIRCLE(H) \
	do \
	{ \
		glNormal3f(0, 1, 0); \
		glTexCoord2f(cth * SIZE_YY * 0.5F + 0.5F, sth * SIZE_YY * 0.5F + 0.5F); \
		glVertex3f(cth * SIZE_YY / 2, H, sth * SIZE_YY / 2); \
	} while (0)

#define RECT(NX, NY, X1, X2, Y1, Y2, Z1, Z2) \
	do \
	{ \
		glTexCoord2f(0, 0); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X1, Y1, Z1); \
\
		glTexCoord2f(0.25F, 0); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X2, Y1, Z2); \
\
		glTexCoord2f(0.25F, 0.1F); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X2, Y2, Z2); \
\
		glTexCoord2f(0, 0.1F); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X1, Y2, Z1); \
	} while (0)


static int GenOctPrism(EIGHT_State *st, int texture, int rotate_texture, float ymin, float ymax)
{
	int i;
	float tx = OCT_SIDE;
	float ty = ymax - ymin;
	float uv[8];

	if (rotate_texture)
	{
		uv[0] = 0;  uv[1] = tx;
		uv[2] = ty; uv[3] = tx;
		uv[4] = ty; uv[5] = 0;
		uv[6] = 0;  uv[7] = 0;
	}
	else
	{
		uv[0] = tx; uv[1] = 0;
		uv[2] = tx; uv[3] = ty;
		uv[4] = 0;  uv[5] = ty;
		uv[6] = 0;  uv[7] = 0;
	}

	glBegin(texture ? GL_QUADS : GL_LINE_LOOP);
	for (i = 0; i < 8; i++)
	{
		float theta = i * M_PI_4;
		float px, py;
		float cth = cos(theta), sth = sin(theta);

		px = OCT_POINTS[(2 * i + 2) % 16];
		py = OCT_POINTS[(2 * i + 3) % 16];

		glTexCoord2f(uv[0], uv[1]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5F, ymin, py - 0.5F);

		glTexCoord2f(uv[2], uv[3]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5F, ymax, py - 0.5F);

		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glTexCoord2f(uv[4], uv[5]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5F, ymax, py - 0.5F);

		glTexCoord2f(uv[6], uv[7]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5F, ymin, py - 0.5F);
	}
	glEnd();
	return 16;
}

static int GenSpinnerInner(EIGHT_State *st, int texture)
{
	const int resolution = 64;
	int i;
	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_TOP]);
	ApplyMat(top_mat);

	glBegin(texture ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
	/* just the circle */
	for (i = 0; i < resolution; i++)
	{
		float theta = i * M_PI * 2 / resolution;
		float cth = cos(theta);
		float sth = sin(theta);

		GEN_INNER_CIRCLE(H_TOP);
	}
	glEnd();

	return resolution - 2;
}

static int GenTrigrams(EIGHT_State *st, int texture)
{
	int polys = 0;
	int i;
	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_WOOD]);
	glBegin(texture ? GL_QUADS : GL_LINE_LOOP);
	ApplyMat(tri_mat);

	for (i = 0; i < 8; i++)
	{
		int j;
		float theta = i * M_PI_4;
		float cth = cos(theta) / 2, sth = sin(theta) / 2;
		float cup = cos(theta + A_TRI_SPAN) / 2, sup = sin(theta + A_TRI_SPAN) / 2;
		float cdn = cos(theta - A_TRI_SPAN) / 2, sdn = sin(theta - A_TRI_SPAN) / 2;

		/* perpendicular */
		float xdf = -sth * W_TRI_SPLIT, ydf = cth * W_TRI_SPLIT;

		float nx = cth * 2, ny = sth * 2;

		/* the textures get skewed horizontally because of their varying sizes
		 * but this isn't a problem if the the trigrams go along the grain of the fiber
		 */

		for (j = 0; j < 3; j++)
		{
			float in = (R_TRI_INNER + j * R_TRI_SPACE);
			float out = (in + R_TRI_WIDTH);

			glTexCoord2f(0, 0);
			glNormal3f(0, 1, 0);
			glVertex3f(cdn * in, H_TRI, sdn * in);

			glTexCoord2f(0.1F, 0);
			glNormal3f(0, 1, 0);
			glVertex3f(cdn * out, H_TRI, sdn * out);

			/* split down the middle */
			if ((1 << j) & TRI_CONFIG[i])
			{
				/* points in middle of trigram */
				float xai = in * (cup + cdn) / 2;
				float yai = in * (sup + sdn) / 2;
				float xao = out * (cup + cdn) / 2;
				float yao = out * (sup + sdn) / 2;

				glTexCoord2f(0.1F, 0.25F);
				glNormal3f(0, 1, 0);
				glVertex3f(xao - xdf, H_TRI, yao - ydf);

				glTexCoord2f(0, 0.25F);
				glNormal3f(0, 1, 0);
				glVertex3f(xai - xdf, H_TRI, yai - ydf);

				/* insert sides */

				/* sides perpendicular to centre */
				RECT(-nx, -ny, xai - xdf, cdn * in, H_TOP, H_TRI, yai - ydf, sdn * in);
				RECT(nx, ny, cdn * out, xao - xdf, H_TOP, H_TRI, sdn * out, yao - ydf);
				RECT(-nx, -ny, cup * in, xai + xdf, H_TOP, H_TRI, sup * in, yai + ydf);
				RECT(nx, ny, xao + xdf, cup * out, H_TOP, H_TRI, yao + ydf, sup * out);
				/* sides facing inwards */
				RECT(-ny, nx, xao - xdf, xai - xdf, H_TOP, H_TRI, yao - ydf, yai - ydf);
				RECT(ny, -nx, xai + xdf, xao + xdf, H_TOP, H_TRI, yai + ydf, yao + ydf);

				glTexCoord2f(0, 0.25F);
				glNormal3f(0, 1, 0);
				glVertex3f(xai + xdf, H_TRI, yai + ydf);

				glTexCoord2f(0.1F, 0.25F);
				glNormal3f(0, 1, 0);
				glVertex3f(xao + xdf, H_TRI, yao + ydf);
			}

			glTexCoord2f(0.1F, 0.5F);
			glNormal3f(0, 1, 0);
			glVertex3f(cup * out, H_TRI, sup * out);

			glTexCoord2f(0, 0.5F);
			glNormal3f(0, 1, 0);
			glVertex3f(cup * in, H_TRI, sup * in);

			if (!((1 << j) & TRI_CONFIG[i]))
			{
				/* large faces that aren't split */
				RECT(-nx, -ny, cup * in, cdn * in, H_TOP, H_TRI, sup * in, sdn * in);
				RECT(nx, ny, cdn * out, cup * out, H_TOP, H_TRI, sdn * out, sup * out);
			}

			/* the normals are slightly off but i can't be bothered to fix them */
			RECT(-ny, nx, cup * out, cup * in, H_TOP, H_TRI, sup * out, sup * in);
			RECT(ny, -nx, cdn * in, cdn * out, H_TOP, H_TRI, sdn * in, sdn * out);
		}
	}
	glEnd();
	polys += 12 * 2 * 3 * 8; /* TODO */

	return polys;
}

static int GenStar(EIGHT_State *st, int texture)
{
	const float ANGLE = M_PI * 2 / 5;
	const float R = 0.2F;
	int i;
	glBegin(texture ? GL_TRIANGLES : GL_LINE_LOOP);

	for (i = 0; i < 5; i++)
	{
		/* we could probably cache the trig results */
		float theta = i * ANGLE;
		float sth = sin(theta) * R * 2, cth = cos(theta) * R * 2;
		float sup = sin(theta + ANGLE) * R, sdn = sin(theta - ANGLE) * R;
		float cup = cos(theta + ANGLE) * R, cdn = cos(theta - ANGLE) * R;

		glNormal3f(0, 0, 1);
		glVertex3f(0, 0, 0);

		glNormal3f(0, 0, 1);
		glVertex3f(cup, sup, 0);

		glNormal3f(0, 0, 1);
		glVertex3f(cdn, sdn, 0);

		glNormal3f(0, 0, 1);
		glVertex3f(cdn, sdn, 0);

		glNormal3f(0, 0, 1);
		glVertex3f(cup, sup, 0);

		glNormal3f(0, 0, 1);
		glVertex3f(cth, sth, 0);
	}

	glEnd();
	return 10;
}

static int GenSpinnerOuter(EIGHT_State *st, int texture)
{
	const int resolution = 16;
	int polys = 0;
	int i;

	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_TOP]);
	ApplyMat(top_mat);

	/* solid octagonal spokes */

	glBegin(texture ? GL_TRIANGLES : GL_LINE_LOOP);
	for (i = 0; i < 8; i++)
	{
		float theta = (i + 1) * M_PI_4;
		float cth = cos(theta);
		float sth = sin(theta);
		float px, py;

		GEN_INNER_CIRCLE(H_TOP);

		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glTexCoord2f(px, py);
		glNormal3f(0, 1, 0);
		glVertex3f(px - 0.5F, H_TOP, py - 0.5F);

		px = OCT_POINTS[(2 * i + 2) % 16];
		py = OCT_POINTS[(2 * i + 3) % 16];

		glTexCoord2f(px, py);
		glNormal3f(0, 1, 0);
		glVertex3f(px - 0.5F, H_TOP, py - 0.5F);

		if (!texture) GEN_INNER_CIRCLE(H_TOP);
	}
	glEnd();
	polys += 8;

	/* discrete circle-approximating spokes */
	for (i = 0; i < 8; i++)
	{
		int j;
		float px, py;
		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glBegin(texture ? GL_TRIANGLE_FAN : GL_LINE_LOOP);

		glTexCoord2f(px, py);
		glNormal3f(0, 1, 0);
		glVertex3f(px - 0.5F, H_TOP, py - 0.5F);

		for (j = resolution; j >= 0; j--)
		{
			float theta = i * M_PI_4 + j * M_PI_4 / resolution;
			float cth = cos(theta);
			float sth = sin(theta);
			GEN_INNER_CIRCLE(H_TOP);
		}

		if (!texture)
		{
			glNormal3f(0, 1, 0);
			glVertex3f(px - 0.5F, H_TOP, py - 0.5F);
		}

		glEnd();
	}
	polys += 8 * resolution;

	polys += GenOctPrism(st, texture, 1, H_MID, H_TOP);

	/* bottom layer */
	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_WOOD]);
	glBegin(texture ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
	for (i = 7; i >= 0; i--)
	{
		float px, py;
		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glTexCoord2f(px, py);
		glNormal3f(0, -1, 0);
		glVertex3f(px - 0.5F, H_MID, py - 0.5F);
	}
	glEnd();
	polys += 6;

	polys += GenTrigrams(st, texture);

	return polys;
}

static int GenBase(EIGHT_State *st, int texture)
{
	int i;
	int polys = 0;
	ApplyMat(base_mat);

	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_STAR]);
	polys += GenOctPrism(st, texture, 0, H_BOT, H_MID);

	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_WOOD]);

	/* base, wind this backwards */
	glBegin(texture ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
	for (i = 7; i >= 0; i--)
	{
		float px, py;
		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glTexCoord2f(px, py);
		glNormal3f(0, -1, 0);
		glVertex3f(px - 0.5F, H_BOT, py - 0.5F);
	}
	glEnd();

	glBegin(texture ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
	/* cap, wind this forwards */
	for (i = 0; i < 8; i++)
	{
		float px, py;
		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glTexCoord2f(px, py);
		glNormal3f(0, 1, 0);
		glVertex3f(px - 0.5F, H_MID, py - 0.5F);
	}
	glEnd();

	polys += 12;

	return polys;
}

static void InitGL(EIGHT_State *st)
{
	int i;

	static const GLfloat pos0[]	= { 0, 20, 20, 1.0F };
	static const GLfloat amb0[]	= { 0.8F, 0.8F, 0.8F, 1.0F };
	static const GLfloat dif0[]	= { 1, 1, 1, 1.0F };

	glLightfv(GL_LIGHT0, GL_POSITION, pos0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, amb0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dif0);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glCullFace(GL_BACK);
	glFrontFace(GL_CW); /* wtf, did i seriously make all of my faces wind backwards?! */

	GLuint *eight_data = (GLuint*) calloc(512 * 1536, sizeof(GLuint));
	if (!eight_data) return;

	int dest = 0;
	for (i = 3 * 512; i >= 0; i--)
	{
		int o;
		for (o = 0; o < 512; o++)
		{
			int ti = i;

			/*
			 * for some reason the texture has a greyscale band at the bottom...
			 * so we'll just copy the second-from-last line to the first line instead :D
			 */
			if (i == 3 * 512) ti = 1;

			eight_data[dest] += header_data_cmap[header_data[ti*512+o]][0] << 24;
			eight_data[dest] += header_data_cmap[header_data[ti*512+o]][1] << 16;
			eight_data[dest] += header_data_cmap[header_data[ti*512+o]][2] << 8;
			eight_data[dest] += 255;
			dest++;
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glGenTextures(TEX_END, st->textures);
	for (i = 0; i < TEX_END; i++)
	{
		glBindTexture(GL_TEXTURE_2D, st->textures[i]);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			512,
			512,
			0,
			GL_RGBA,
			GL_UNSIGNED_INT_8_8_8_8,
			eight_data + (i * 512 * 512)
			);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#if !defined(GL_TEXTURE_LOD_BIAS) && defined(GL_TEXTURE_LOD_BIAS_EXT)
#define GL_TEXTURE_LOD_BIAS GL_TEXTURE_LOD_BIAS_EXT
#endif
#ifdef GL_TEXTURE_LOD_BIAS
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.25F);
#endif
	}

	for (i = 0; i < countof(st->dls); i++)
	{
		st->dls[i] = glGenLists(1);
		st->pcounts[i] = 0;

		glNewList(st->dls[i], GL_COMPILE);

		switch (i)
		{
			case DL_SPINNER_INNER:
				st->pcounts[i] += GenSpinnerInner(st, 1);
				break;
			case DL_SPINNER_OUTER:
				st->pcounts[i] += GenSpinnerOuter(st, 1);
				break;
			case DL_BASE:
				st->pcounts[i] += GenBase(st, 1);
				break;
			case DL_STAR:
				st->pcounts[i] += GenStar(st, 1);
				break;
		}

		glEndList();
	}

}

void EIGHT_Reshape(EIGHT_State *st)
{
	GLfloat h = (GLfloat) st->height / (GLfloat) st->width;
	glViewport(0, 0, (GLint) st->width, (GLint) st->height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0F, 1.0F, -h, h, 2.0F, 70.0F);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);
	glTranslatef(0, -7, -40);
}

void EIGHT_Init(EIGHT_State *st)
{
	st->inner_omega = 0;
	st->inner_theta = 0;
	st->outer_omega = 0;
	st->outer_theta = 0;

	EIGHT_Reshape(st);
	InitGL(st);
}

void EIGHT_Draw(EIGHT_State *st)
{
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	Animate(st);
	glFlush();
	glFinish();
}

void EIGHT_Release(EIGHT_State *st)
{
	if (sts)
	{
		free(sts);
		sts = NULL;
	}
}
