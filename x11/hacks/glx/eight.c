/* xscreensaver, Copyright (c) 1998-2015 Kevin Zhang <sudo@pt-get.com>
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
 * Based off lament.c by jwz, 20-Mar-15.
 *
 * TODO:
 *  - Find a better function to simulate the "spring" of the ticking.
 *  - Maybe we can make some cool glowing effects when the stars come out.
 *  - The polygon count is not really accurate.
 */

#ifdef USE_GL

#define DEFAULTS          \
	"*delay: 20000\n"     \
	"*showFPS: False\n"   \
	"*wireframe: False\n"

#define PRINTMAT(x) do { int i; for (i = 0; i < 16; i += 4) \
	fprintf(stderr, "%f %f %f %f\n", x[i], x[i + 1], x[i + 2], x[i + 3]); } while (0)

#include "xlockmore.h"
#include "xpm-ximage.h"
#include "gltrackball.h"
#include "normals.h"

#ifdef __GNUC__
	__extension__
/**
 * don't warn about "string length is greater than the length
 * ISO C89 compilers are required to support" when including
 * the following XPM file...
 */
#endif
#include "../images/eight_tex.xpm"

#undef countof
#define countof(x) (sizeof((x)) / sizeof((*x)))

#define OCT_OFFSET ((float) (0.58578643762690495119831 / 2.0)) /* 1 - sqrt(2)/2 */
#define OCT_SIDE ((float) 0.414213562373095048801) /* sqrt2 - 1 */

static const float SIZE_YY = 1.0 / 3.0;
static const float H_TRI = 0.47;
static const float H_TOP = 0.45;
static const float H_MID = 0.35;
static const float H_BOT = 0.0;
static const float A_TRI_SPAN = 0.35;
static const float W_TRI_SPLIT = 0.04;
static const float R_TRI_INNER = 0.5;
static const float R_TRI_WIDTH = 0.10;
static const float R_TRI_SPACE = 0.15;

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
	0.2, 0.2, 0.2, 1.0,      /* ambient */
	0.0, 0.0, 0.0, 1.0,      /* specular */
	0.8, 0.8, 0.8, 1.0,      /* diffuse */
	0                        /* shininess */
};
static const GLfloat tri_mat[] =
{
	0.1, 0.1, 0.1, 1.0,      /* ambient */
	0.50, 0.50, 0.50, 1.00,  /* specular */
	0.20, 0.20, 0.20, 1.00,  /* diffuse */
	100                      /* shininess */
};
static const GLfloat base_mat[] =
{
	0.40, 0.42, 0.43, 1.00, /* ambient */
	0.60, 0.67, 0.61, 1.00, /* specular */
	0.99, 0.91, 0.81, 1.00, /* diffuse */
	80.80                   /* shininess */
};
static const GLfloat top_mat[] =
{
	0.50, 0.50, 0.50, 1.00, /* ambient */
	0.85, 0.85, 0.95, 1.00, /* specular */
	0.99, 0.99, 0.99, 1.00, /* diffuse */
	80.80                   /* shininess */
};

typedef enum
{
	DL_SPINNER_OUTER = 0,
	DL_SPINNER_INNER,
	DL_BASE,
	DL_STAR,
	DL_END
} DLType;

typedef enum
{
	CAM_NONE = 0,
	CAM_SHAKE,
	CAM_SWEEP,
	CAM_NORMAL,
	CAM_COOL
} CamState;

typedef enum
{
	TEX_WOOD = 0,
	TEX_TOP,
	TEX_STAR,
	TEX_END
} TexType;

typedef struct
{
	float x, y, z;
	float dx, dy, dz;
	float r, g, b;
	float rot;
	int lifetime;
} StarInfo;

typedef struct
{
	GLXContext *glx_context;
	trackball_state *trackball;

	GLuint dls[DL_END];
	GLuint pcounts[DL_END];
	XImage *texture;
	GLuint textures[TEX_END];

	StarInfo stars[100];

	double inner_theta, inner_omega;
	double outer_theta, outer_omega;
	double outer_target_theta;
	double base_theta;

	int time;

	CamState cam_state;
	int cam_ticks;
	Bool button_down_p;
} AniState;

static AniState *sts = NULL;

ENTRYPOINT ModeSpecOpt eight_opts = { 0, NULL, 0, NULL, NULL };

/* shamelessly stolen from Lament... thanks jwz */
static void scale_for_window(ModeInfo *mi)
{
	AniState *st = &sts[MI_SCREEN(mi)];

	GLfloat target_size = 1.4 * (st->texture ? st->texture->width : 512);
	GLfloat size = MI_WIDTH(mi) < MI_HEIGHT(mi) ? MI_WIDTH(mi) : MI_HEIGHT(mi);
	GLfloat scale;

	/* Make it take up roughly the full width of the window. */
	scale = 25;

	/* But if the window is wider than tall, make it only take up the
		 height of the window instead.
	 */
	if (MI_WIDTH(mi) > MI_HEIGHT(mi))
	{
		scale /= MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi);
	}

	/* Constrain it to roughly life-sized on the screen, not huge.
	 */
# ifdef USE_IPHONE
	if (size > 768)
	{
		/* iPad retina */
		target_size *= 1.5;
	}
	else
# endif
	{
		GLfloat max = 500;	/* 3" on my screen... */
		if (target_size > max) target_size = max;
	}

	/**
	 * But if that would make the image larger than target_size, scale it
	 * back down again.  The image-map bits we have are 512x512, so if the
	 * image is magnified a lot, it looks pretty blocky.  It's better to
	 * have a 512x512 animation on a 1920x1080 screen that looks good
	 * than a 1024x1024 animation that looks really pixelated.
	 */
	if (size > target_size) scale *= target_size / size;

	glScalef(scale, scale, scale);
}

static void clear_rotation(void)
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

static void apply_mat(const GLfloat *specs)
{
	glMaterialfv(GL_FRONT, GL_AMBIENT, specs);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, specs + 4);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specs + 8);
	glMaterialfv(GL_FRONT, GL_SHININESS, specs + 12);
}

static int draw_star(AniState *st, StarInfo *inf)
{
	if (inf->lifetime == 0) return 0;

	glPushMatrix();

	glTranslatef(inf->x, inf->y, inf->z);
	glColor4f(inf->r, inf->g, inf->b, 0.2);
	clear_rotation();
	glRotatef(inf->rot / M_PI * 180, 0, 0, 1);

	glCallList(st->dls[DL_STAR]);

	glPopMatrix();
	return st->pcounts[DL_STAR];
}

static int draw_stars(AniState *st)
{
	int i, j, polys = 0;

	if (random() % 16 == 0)
	{
		for (j = 0; j < countof(st->stars); j++)
		{
			StarInfo *inf = &st->stars[j];
			if (!inf->lifetime)
			{
				float theta, r;
				inf->lifetime = 128;
				inf->x = 0;
				inf->y = 0;
				inf->z = 0;
				inf->r = random() / (float) RAND_MAX;
				inf->g = random() / (float) RAND_MAX;
				inf->b = random() / (float) RAND_MAX;

				inf->dy = 0.1 + random() / (float) RAND_MAX * 0.1;

				theta = random() / (float) RAND_MAX * M_PI * 2;
				r = 0.08 + random() / (float) RAND_MAX * 0.03;
				inf->dx = r * cos(theta);
				inf->dz = r * sin(theta);

				inf->rot = (random() / (float) RAND_MAX) * M_PI * 2;
				break;
			}
		}
	}

	glPushMatrix();
	apply_mat(star_mat);

	glTranslatef(0, H_TOP + 0.15, 0);
	glScalef(0.2, 0.2, 0.2);

	glBindTexture(GL_TEXTURE_2D, 0);
	for (i = 0; i < countof(st->stars); i++)
	{
		StarInfo *inf = &st->stars[i];

		polys += draw_star(st, inf);

		if (inf->lifetime)
		{
			inf->x += inf->dx;
			inf->y += inf->dy;
			inf->z += inf->dz;
			inf->dy -= 0.008;
			inf->lifetime--;
		}
	}

	glPopMatrix();
	return polys;
}

static void animate_ticking(AniState *st)
{
	if (st->time++ > 60)
	{
		st->time = 0;

		st->outer_target_theta -= M_PI_4;
	}

	st->outer_omega += (st->outer_target_theta - st->outer_theta) * 0.08; /* spring constant */
	st->outer_omega *= 0.85; /* damping */
	/* calculating the frequency of oscillation is left as an exercise to the reader */

	st->inner_omega = 0.02;

	st->outer_theta += st->outer_omega;
	st->inner_theta += st->inner_omega;
	st->base_theta -= 0.006;

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

static void animate(ModeInfo *mi)
{
	float rx, ry, rz;
	float tx, ty, tz;
	AniState *st = &sts[MI_SCREEN(mi)];
	Bool wire = MI_IS_WIREFRAME(mi);

	mi->polygon_count = 0;

	if (!wire) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else glClear(GL_COLOR_BUFFER_BIT);

	glPushMatrix();

	tx = ty = tz = 0;
	rx = M_PI_2 / 5;
	ry = 0;
	rz = 0;

#define SMOOTH(x, xmax) ((sin((x) / (xmax) * 2 * M_PI_2 - M_PI_2) + 1) / 2.0)

	switch (st->cam_state)
	{
		case CAM_NONE:
			if (st->cam_ticks > -100) break; /* cool down */
			if (random() % 1500 == 0)
			{
				st->cam_ticks = 150;
				st->cam_state = CAM_SHAKE;
			}
			else if (random() % 1500 == 0)
			{
				st->cam_ticks = 250;
				st->cam_state = CAM_SWEEP;
			}
			else if (random() % 1500 == 0)
			{
				st->cam_ticks = 400;
				st->cam_state = CAM_COOL;
			}
			else if (random() % 1500 == 0)
			{
				st->cam_ticks = 800;
				st->cam_state = CAM_NORMAL;
			}
			break;
		case CAM_SHAKE:
			rz = sin(st->cam_ticks / 5.0) * sin(st->cam_ticks / 150.0 * M_PI) * 0.1;
			break;
		case CAM_NORMAL:
			{
				float stage;
				if (st->cam_ticks > 750)
				{
					stage = SMOOTH(800 - st->cam_ticks, 50.0);
				}
				else if (st->cam_ticks < 50)
				{
					stage = 1 - SMOOTH(50 - st->cam_ticks, 50.0);
				}
				else stage = 1;

				ty = 7 * stage;

				rx = M_PI_2 / 5 + stage * (M_PI_2 - M_PI_2 / 5);
			}
			break;
		case CAM_COOL:
			{
				float stage;
				if (st->cam_ticks > 350)
				{
					stage = SMOOTH(400 - st->cam_ticks, 50.0);
				}
				else if (st->cam_ticks < 50)
				{
					stage = 1 - SMOOTH(50 - st->cam_ticks, 50.0);
				}
				else stage = 1;

				tx = stage * 4;
				ty = stage * 0.2;

				rx = M_PI_2 / 5 + stage * (M_PI_2 / 3 - M_PI_2 / 5);
			}
			break;
		case CAM_SWEEP:
			ry = -SMOOTH(250 - st->cam_ticks, 250.0) * M_PI * 4;
			break;
	}

#undef SMOOTH

	if (st->cam_ticks-- < 0) st->cam_state = CAM_NONE;

	glTranslatef(tx, ty, tz);
	gltrackball_rotate(st->trackball);
	glRotatef(rx / M_PI * 180, 1, 0, 0);
	glRotatef(ry / M_PI * 180, 0, 1, 0);
	glRotatef(rz / M_PI * 180, 0, 0, 1);


	scale_for_window(mi);
	animate_ticking(st);

	glEnable(GL_LIGHTING);
	glPushMatrix();

	glRotatef(st->base_theta / M_PI * 180, 0, 1, 0);
	glCallList(st->dls[DL_BASE]);
	mi->polygon_count += st->pcounts[DL_BASE];
	
	glPushMatrix();
	glRotatef(st->outer_theta / M_PI * 180, 0, 1, 0);
	glCallList(st->dls[DL_SPINNER_OUTER]);
	mi->polygon_count += st->pcounts[DL_SPINNER_OUTER];
	glPopMatrix();

	glPushMatrix();
	glRotatef(st->inner_theta / M_PI * 180, 0, 1, 0);
	glCallList(st->dls[DL_SPINNER_INNER]);
	mi->polygon_count += st->pcounts[DL_SPINNER_INNER];
	glPopMatrix();

	glPopMatrix();

	glDisable(GL_LIGHTING);
	mi->polygon_count += draw_stars(st);

	glPopMatrix();
}

#undef GEN_INNER_CIRCLE
#define GEN_INNER_CIRCLE(H) \
	do \
	{ \
		glNormal3f(0, 1, 0); \
		glTexCoord2f(cth * SIZE_YY * 0.5 + 0.5, sth * SIZE_YY * 0.5 + 0.5); \
		glVertex3f(cth * SIZE_YY / 2, H, sth * SIZE_YY / 2); \
	} while (0)

#define RECT(NX, NY, X1, X2, Y1, Y2, Z1, Z2) \
	do \
	{ \
		glTexCoord2f(0, 0); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X1, Y1, Z1); \
\
		glTexCoord2f(0.25, 0); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X2, Y1, Z2); \
\
		glTexCoord2f(0.25, 0.1); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X2, Y2, Z2); \
\
		glTexCoord2f(0, 0.1); \
		glNormal3f(NX, 0, NY); \
		glVertex3f(X1, Y2, Z1); \
	} while (0)


static int gen_oct_prism(AniState *st, Bool texture, Bool rotate_texture, float ymin, float ymax)
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
		glVertex3f(px - 0.5, ymin, py - 0.5);

		glTexCoord2f(uv[2], uv[3]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5, ymax, py - 0.5);

		px = OCT_POINTS[2 * i];
		py = OCT_POINTS[2 * i + 1];

		glTexCoord2f(uv[4], uv[5]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5, ymax, py - 0.5);

		glTexCoord2f(uv[6], uv[7]);
		glNormal3f(cth, 0, sth);
		glVertex3f(px - 0.5, ymin, py - 0.5);
	}
	glEnd();
	return 16;
}

static int gen_spinner_inner(AniState *st, Bool texture)
{
	const int resolution = 64;
	int i;
	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_TOP]);
	apply_mat(top_mat);

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

static int gen_trigrams(AniState *st, Bool texture)
{
	int polys = 0;
	int i;
	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_WOOD]);
	glBegin(texture ? GL_QUADS : GL_LINE_LOOP);
	apply_mat(tri_mat);

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

			glTexCoord2f(0.1, 0);
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

				glTexCoord2f(0.1, 0.25);
				glNormal3f(0, 1, 0);
				glVertex3f(xao - xdf, H_TRI, yao - ydf);

				glTexCoord2f(0, 0.25);
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

				glTexCoord2f(0, 0.25);
				glNormal3f(0, 1, 0);
				glVertex3f(xai + xdf, H_TRI, yai + ydf);

				glTexCoord2f(0.1, 0.25);
				glNormal3f(0, 1, 0);
				glVertex3f(xao + xdf, H_TRI, yao + ydf);
			}

			glTexCoord2f(0.1, 0.5);
			glNormal3f(0, 1, 0);
			glVertex3f(cup * out, H_TRI, sup * out);

			glTexCoord2f(0, 0.5);
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

static int gen_star(AniState *st, Bool texture)
{
	const float ANGLE = M_PI * 2 / 5;
	const float R = 0.2;
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

static int gen_spinner_outer(AniState *st, Bool texture)
{
	const int resolution = 16;
	int polys = 0;
	int i;

	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_TOP]);
	apply_mat(top_mat);

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
		glVertex3f(px - 0.5, H_TOP, py - 0.5);

		px = OCT_POINTS[(2 * i + 2) % 16];
		py = OCT_POINTS[(2 * i + 3) % 16];

		glTexCoord2f(px, py);
		glNormal3f(0, 1, 0);
		glVertex3f(px - 0.5, H_TOP, py - 0.5);

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
		glVertex3f(px - 0.5, H_TOP, py - 0.5);

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
			glVertex3f(px - 0.5, H_TOP, py - 0.5);
		}

		glEnd();
	}
	polys += 8 * resolution;

	polys += gen_oct_prism(st, texture, True, H_MID, H_TOP);

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
		glVertex3f(px - 0.5, H_MID, py - 0.5);
	}
	glEnd();
	polys += 6;

	polys += gen_trigrams(st, texture);

	return polys;
}

static int gen_base(AniState *st, Bool texture)
{
	int i;
	int polys = 0;
	apply_mat(base_mat);

	glBindTexture(GL_TEXTURE_2D, st->textures[TEX_STAR]);
	polys += gen_oct_prism(st, texture, False, H_BOT, H_MID);

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
		glVertex3f(px - 0.5, H_BOT, py - 0.5);
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
		glVertex3f(px - 0.5, H_MID, py - 0.5);
	}
	glEnd();
	polys += 12;

	return polys;
}

static void init_gl(ModeInfo *mi)
{
	int i;
	AniState *st = &sts[MI_SCREEN(mi)];
	Bool wire = MI_IS_WIREFRAME(mi);

	if (!wire)
	{
		static const GLfloat pos0[]	= { 0, 20, 20, 1.0 };
		static const GLfloat amb0[]	= { 0.8, 0.8, 0.8, 1.0 };
		static const GLfloat dif0[]	= { 1, 1, 1, 1.0 };

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

		st->texture = xpm_to_ximage(mi->dpy, mi->xgwa.visual,
				mi->xgwa.colormap, eight_tex);

		glGenTextures(countof(st->textures), st->textures);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		for (i = 0; i < countof(st->textures); i++)
		{
			int height = st->texture->width; /* assume square */
			glBindTexture(GL_TEXTURE_2D, st->textures[i]);

			clear_gl_error();

			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGBA,
				st->texture->width,
				height,
				0,
				GL_RGBA,
				GL_UNSIGNED_INT_8_8_8_8_REV,
				(st->texture->data + (st->texture->bytes_per_line * height * i))
			);

			check_gl_error("texture");
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			check_gl_error("texture");

			/* This makes scaled pixmaps tolerable to look at. */
#if !defined(GL_TEXTURE_LOD_BIAS) && defined(GL_TEXTURE_LOD_BIAS_EXT)
	#define GL_TEXTURE_LOD_BIAS GL_TEXTURE_LOD_BIAS_EXT
#endif
#ifdef GL_TEXTURE_LOD_BIAS
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.25);
#endif
			clear_gl_error(); /* invalid enum on iPad 3 */
		}
	}

	for (i = 0; i < countof(st->dls); i++)
	{
		st->dls[i] = glGenLists(1);
		st->pcounts[i] = 0;

		glNewList(st->dls[i], GL_COMPILE);

		switch (i)
		{
			case DL_SPINNER_INNER:
				st->pcounts[i] += gen_spinner_inner(st, !wire);
				break;
			case DL_SPINNER_OUTER:
				st->pcounts[i] += gen_spinner_outer(st, !wire);
				break;
			case DL_BASE:
				st->pcounts[i] += gen_base(st, !wire);
				break;
			case DL_STAR:
				st->pcounts[i] += gen_star(st, !wire);
				break;
			default:
				abort();
				return;
		}

		glEndList();
	}
	check_gl_error("lists");
}

ENTRYPOINT Bool eight_handle_event(ModeInfo *mi, XEvent *event)
{
	AniState *st = &sts[MI_SCREEN(mi)];

	if (gltrackball_event_handler(event, st->trackball, MI_WIDTH(mi),
			MI_HEIGHT(mi), &st->button_down_p))
	{
		return True;
	}
	else if (event->xany.type == KeyPress)
	{
		KeySym keysym;
		char c = 0;
		XLookupString(&event->xkey, &c, 1, &keysym, 0);
		if (c == ' ' || c == '\t')
		{
			return True;
		}
	}

	return False;
}

ENTRYPOINT void reshape_eight(ModeInfo *mi, int width, int height)
{
	GLfloat h = (GLfloat) height / (GLfloat) width;
	glViewport(0, 0, (GLint) width, (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -h, h, 2.0, 70.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);
	glTranslatef(0, -7, -40);
}

ENTRYPOINT void init_eight(ModeInfo *mi)
{
	AniState *st;
	if (!sts)
	{
		sts = (AniState*) calloc(MI_NUM_SCREENS(mi), sizeof(AniState));
		if (!sts)
		{
			fprintf(stderr, "%s: out of memory\n", progname);
			exit(1);
		}
	}

	st = &sts[MI_SCREEN(mi)];

	st->trackball = gltrackball_init(True);

	st->inner_omega = 0;
	st->inner_theta = 0;
	st->outer_omega = 0;
	st->outer_theta = 0;

	if ((st->glx_context = init_GL(mi)) != NULL)
	{
		reshape_eight(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		init_gl(mi);
	}
}

ENTRYPOINT void draw_eight(ModeInfo *mi)
{
	AniState *st = &sts[MI_SCREEN(mi)];
	Display *dpy = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);

	if (!st->glx_context) return;

	glDrawBuffer(GL_BACK);
	glXMakeCurrent(dpy, window, *(st->glx_context));

	animate(mi);

	if (mi->fps_p) do_fps (mi);
	
	glFinish();
	glXSwapBuffers(dpy, window);
}

ENTRYPOINT void release_eight(ModeInfo *mi)
{
	if (sts)
	{
		free(sts);
		sts = NULL;
	}
}

ENTRYPOINT void refresh_eight(ModeInfo *mi)
{
}


XSCREENSAVER_MODULE("Eight", eight)

#endif /* USE_GL */
