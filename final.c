/*
 *  Final
 *  Jigme Fritz
 *  a program that shows what I learned in graphics
 *
 *  Key bindings:
 *  arrows     Projection and Orthogonal mode: Changes view angle
 *             First Person mode: Left and Right allows user to look left and right, up and down allows forwards and backwards movement       
 *  0          Projection and Orthogonal mode: Reset view angle
 *  PGUP       In all modes increase the dim variable slightly
 *  PGDN       In all modes increase the dim variable slightly
 *  s/S        stop the light from moving
 *  -/+        In projection and first person change the field of view
 *  ESC        Exit
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#ifdef USEGLEW
#include <GL/glew.h>
#endif
//  OpenGL with prototypes for glext
#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
// Tell Xcode IDE to not gripe about OpenGL deprecation
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <GL/glut.h>
#endif
//  Default resolution
//  For Retina displays compile with -DRES=2
#ifndef RES
#define RES 1
#endif

int th=0;          //  Azimuth of view angle
int ph=0;          //  Elevation of view angle
int fh = 115;
int mode=0;  
double dim=5.0; 
int fov=55;       //  Field of view (for perspective)
double asp=1; 
int mvForward = 0; //boolean
int mvBackward = 0; // boolean

int ambient   =  1;  // Ambient intensity (%)
int diffuse   =  20;  // Diffuse intensity (%)
int specular  =   0;  // Specular intensity (%)

int zh = 90;
int move = 1;
const char* text[] = {"Orthogonal","Perspective","First Person"};
//  Cosine and Sine in degrees
#define Cos(x) (cos((x)*3.14159265/180))
#define Sin(x) (sin((x)*3.14159265/180))

double EFx = 1;
double EFy = 0.4;
double EFz = -2;
//PROB NOT GONNA NEED THIS STUFF
// typedef struct {float x,y,z;} vtx;
// #define n 500
// vtx is[n];

// TEMP STUFF
#define MAXN 64    // Maximum number of slices (n) and points in a polygon
int light;     // Light mode: true=draw polygon, false=draw shadow volume
typedef struct {float x,y,z;} Point;
Point  Lp;        // Light position in local coordinate system
Point  Nc,Ec;     // Far or near clipping plane in local coordinate system
int    inf=1;     // Infinity
int n = 8;
float Position[4];
int    depth=0;   // Stencil depth
unsigned int tex2d[2];
/*
 *  Convenience routine to output raster text
 *  Use VARARGS to make this more flexible
 */
#define LEN 8192  //  Maximum length of text string
void Print(const char* format , ...)
{
   char    buf[LEN];
   char*   ch=buf;
   va_list args;
   //  Turn the parameters into a character string
   va_start(args,format);
   vsnprintf(buf,LEN,format,args);
   va_end(args);
   //  Display the characters one at a time at the current raster position
   while (*ch)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*ch++);
}

/*
 *  Check for OpenGL errors
 */
void ErrCheck(const char* where)
{
   int err = glGetError();
   if (err) fprintf(stderr,"ERROR: %s [%s]\n",gluErrorString(err),where);
}

/*
 *  Print message to stderr and exit
 */
void Fatal(const char* format , ...)
{
   va_list args;
   va_start(args,format);
   vfprintf(stderr,format,args);
   va_end(args);
   exit(1);
}
//
//  Reverse n bytes
//
static void Reverse(void* x,const int nu)
{
   char* ch = (char*)x;
   for (int k=0;k<nu/2;k++)
   {
      char tmp = ch[k];
      ch[k] = ch[nu-1-k];
      ch[nu-1-k] = tmp;
   }
}

//
//  Load texture from BMP file
//
unsigned int LoadTexBMP(const char* file)
{
   //  Open file
   FILE* f = fopen(file,"rb");
   if (!f) Fatal("Cannot open file %s\n",file);
   //  Check image magic
   unsigned short magic;
   if (fread(&magic,2,1,f)!=1) Fatal("Cannot read magic from %s\n",file);
   if (magic!=0x4D42 && magic!=0x424D) Fatal("Image magic not BMP in %s\n",file);
   //  Read header
   unsigned int dx,dy,off,k; // Image dimensions, offset and compression
   unsigned short nbp,bpp;   // Planes and bits per pixel
   if (fseek(f,8,SEEK_CUR) || fread(&off,4,1,f)!=1 ||
       fseek(f,4,SEEK_CUR) || fread(&dx,4,1,f)!=1 || fread(&dy,4,1,f)!=1 ||
       fread(&nbp,2,1,f)!=1 || fread(&bpp,2,1,f)!=1 || fread(&k,4,1,f)!=1)
     Fatal("Cannot read header from %s\n",file);
   //  Reverse bytes on big endian hardware (detected by backwards magic)
   if (magic==0x424D)
   {
      Reverse(&off,4);
      Reverse(&dx,4);
      Reverse(&dy,4);
      Reverse(&nbp,2);
      Reverse(&bpp,2);
      Reverse(&k,4);
   }
   //  Check image parameters
   unsigned int max;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE,(int*)&max);
   if (dx<1 || dx>max) Fatal("%s image width %d out of range 1-%d\n",file,dx,max);
   if (dy<1 || dy>max) Fatal("%s image height %d out of range 1-%d\n",file,dy,max);
   if (nbp!=1)  Fatal("%s bit planes is not 1: %d\n",file,nbp);
   if (bpp!=24) Fatal("%s bits per pixel is not 24: %d\n",file,bpp);
   if (k!=0)    Fatal("%s compressed files not supported\n",file);
#ifndef GL_VERSION_2_0
   //  OpenGL 2.0 lifts the restriction that texture size must be a power of two
   for (k=1;k<dx;k*=2);
   if (k!=dx) Fatal("%s image width not a power of two: %d\n",file,dx);
   for (k=1;k<dy;k*=2);
   if (k!=dy) Fatal("%s image height not a power of two: %d\n",file,dy);
#endif

   //  Allocate image memory
   unsigned int size = 3*dx*dy;
   unsigned char* image = (unsigned char*) malloc(size);
   if (!image) Fatal("Cannot allocate %d bytes of memory for image %s\n",size,file);
   //  Seek to and read image
   if (fseek(f,off,SEEK_SET) || fread(image,size,1,f)!=1) Fatal("Error reading data from image %s\n",file);
   fclose(f);
   //  Reverse colors (BGR -> RGB)
   for (k=0;k<size;k+=3)
   {
      unsigned char temp = image[k];
      image[k]   = image[k+2];
      image[k+2] = temp;
   }

   //  Sanity check
   ErrCheck("LoadTexBMP");
   //  Generate 2D texture
   unsigned int texture;
   glGenTextures(1,&texture);
   glBindTexture(GL_TEXTURE_2D,texture);
   //  Copy image
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,dx,dy,0,GL_RGB,GL_UNSIGNED_BYTE,image);
   if (glGetError()) Fatal("Error in glTexImage2D %s %dx%d\n",file,dx,dy);
   //  Scale linearly when image size doesn't match
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

   //  Free image memory
   free(image);
   //  Return texture name
   return texture;
}

static void Project()
{
   //  Tell OpenGL we want to manipulate the projection matrix
   glMatrixMode(GL_PROJECTION);
   //  Undo previous transformations
   glLoadIdentity();
   //  Perspective transformation
   if (mode == 0)
      gluPerspective(fov,asp,dim/4,4*dim);
   //  Orthogonal projection
   else if (mode == 1)
      glOrtho(-asp*dim,+asp*dim, -dim,+dim, -dim,+dim);
   else if (mode == 2)
      gluPerspective(fov,asp,dim/4,4*dim);
   //  Switch to manipulating the model matrix
   glMatrixMode(GL_MODELVIEW);
   //  Undo previous transformations
   glLoadIdentity();
}

/*DEFINE FUNCTIONS THAT DRAW OBJECTS HERE! */

/*
 *  Draw vertex in polar coordinates with normal
 */
static void Vertex(double th,double ph)
{
   double x = Sin(th)*Cos(ph);
   double y = Cos(th)*Cos(ph);
   double z =         Sin(ph);
   //  For a sphere at the origin, the position
   //  and normal vectors are the same
   glNormal3d(x,y,z);
   glVertex3d(x,y,z);
}
/*
 *  Draw a conal looking cylinder, starts with small radius, ends with larger, downwards
 *     at (x,y,z)
 *     with scaling (dx,dy,dz)
 *     rotated around the x axis (rot)
 */
static void conal(double x,double y,double z,
                 double dx,double dy,double dz, double rot)
{
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(dx,dy,dz);
   glRotated(rot,1,0,0);
   //  White ball with yellow specular
   float yellow[]   = {1.0,1.0,0.0,1.0};
   float Emission[] = {0.0,0.0,0.01,1.0};
   glColor3f(1,1,1);
   glMaterialf(GL_FRONT,GL_SHININESS,1);
   glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   int ph = 20;
   glBegin(GL_QUAD_STRIP);
   for (int th=0;th<=360;th+=2*10)
   {
      Vertex(th,ph);
      Vertex(th,ph+50);
   }
   glEnd();
   //  Undo transofrmations
   glPopMatrix();


}
/*
 *  Draw a ball
 *     at (x,y,z)
 *     radius (r)
 */
static void ball(double x,double y,double z,double r)
{
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(r,r,r);
   //  White ball with yellow specular
   float yellow[]   = {1.0,1.0,0.0,1.0};
   float Emission[] = {0.0,0.0,0.01,1.0};
   glColor3f(1,1,1);
   glMaterialf(GL_FRONT,GL_SHININESS,1);
   glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   for (int ph=-90;ph<90;ph+=10)
   {
      glBegin(GL_QUAD_STRIP);
      for (int th=0;th<=360;th+=2*10)
      {
         Vertex(th,ph);
         Vertex(th,ph+10);
      }
      glEnd();
   }
   //  Undo transofrmations
   glPopMatrix();
}
/*
 *  Draw a ball, and then turn it into an ellipsoid!
 *     at (x,y,z)
 *     use dx,dy,dz to smush it to your liking
 */
static void ellipsoid(double x,double y,double z,double dx, double dy, double dz)
{
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(dx,dy,dz);
   //  White ball with yellow specular
   float yellow[]   = {1.0,1.0,0.0,1.0};
   float Emission[] = {0.0,0.0,0.01,1.0};
   glColor3f(1,1,1);
   glMaterialf(GL_FRONT,GL_SHININESS,1);
   glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   for (int ph=-90;ph<90;ph+=10)
   {
      glBegin(GL_QUAD_STRIP);
      for (int th=0;th<=360;th+=2*10)
      {
         Vertex(th,ph);
         Vertex(th,ph+10);
      }
      glEnd();
   }
   //  Undo transofrmations
   glPopMatrix();
}
/*
 *  Draw a cube
 *     at (x,y,z)
 *     dimensions (dx,dy,dz)
 *     rotated th about the y axis
 *     rotated zh about the z axis
 *     rotated xh about the x axis
 */
static void cube(double x,double y,double z,
                 double dx,double dy,double dz,
                 double th, double zh, double xh)
{
   float yellow[]   = {1.0,1.0,0.0,1.0};
   float Emission[] = {0.0,0.0,0.01,1.0};

   glMaterialf(GL_FRONT,GL_SHININESS,1);
   glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   //  Save transformation
   glPushMatrix();
   //  Offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glRotated(zh, 0,0,1);
   glRotated(xh, 1,0,0);
   glScaled(dx,dy,dz);
   //  Cube
   glBegin(GL_QUADS);
   //  Front
   glNormal3f(0,0,1);
   glVertex3f(-1,-1, 1);
   glVertex3f(+1,-1, 1);
   glVertex3f(+1,+1, 1);
   glVertex3f(-1,+1, 1);
   //  Back
   glNormal3f(0,0,-1);
   glVertex3f(+1,-1,-1);
   glVertex3f(-1,-1,-1);
   glVertex3f(-1,+1,-1);
   glVertex3f(+1,+1,-1);
   //  Right
   glNormal3f(1,0,0);
   glVertex3f(+1,-1,+1);
   glVertex3f(+1,-1,-1);
   glVertex3f(+1,+1,-1);
   glVertex3f(+1,+1,+1);
   //  Left
   glNormal3f(-1,0,0);
   glVertex3f(-1,-1,-1);
   glVertex3f(-1,-1,+1);
   glVertex3f(-1,+1,+1);
   glVertex3f(-1,+1,-1);
   //  Top
   glNormal3f(0,1,0);
   glVertex3f(-1,+1,+1);
   glVertex3f(+1,+1,+1);
   glVertex3f(+1,+1,-1);
   glVertex3f(-1,+1,-1);
   //  Bottom
   glNormal3f(0,-1,0);
   glVertex3f(-1,-1,-1);
   glVertex3f(+1,-1,-1);
   glVertex3f(+1,-1,+1);
   glVertex3f(-1,-1,+1);
   //  End
   glEnd();
   //  Undo transformations
   glPopMatrix();
}
//a cylinder that we can use 
static void cylinder(double x,double y,double z,double l, double r, double wid)
{
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(r,r,r);

   // float yellow[]   = {1.0,1.0,0.0,1.0};
   // float Emission[] = {0.0,0.0,0.01,1.0};

   // glMaterialf(GL_FRONT,GL_SHININESS,1);
   // glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   // glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   glBegin(GL_QUAD_STRIP);
   for (int th=0;th<=360;th+=30)
   {
      glNormal3d(wid*Cos(th),0,wid*Sin(th));
      glVertex3d(wid*Cos(th),0,wid*Sin(th));
      glNormal3d(wid*Cos(th),l,wid*Sin(th));
      glVertex3d(wid*Cos(th),l,wid*Sin(th));
   }
   //lets close this cylinder
   for (int th=0;th<=180;th+=30)
   {
      glNormal3d(0,1,0);
      glVertex3d(wid*Cos(th),0,wid*Sin(th));
      glVertex3d(wid*Cos(-th),0,wid*Sin(-th));

   }
   for (int th=0; th<= 180; th+=30){
      glNormal3d(0,-1,0);
      glVertex3d(wid*Cos(th),l,wid*Sin(th));
      glVertex3d(wid*Cos(-th),l,wid*Sin(-th));
   }
   glEnd();
   //  Undo transofrmations
   glPopMatrix();
}


static void chessBoard(double x,double y,double z,
                 double dx,double dy,double dz,
                 double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   // offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(dx,dy,dz);

   //board
   glBegin(GL_QUADS);
   glColor3f(0.58,0.29,0);
   //base of board
   glNormal3f( 0,-1, 0);
   glVertex3f(-1,0,-1);
   glVertex3f(+1,0,-1);
   glVertex3f(+1,0,+1);
   glVertex3f(-1,0,+1);
   //front side
   glNormal3f(0,0,1);
   glVertex3f(-1,0, 1);
   glVertex3f(+1,0, 1);
   glVertex3f(+1,+.2, 1);
   glVertex3f(-1,.2, 1);
   //back side
   glNormal3f(0,0,-1);
   glVertex3f(+1,0,-1);
   glVertex3f(-1,0,-1);
   glVertex3f(-1,+.2,-1);
   glVertex3f(+1,+.2,-1);
   //right side
   glNormal3f(+1, 0, 0);
   glVertex3f(+1,0,+1);
   glVertex3f(+1,0,-1);
   glVertex3f(+1,+.2,-1);
   glVertex3f(+1,+.2,+1);
   //left side
   glNormal3f(-1, 0, 0);
   glVertex3f(-1,-0,-1);
   glVertex3f(-1,-0,+1);
   glVertex3f(-1,+.2,+1);
   glVertex3f(-1,+.2,-1);
   //top front edge
   glNormal3f( 0,1, 0);
   glVertex3f(-1,.2,-1);
   glVertex3f(+1,.2,-1);
   glVertex3f(+1,.2,-.8);
   glVertex3f(-1,.2,-.8);
   //top back edge
   glNormal3f( 0,1, 0);
   glVertex3f(-1,.2,.8);
   glVertex3f(+1,.2,.8);
   glVertex3f(+1,.2, 1);
   glVertex3f(-1,.2,1);
   //top left edge
   glNormal3f( 0,1, 0);
   glVertex3f(-1,.2,-1);
   glVertex3f(-.8,.2, -1);
   glVertex3f(-.8,.2, 1);
   glVertex3f(-1,.2,1);
   //top right edge
   glNormal3f( 0,1, 0);
   glVertex3f(.8,.2,-1);
   glVertex3f(1,.2, -1);
   glVertex3f(1,.2, 1);
   glVertex3f(.8,.2,1);
   //square where texture will go
   glColor3f(0,1,0);
   glNormal3f( 0,1, 0);
   glVertex3f(-.8,.2,-.8);
   glVertex3f(.8,.2, -.8);
   glVertex3f(.8,.2, .8);
   glVertex3f(-.8,.2,.8);
   glEnd();
   //  Undo transformations
   glPopMatrix();
}


static void aTable(double x,double y,double z,
                 double dx,double dy,double dz,
                 double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   // offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(dx,dy,dz);

   glBegin(GL_QUADS);
   
   glColor3f(0.58,0.29,0);
   //base of table
   glNormal3f( 0,-1, 0);
   glVertex3f(-1,0,-1);
   glVertex3f(+1,0,-1);
   glVertex3f(+1,0,+1);
   glVertex3f(-1,0,+1);
   //front side
   glNormal3f(0,0,1);
   glVertex3f(-1,0, 1);
   glVertex3f(+1,0, 1);
   glVertex3f(+1,+.33, 1);
   glVertex3f(-1,.33, 1);
   //back side
   glNormal3f(0,0,-1);
   glVertex3f(+1,0,-1);
   glVertex3f(-1,0,-1);
   glVertex3f(-1,+.33,-1);
   glVertex3f(+1,+.33,-1);
   //right side
   glNormal3f(+1, 0, 0);
   glVertex3f(+1,0,+1);
   glVertex3f(+1,0,-1);
   glVertex3f(+1,+.33,-1);
   glVertex3f(+1,+.33,+1);
   //left side
   glNormal3f(-1, 0, 0);
   glVertex3f(-1,-0,-1);
   glVertex3f(-1,-0,+1);
   glVertex3f(-1,+.33,+1);
   glVertex3f(-1,+.33,-1);
   //top of table
   glNormal3f( 0,1, 0);
   glVertex3f(-1,.33,-1);
   glVertex3f(+1,.33,-1);
   glVertex3f(+1,.33,+1);
   glVertex3f(-1,.33,+1);
   glEnd();
   //legs
   cylinder(-.90,0,-.90, -1.5, 1,0.1);
   cylinder(-.90,0,.90, -1.5, 1,0.1);
   cylinder(.90,0,.90, -1.5, 1,0.1);
   cylinder(.90,0,-.90, -1.5, 1,0.1);
   //  Undo transformations
   glPopMatrix();
}


static void pawn(double x,double y,double z,
                 double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   // offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   ball(0,3.31,0,.6);
   conal(0,-.71,0,1,4,1, -90);
   cylinder(0,.69,0,-.69,1,1.1);
   glPopMatrix();
}
static void bishop(double x, double y, double z,
                   double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   // offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   conal(0,-1.11,0,1,5,1, -90);
   ellipsoid(0,4.04,0,.5,.75,.5);
   ellipsoid(0,4.79,0, .2, .15, .2);
   cylinder(0,.69,0,-.69,1,1.1);
   glPopMatrix();
}
static void rook(double x, double y, double z,
                   double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);

   //base 
   cylinder(0,0.69,0,-.69,1,1.1);
   conal(0,-1.11,0,1,5,1, -90);
   cylinder(0,4.2,0,-.7, 1, .75);
   ball(.55,4.3,0,.18);
   ball(-.55,4.3,0,.18);
   ball(0,4.3,.55,.18);
   ball(0,4.3,-.55,.18);
   ball(-.40,4.3,-.40,.18);
   ball(.40,4.3,.40,.18);
   ball(.40,4.3,-.40,.18);
   ball(-.40,4.3,.40,.18);
   glPopMatrix();
}
static void king(double x, double y, double z,
                   double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   //le piece
   cylinder(0,0.69,0,-.69,1,1);
   conal(0,-1.11,0,1,4,1, -90);
   ellipsoid(0,2.7,0,.4,.1,.4);
   conal(0,4.4,0,.7,2,.7,90);
   ellipsoid(0,3.8,0,.7,.2,.7);
   cube(0,4.2,0,.11,.5,.08,0,0,0);
   cube(0,4.4,0,.3,.1,.08,0,0,0);
   glPopMatrix();
}
// draws the queen piece at (x,y,z) scaled by r, rotated around the y axis by th
static void queen(double x, double y, double z, double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   //le piece
   cylinder(0,0.69,0,-.69,1,1);
   conal(0,-1.11,0,1,4,1, -90);
   ellipsoid(0,2.7,0,.4,.1,.4);
   conal(0,4.4,0,.7,1.8,.7,90);
   ellipsoid(0,3.8,0,.7,.2,.7);
   conal(0,3.75,0,.5,.5,.5, -90);
   ball(0,4.3,0,.22);
   glPopMatrix();
}
// draws the knight piece at (x,y,z) scaled by r, rotated around the y axis by th
static void knight(double x, double y, double z, double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   //le piece
   cylinder(0,0.69,0,-.69,1,1);
   conal(0,-1,0,1,3,1, -90);
   cylinder(0,1,0,1.3,1,.4);
   ball(0,2.5,0,.5);
   cube(-.6,2.4,0,.5,.3,.3,0,20,0);
   ellipsoid(-1.15,2.2,0,.3,.4,.55);
   ball(0,2.69,.4,.12);
   ball(0,2.69,-.4,.12);
   //funny eyebrows i made on accident
   ellipsoid(0,2.8,.4,.2,.1,.1);
   ellipsoid(0,2.8,-.4,.2,.1,.1);
   //ears
   ellipsoid(.2,2.9,.4,.1,.2,.1);
   ellipsoid(.2,2.9,-.4,.1,.2,.1);
   glPopMatrix();
}
//expirement **********************************************************************

/*
 *  Set color with shadows
 */
void Color(float R,float G,float B)
{
   //  Use black to draw shadowed objects for demonstration
   //  purposes in mode=3 (lit parts of objects only)
   if (light<0 && mode==3)
      glColor3f(0,0,0);
   //  Shaded color is 1/2 of lit color
   else if (light<0)
      glColor3f(0.5*R,0.5*G,0.5*B);
   //  Lit color
   else
      glColor3f(R,G,B);
}

/*
 *  Calculate shadow location
 */
Point Shadow(Point P)
{
   double lambda;
   Point  S;

   //  Fixed lambda
   if (inf)
      lambda = 1024;
   //  Calculate lambda for clipping plane
   else
   {
      // lambda = (E-L).N / (P-L).N = (E.N - L.N) / (P.N - L.N)
      double LN = Lp.x*Nc.x+Lp.y*Nc.y + Lp.z*Nc.z;
      double PLN = P.x*Nc.x+P.y*Nc.y+P.z*Nc.z - LN;
      lambda = (fabs(PLN)>1e-10) ? (Ec.x*Nc.x+Ec.y*Nc.y+Ec.z*Nc.z - LN)/PLN : 1024;
      //  If lambda<0, then the plane is behind the light
      //  If lambda [0,1] the plane is between the light and the object
      //  So make lambda big if this is true
      if (lambda<=1) lambda = 1024;
   }

   //  Calculate shadow location
   S.x = lambda*(P.x-Lp.x) + Lp.x;
   S.y = lambda*(P.y-Lp.y) + Lp.y;
   S.z = lambda*(P.z-Lp.z) + Lp.z;
   return S;
}

/*
 *  Draw polygon or shadow volume
 *    P[] array of vertexes making up the polygon
 *    N[] array of normals (not used with shadows)
 *    T[] array of texture coordinates (not used with shadows)
 *    n   number of vertexes
 *  Killer fact: the order of points MUST be CCW
 */
void DrawPolyShadow(Point P[],Point N[],Point T[],int nV)
{
   //  Draw polygon with normals and textures
   if (light)
   {
      glBegin(GL_POLYGON);
      for (int k=0;k<nV;k++)
      {
         glNormal3f(N[k].x,N[k].y,N[k].z);
         glTexCoord2f(T[k].x,T[k].y);
         glVertex3f(P[k].x,P[k].y,P[k].z);
      }
      glEnd();
   }
   //  Draw shadow volume
   else
   {
      //  Check if polygon is visible
      int vis = 0;
      for (int k=0;k<nV;k++)
         vis = vis | (N[k].x*(Lp.x-P[k].x) + N[k].y*(Lp.y-P[k].y) + N[k].z*(Lp.z-P[k].z) >= 0);
      //  Draw shadow volume only for those polygons facing the light
      if (vis)
      {
         //  Shadow coordinates (at infinity)
         Point S[MAXN];
         if (nV>MAXN) Fatal("Too many points in polygon %d\n",nV);
         //  Project shadow
         for (int k=0;k<nV;k++)
            S[k] = Shadow(P[k]);
         //  Front face
         glBegin(GL_POLYGON);
         for (int k=0;k<nV;k++)
            glVertex3f(P[k].x,P[k].y,P[k].z);
         glEnd();
         //  Back face
         glBegin(GL_POLYGON);
         for (int k=nV-1;k>=0;k--)
            glVertex3f(S[k].x,S[k].y,S[k].z);
         glEnd();
         //  Sides
         glBegin(GL_QUAD_STRIP);
         for (int k=0;k<=nV;k++)
         {
            glVertex3f(P[k%nV].x,P[k%nV].y,P[k%nV].z);
            glVertex3f(S[k%nV].x,S[k%nV].y,S[k%nV].z);
         }
         glEnd();
      }
   }
}
/*
 *  Perform Crout LU decomposition of M in place
 *     M  4x4 matrix
 *     I  Pivot index
 *  Calls Fatal if the matrix is singular
 */
#define M(i,j) (M[4*j+i])
void Crout(double M[16],int I[4])
{
   int i,j,k;  // Counters

   //  Initialize index
   for (j=0;j<4;j++)
      I[j] = j;

   //  Pivot matrix to maximize diagonal
   for (j=0;j<3;j++)
   {
      //  Find largest absolute value in this column (J)
      int J=j;
      for (k=j+1;k<4;k++)
         if (fabs(M(k,j)) > fabs(M(J,j))) J = k;
      //  Swap rows if required
      if (j!=J)
      {
         k=I[j]; I[j]=I[J]; I[J]=k;
         for (k=0;k<4;k++)
         {
            double t=M(j,k); M(j,k)=M(J,k); M(J,k)=t;
         }
      }
   }

   //  Perform Crout LU decomposition
   for (j=0;j<4;j++)
   {
      //  Upper triangular matrix
      for (i=j;i<4;i++)
         for (k=0;k<j;k++)
            M(j,i) -= M(k,i)*M(j,k);
      if (fabs(M(j,j))<1e-10) Fatal("Singular transformation matrix\n");
      //  Lower triangular matrix
      for (i=j+1;i<4;i++)
      {
         for (k=0;k<j;k++)
            M(i,j) -= M(k,j)*M(i,k);
         M(i,j) /= M(j,j);
      }
   }
}

/*
 *  Backsolve LU decomposition
 *     M  4x4 matrix
 *     I  Pivot index
 *     Bx,By,Bz,Bw is RHS
 *     Returns renormalized Point
 */
Point Backsolve(double M[16],int I[4],double Bx,double By,double Bz,double Bw)
{
   int    i,j;                  //  Counters
   double x[4];                 //  Solution vector
   Point  X;                    //  Solution Point
   double b[4] = {Bx,By,Bz,Bw}; //  RHS

   //  Backsolve
   for (i=0;i<4;i++)
   {
      x[i] = b[I[i]];
      for (j=0;j<i;j++)
         x[i] -= M(i,j)*x[j];
   }
   for (i=3;i>=0;i--)
   {
      for (j=i+1;j<4;j++)
         x[i] -= M(i,j)*x[j];
      x[i] /= M(i,i);
   }

   //  Renormalize
   if (fabs(x[3])<1e-10) Fatal("Light position W is zero\n");
   X.x = x[0]/x[3];
   X.y = x[1]/x[3];
   X.z = x[2]/x[3];
   return X;
}

/*
 *  Set up transform
 *     Push and apply transformation
 *     Calculate light position in local coordinate system (Lp)
 *     Calculate clipping plane in local coordinate system (Ec,Nc)
 *  Global variables set: Lp,Nc,Ec
 *  Killer fact 1:  You MUST call this to set transforms for objects
 *  Killer fact 2:  You MUST call glPopMatrix to undo the transform
 */
void Transform(float x0,float y0,float z0,
               float Sx,float Sy,float Sz,
               float th,float ph)
{
   double M[16]; // Transformation matrix
   int    I[4];  // Pivot
   double Z;     // Location of clip plane

   //  Save current matrix
   glPushMatrix();

   //  Build transformation matrix and copy into M
   glPushMatrix();
   glLoadIdentity();
   glTranslated(x0,y0,z0);
   glRotated(ph,1,0,0);
   glRotated(th,0,1,0);
   glScaled(Sx,Sy,Sz);
   glGetDoublev(GL_MODELVIEW_MATRIX,M);
   glPopMatrix();

   //  Apply M to existing transformations
   glMultMatrixd(M);

   /*
    *  Determine light position in this coordinate system
    */
   Crout(M,I);
   Lp = Backsolve(M,I,Position[0],Position[1],Position[2],Position[3]);

   /*
    *  Determine clipping plane E & N
    *  Use the entire MODELVIEW matrix here
    */
   glGetDoublev(GL_MODELVIEW_MATRIX,M);
   //  Normal is down the Z axis (0,0,1) since +/- doesn't matter here
   //  The normal matrix is the inverse of the transpose of M
   Nc.x = M(2,0);
   Nc.y = M(2,1);
   Nc.z = M(2,2);
   //  Far  clipping plane for Z-fail should be just less than 8*dim
   //  Near clipping plane for Z-pass should be just more than dim/8
   Crout(M,I);
   Z = (mode==5) ? -7.9*dim : -0.13*dim;
   Ec = Backsolve(M,I,0,0,Z,1);
}
/*
 *  Draw a cylinder
 */
static void Cylinder(float x,float y,float z , float th,float ph , float R,float H)
{
   int i,j;   // Counters
   int N=4*n; // Number of slices

   Transform(x,y,z,R,R,H,th,ph);
   //  Two end caps (fan of triangles)
   for (j=-1;j<=1;j+=2)
      for (i=0;i<N;i++)
      {
         float th0 = j* i   *360.0/N;
         float th1 = j*(i+1)*360.0/N;
         Point P[3] = { {0,0,j} , {Cos(th0),Sin(th0),j} , {Cos(th1),Sin(th1),j} };
         Point N[3] = { {0,0,j} , {       0,       0,j} , {       0,       0,j} };
         Point T[3] = { {0,0,0} , {Cos(th0),Sin(th0),0} , {Cos(th1),Sin(th1),0} };
         DrawPolyShadow(P,N,T,3);
      }
   //  Cylinder Body (strip of quads)
   for (i=0;i<N;i++)
   {
      float th0 =  i   *360.0/N;
      float th1 = (i+1)*360.0/N;
      Point P[4] = { {Cos(th0),Sin(th0),+1} , {Cos(th0),Sin(th0),-1} , {Cos(th1),Sin(th1),-1} , {Cos(th1),Sin(th1),+1} };
      Point N[4] = { {Cos(th0),Sin(th0), 0} , {Cos(th0),Sin(th0), 0} , {Cos(th1),Sin(th1), 0} , {Cos(th1),Sin(th1), 0} };
      Point T[4] = { {       0,th0/90.0, 0} , {       2,th0/90.0, 0} , {       2,th1/90.0, 0} , {       0,th1/90.0, 0} };
      DrawPolyShadow(P,N,T,4);
   }

   glPopMatrix();
}
/*
 *  Draw a sphere
 */
static void Sphere(float x,float y,float z , float th,float ph , float dx,float dy, float dz)
{
   int i,j;   // Counters
   int N=4*n; // Number of slices

   Transform(x,y,z,dx,dy,dz,th,ph);


   //  sphere
   for(int phi = -90; phi < 90; phi+=10)
   {
      for (i=0;i<N;i++)
      {
         float th0 =  i   *360.0/N;
         float th1 = (i+1)*360.0/N;
         Point P[4] = { {Sin(th0)*Cos(phi),Cos(th0)*Cos(phi),Sin(phi)} , {Sin(th0)*Cos(phi+10),Cos(th0)*Cos(phi+10),Sin(phi+10)} , {Sin(th1)*Cos(phi+10),Cos(th1)*Cos(phi+10),Sin(phi+10)} , {Sin(th1)*Cos(phi),Cos(th1)*Cos(phi),Sin(phi)} };
         Point N[4] = { {Sin(th0)*Cos(phi),Cos(th0)*Cos(phi),Sin(phi)} , {Sin(th0)*Cos(phi+10),Cos(th0)*Cos(phi+10),Sin(phi+10)} , {Sin(th1)*Cos(phi+10),Cos(th1)*Cos(phi+10),Sin(phi+10)} , {Sin(th1)*Cos(phi),Cos(th1)*Cos(phi),Sin(phi)} };
         Point T[4] = { {       0,th0/90.0, 0} , {       2,th0/90.0, 0} , {       2,th1/90.0, 0} , {       0,th1/90.0, 0} }; //this is wrong, dont use textures, dont have time to implement
         DrawPolyShadow(P,N,T,4);
      }
   }

   // double x = Sin(th)*Cos(ph);
   // double y = Cos(th)*Cos(ph);
   // double z =         Sin(ph);
   // //  For a sphere at the origin, the position
   // //  and normal vectors are the same
   // glNormal3d(x,y,z);
   // glVertex3d(x,y,z);
   glPopMatrix();
}
/*
 *  Draw a cube
 */
static void Cube(float x,float y,float z , float th,float ph , float D)
{
   //  Vertexes
   Point P1[] = { {-1,-1,+1} , {+1,-1,+1} , {+1,+1,+1} , {-1,+1,+1} }; //  Front
   Point P2[] = { {+1,-1,-1} , {-1,-1,-1} , {-1,+1,-1} , {+1,+1,-1} }; //  Back
   Point P3[] = { {+1,-1,+1} , {+1,-1,-1} , {+1,+1,-1} , {+1,+1,+1} }; //  Right
   Point P4[] = { {-1,-1,-1} , {-1,-1,+1} , {-1,+1,+1} , {-1,+1,-1} }; //  Left
   Point P5[] = { {-1,+1,+1} , {+1,+1,+1} , {+1,+1,-1} , {-1,+1,-1} }; //  Top
   Point P6[] = { {-1,-1,-1} , {+1,-1,-1} , {+1,-1,+1} , {-1,-1,+1} }; //  Bottom
   //  Normals
   Point N1[] = { { 0, 0,+1} , { 0, 0,+1} , { 0, 0,+1} , { 0, 0,+1} }; //  Front
   Point N2[] = { { 0, 0,-1} , { 0, 0,-1} , { 0, 0,-1} , { 0, 0,-1} }; //  Back
   Point N3[] = { {+1, 0, 0} , {+1, 0, 0} , {+1, 0, 0} , {+1, 0, 0} }; //  Right
   Point N4[] = { {-1, 0, 0} , {-1, 0, 0} , {-1, 0, 0} , {-1, 0, 0} }; //  Left
   Point N5[] = { { 0,+1, 0} , { 0,+1, 0} , { 0,+1, 0} , { 0,+1, 0} }; //  Top
   Point N6[] = { { 0,-1, 0} , { 0,-1, 0} , { 0,-1, 0} , { 0,-1, 0} }; //  Bottom
   //  Textures
   Point T[] = { {0,0,0} , {1,0,0} , {1,1,0} , {0,1,0} };

   Transform(x,y,z,D,D,D,th,ph);
   Color(1,1,1);
   DrawPolyShadow(P1,N1,T,4); //  Front
   DrawPolyShadow(P2,N2,T,4); //  Back
   DrawPolyShadow(P3,N3,T,4); //  Right
   DrawPolyShadow(P4,N4,T,4); //  Left
   DrawPolyShadow(P5,N5,T,4); //  Top
   DrawPolyShadow(P6,N6,T,4); //  Bottom
   glPopMatrix();
}
static void Conal(float x,float y,float z , float th,float ph , float dx,float dy, float dz)
{
   int i,j;   // Counters
   int N=4*n; // Number of slices

   Transform(x,y,z,dx,dy,dz,th,ph);
   // Body (strip of quads)
   for (i=0;i<N;i++)
   {
      float th0 =  i   *360.0/N;
      float th1 = (i+1)*360.0/N;
      Point P[4] = { {1.5*Cos(th0),1.5*Sin(th0),+1} , {.75*Cos(th0),.75*Sin(th0),-1} , {.75*Cos(th1),.75*Sin(th1),-1} , {1.5*Cos(th1),1.5*Sin(th1),+1} };
      Point N[4] = { {1.5*Cos(th0),1.5*Sin(th0),0} , {.75*Cos(th0),.75*Sin(th0),0} , {.75*Cos(th1),.75*Sin(th1),0} , {1.5*Cos(th1),1.5*Sin(th1),0} };
      Point T[4] = { {       0,th0/90.0, 0} , {       2,th0/90.0, 0} , {       2,th1/90.0, 0} , {       0,th1/90.0, 0} }; //textures are broken dont use
      DrawPolyShadow(P,N,T,4);
   }

   glPopMatrix();
}
//refactored pawn
static void Pawn(double x,double y,double z,
                 double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   // offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   // ball(0,3.31,0,.6);
   // conal(0,-.71,0,1,4,1, -90);
   // cylinder(0,.69,0,-.69,1,1.1);
   Color(1,1,1);
   Cylinder(0,0,0,0,90,1,.2);
   Conal(0,.8,0,0,90,.5,.5,.9);
   Sphere(0,2.1,0,0,0,.57,.57,.57);
   glPopMatrix();
}
//refactored bishop, has shadows
static void Bishop(double x,double y,double z,
                 double r, double th)
{
   //  Set specular color to white
   float white[] = {1,1,1,1};
   float black[] = {0,0,0,1};
   glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,1);
   glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
   glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
   glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,black);
   //  Save transformation
   glPushMatrix();
   // offset
   glTranslated(x,y,z);
   glRotated(th,0,1,0);
   glScaled(r,r,r);
   //bishop
   // conal(0,-1.11,0,1,5,1, -90);
   // ellipsoid(0,4.04,0,.5,.75,.5);
   // ellipsoid(0,4.79,0, .2, .15, .2);
   // cylinder(0,.69,0,-.69,1,1.1);
   Color(1,1,1);
   Cylinder(0,0,0,0,90,1,.2);
   Conal(0,2.0,0,0,90,.5,.5,1.8);
   Sphere(0,4.3,0,0,0,.5,.90,.5);
   Sphere(0,5.2,0,0,0,.12,.07,.12);
   glPopMatrix();

}
/*
 *  Enable lighting
 */
static void Light(int on)
{
   if (on)
   {
      float Ambient[]   = {0.3,0.3,0.3,1.0};
      float Diffuse[]   = {1,1,1,1};
      //  Enable lighting with normalization
      glEnable(GL_LIGHTING);
      glEnable(GL_NORMALIZE);
      //  glColor sets ambient and diffuse color materials
      glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      glEnable(GL_COLOR_MATERIAL);
      //  Enable light 0
      glEnable(GL_LIGHT0);
      glLightfv(GL_LIGHT0,GL_AMBIENT ,Ambient);
      glLightfv(GL_LIGHT0,GL_DIFFUSE ,Diffuse);
      glLightfv(GL_LIGHT0,GL_POSITION,Position);
   }
   else
      glDisable(GL_LIGHTING);
}

/*
 *  Draw scene
 *    light>0  lit colors
 *    light<0  shaded colors
 *    light=0  shadow volumes
 */
void Scene(int Light)
{
   int k;  // Counters used to draw floor
 
   //  Set global light switch (used in DrawPolyShadow)
   light = Light;

   //  Texture (crate.bmp) (for now)
   // glBindTexture(GL_TEXTURE_2D,tex2d[1]);
   //  Enable textures if not drawing shadow volumes
   // if (light) glEnable(GL_TEXTURE_2D);

   //  Draw objects         x    y   z          th,ph    dims   
   // aTable(0,-1,0,1,1,1,0);

   // Cube(0,-1,0,0,0,.3);
   // glBindTexture(GL_TEXTURE_2D,tex2d[0]);
   // Cylinder(0,0,0,0,90,.2,.2);
   // Sphere(0,0,0,0,0,.3,.3,.3);
   // Conal(0,0,0,0,0,.1,.1,.1);
   // Pawn(0,-1,0,1,0);
   // bishop(0,-1,0,1,0);
   Bishop(0,-1,0,1,0);
   //  Disable textures
   // if (light) glDisable(GL_TEXTURE_2D);
   
   //  The floor, ceiling and walls don't cast a shadow, so bail here
   // if (!light) return;

   // //  Always enable textures
   // glEnable(GL_TEXTURE_2D);
   // Color(1,1,1);

   // //  Water texture for floor and ceiling
   // glBindTexture(GL_TEXTURE_2D,tex2d[0]);
   // for (k=-1;k<=box;k+=2)
   //    Wall(0,0,0, 0,90*k , 8,8,box?6:2 , 4);
   // //  Crate texture for walls
   // glBindTexture(GL_TEXTURE_2D,tex2d[1]);
   // for (k=0;k<4*box;k++)
   //    Wall(0,0,0, 90*k,0 , 8,box?6:2,8 , 1);

   // //  Disable textures
   // glDisable(GL_TEXTURE_2D);
}

/*
 *  Draw scene using shadow volumes
 */
static void DrawSceneWithShadows()
{
   //  PASS 1:
   //  Draw whole scene as shadowed
   //  Lighting is still off
   //  The color should be what the object looks like when in the shadow
   //  This sets the object depths in the Z-buffer
   Scene(-1);

   //  Make color buffer and Z buffer read-only
   glColorMask(0,0,0,0);
   glDepthMask(0);
   //  Enable stencil
   glEnable(GL_STENCIL_TEST);
   //  Always draw regardless of the stencil value
   glStencilFunc(GL_ALWAYS,0,0xFFFFFFFF);

   //  Enable face culling
   glEnable(GL_CULL_FACE);



   //  Z-pass variation
   //  Count from the eye to the object

      //  PASS 2:
      //  Draw only the front faces of the shadow volume
      //  Increment the stencil value on Z pass
      //  Depth and color buffers unchanged
      glFrontFace(GL_CCW);
      glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
      Scene(0);
      //  PASS 3:
      //  Draw only the back faces of the shadow volume
      //  Decrement the stencil value on Z pass
      //  Depth and color buffers unchanged
      glFrontFace(GL_CW);
      glStencilOp(GL_KEEP,GL_KEEP,GL_DECR);
      Scene(0);


   //  Disable face culling
   glDisable(GL_CULL_FACE);
   //  Make color mask and depth buffer read-write
   glColorMask(1,1,1,1);
   //  Update the color only where the stencil value is 0
   //  Do not change the stencil
   glStencilFunc(GL_EQUAL,0,0xFFFFFFFF);
   glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
   //  Redraw only if depth matches front object
   glDepthFunc(GL_EQUAL);

   //  PASS 4:
   //  Enable lighting
   //  Render the parts of objects not in shadows
   //  (The mode test is for demonstrating unlit objects only)
   Light(1);
   if (4) Scene(1); // 4 was once variable mode

   //  Undo changes (no stencil test, draw if closer and update Z-buffer)
   glDisable(GL_STENCIL_TEST);
   glDepthFunc(GL_LESS); 
   glDepthMask(1);
   //  Disable lighting
   Light(0);
}

//************************************************************
/*
 *  OpenGL (GLUT) calls this routine to display the scene
 */
void display()
{
   //  Erase the window and the depth buffer
   glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
   //  Enable Z-buffering in OpenGL
   glEnable(GL_DEPTH_TEST);
   //  Undo previous transformations
   glLoadIdentity();
   if (mode == 0)
   {
      double Ex = -2*dim*Sin(th)*Cos(ph);
      double Ey = +2*dim        *Sin(ph);
      double Ez = +2*dim*Cos(th)*Cos(ph);
      gluLookAt(Ex,Ey,Ez , 0,0,0 , 0,Cos(ph),0);
   }
   //  Orthogonal - set world orientation
   else if (mode == 1)
   {
      glRotatef(ph,1,0,0);
      glRotatef(th,0,1,0);
   }
   else if (mode == 2)
   {
      double Dx = Cos(fh); //Cos(th)
      double Dy = 0;
      double Dz = Sin(fh); //Sin(th)
      if(mvForward){
         EFx = EFx + 0.1*Dx;
         EFy = 0.4;
         EFz = EFz + 0.1*Dz;
      }
      if(mvBackward){
         EFx = EFx - 0.1*Dx;
         EFy = 0.4;
         EFz = EFz - 0.1*Dz;
      }

      double Cx = EFx + Dx;
      double Cy = EFy + Dy;
      double Cz = EFz + Dz;

      mvForward = 0;
      mvBackward = 0;
      gluLookAt(EFx,EFy,EFz , Cx,Cy,Cz , 0,1,0);

   }

   //  Light position
   Position[0]  = 5*Cos(zh);
   Position[1] = 5*Sin(zh);
   Position[2] = 0;
   Position[3] = 1;
   // float Position[] = {1,1,1};
   //  Draw light position as ball (still no lighting here)
   glColor3f(1,1,1);
   ball(Position[0],Position[1],Position[2] , 0.1);

   //  Decide what to draw
   /*PUT FUNCTIONS THAT DRAW HERE! */
   // chessBoard(0,0,0,1,1,1,190);
   // cylinder(0,-2,0,1);
   // aTable(0,0,0,1,1,1,0);
   // conal(0,0,0,1,5,1, -90);
   // pawn(.7,0,.7,.4,0);
   // bishop(0,0,0,.5,0);
   // rook(-.7,0,-.7,.5,0);
   // king(0,0,0,.6,0);
   // queen(0,-.5,0,.55,0);
   // knight(0,-0.5,0,.5,0);

   //EXPERIMENTING WITH SHADOWS 
   //a table 

   DrawSceneWithShadows();

   //  Five pixels from the lower left corner of the window
   glWindowPos2i(20,20);
   //  Print the text string

   Print("(Orthogonal/perspective)Angle=%d,%d  Dim=%.1f FOV=%d Projection=%s",th,ph,dim,fov,text[mode]);
   
   //  Render the scene
   ErrCheck("display");
   glFlush();
   glutSwapBuffers();
}

void idle()
{
   //  Elapsed time in seconds
   double t = glutGet(GLUT_ELAPSED_TIME)/1000.0;
   zh = fmod(90*t,360.0);
   //  Tell GLUT it is necessary to redisplay the scene
   glutPostRedisplay();
}


/*
 *  GLUT calls this routine when an arrow key is pressed
 */
void special(int key,int x,int y)
{
   //  Right arrow key - increase angle by 5 degrees
   if (key == GLUT_KEY_RIGHT)
      if (mode == 2){
         fh += 5;
      }
      else{
         th += 5;
      }
   //  Left arrow key - decrease angle by 5 degrees
   else if (key == GLUT_KEY_LEFT)
      if (mode == 2){
         fh -= 5;
      }
      else{
         th -= 5;
      }
   //  Up arrow key - increase elevation by 5 degrees
   else if (key == GLUT_KEY_UP)
      if (mode == 2){
         mvForward = 1;
      }
      else{
         ph += 5;
      }
   //  Down arrow key - decrease elevation by 5 degrees
   else if (key == GLUT_KEY_DOWN)
      if (mode == 2){
         mvBackward = 1;
      }
      else{
         ph -= 5;
      }
   //  PageUp key - increase dim
   else if (key == GLUT_KEY_PAGE_UP)
      dim += 0.1;
   //  PageDown key - decrease dim
   else if (key == GLUT_KEY_PAGE_DOWN && dim>1)
      dim -= 0.1;
   //  Keep angles to +/-360 degrees
   th %= 360;
   ph %= 360;
   //  Update projection
   Project();
   //  Tell GLUT it is necessary to redisplay the scene
   glutPostRedisplay();
}

/*
 *  GLUT calls this routine when a key is pressed
 */
void key(unsigned char ch,int x,int y)
{
   //  Exit on ESC
   if (ch == 27)
      exit(0);
   //  Reset view angle
   else if (ch == '0')
      th = ph = 0;
   //  Switch display mode
   else if (ch == 'm' || ch == 'M')
      mode = (mode + 1)%3;
   //  Change field of view angle
   else if (ch == '-' && ch>1)
      fov--;
   else if (ch == '+' && ch<179)
      fov++;
   else if (ch == 's' || ch == 'S')
      move = 1-move;
   glutIdleFunc(move?idle:NULL);
   //  Reproject
   Project();
   //  Tell GLUT it is necessary to redisplay the scene
   glutPostRedisplay();
}

/*
 *  GLUT calls this routine when the window is resized
 */
void reshape(int width,int height)
{
   //  Ratio of the width to the height of the window
   asp = (height>0) ? (double)width/height : 1;
   //  Set the viewport to the entire window
   glViewport(0,0, RES*width,RES*height);
   //  Set projection
   Project();
}


/*
 *  Start up GLUT and tell it what to do
 */
int main(int argc,char* argv[])
{
   //  Initialize GLUT and process user parameters
   glutInit(&argc,argv);
   //  Request double buffered, true color window with Z buffering at 600x600
   glutInitWindowSize(800,800);
   glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_STENCIL);
   //  Create the window
   glutCreateWindow("Jigme Fritz");
#ifdef USEGLEW
   //  Initialize GLEW
   if (glewInit()!=GLEW_OK) Fatal("Error initializing GLEW\n");
#endif
   glClearColor(0,0,0,0);
   //  Tell GLUT to call "display" when the scene should be drawn
   glutDisplayFunc(display);
   //  Tell GLUT to call "reshape" when the window is resized
   glutReshapeFunc(reshape);
   //  Tell GLUT to call "special" when an arrow key is pressed
   glutSpecialFunc(special);
   //  Tell GLUT to call "key" when a key is pressed
   glutKeyboardFunc(key);

   glutIdleFunc(idle);

   glGetIntegerv(GL_STENCIL_BITS,&depth);
   if (depth<=0) Fatal("No stencil buffer\n");
   tex2d[0] = LoadTexBMP("water.bmp");
   tex2d[1] = LoadTexBMP("crate.bmp");;

   //  Pass control to GLUT so it can interact with the user
   glutMainLoop();
   return 0;
}
