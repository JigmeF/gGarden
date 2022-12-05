/*
 *  HW5:Lighting
 *  Jigme Fritz
 *  some objects in space, in three different perspectives!
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

typedef struct {float x,y,z;} vtx;
#define n 500
vtx is[n];

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
   glNormal3f( 0,1, 0);
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
   glColor3f(1,1,1);
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



/*
 *  OpenGL (GLUT) calls this routine to display the scene
 */
void display()
{
   //  Erase the window and the depth buffer
   glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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
   //light bulb
      //  Translate intensity to color vectors
   float Ambient[]   = {0.01*ambient ,0.01*ambient ,0.01*ambient ,1.0};
   float Diffuse[]   = {0.01*diffuse ,0.01*diffuse ,0.01*diffuse ,1.0};
   float Specular[]  = {0.01*specular,0.01*specular,0.01*specular,1.0};
   //  Light position
   float Position[]  = {5*Cos(zh),0,5*Sin(zh),1.0};
   //  Draw light position as ball (still no lighting here)
   glColor3f(1,1,1);
   ball(Position[0],Position[1],Position[2] , 0.1);
   //  OpenGL should normalize normal vectors
   glEnable(GL_NORMALIZE);
   //  Enable lighting
   glEnable(GL_LIGHTING);
   //  Location of viewer for specular calculations
   glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);
   //  glColor sets ambient and diffuse color materials
   glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);
   //  Enable light 0
   glEnable(GL_LIGHT0);
   //  Set ambient, diffuse, specular components and position of light 0
   glLightfv(GL_LIGHT0,GL_AMBIENT ,Ambient);
   glLightfv(GL_LIGHT0,GL_DIFFUSE ,Diffuse);
   glLightfv(GL_LIGHT0,GL_SPECULAR,Specular);
   glLightfv(GL_LIGHT0,GL_POSITION,Position);

   
   
   
   //  Decide what to draw
   /*PUT FUNCTIONS THAT DRAW HERE! */
   // aHouse(.5,.25,1,.7,.5,.5,190);
   // aHouse(-.75,.25,0,.3,.5,.2,80);
   // aCamera(1,2,1,.2,.2,.2,42,83);
   chessBoard(0,0,0,1,1,1,190);
   glDisable(GL_LIGHTING);

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
   glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
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
   //  Pass control to GLUT so it can interact with the user
   glutMainLoop();
   return 0;
}
