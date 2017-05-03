

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenCL/opencl.h>
#include <GLUT/glut.h>
#else                      
#include <CL/cl.h>
#endif

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "bitmap.h"


void initFrameBuffer();
void display();
void createTextures();
int initCLGL();
int runTheKernel();
int createCLimageFromGLtext();


int  fbo_width = 512;
int  fbo_height = 512;
double rtime = 0;
GLuint fb;

//GLuint depth;
static cl_context          clcontext;
static cl_device_id        deviceId;
static cl_command_queue    cmd_queue;
static cl_program          program;
static cl_kernel           kernel[2];
static cl_sampler          sampler;
int glwidth;
int glheight;

/** OpenCL memory data type to containg data generated by clCreateFromGLTexture2D(), which
   create a cl_mem object from a GL texture */
cl_mem src_image_mem;
cl_mem dst_image_mem;

/* Contains texture ID */
GLuint src_text;
GLuint dst_text;

/** Store the raw data from the image, bmp object returned by bitmap. */
char * src_image;
char * dst_image;

char * imgname = "/Users/camel/Desktop/Portfolio/GPU_PORTFOLIO/CLGL_FBO/Images/image3.BMP";
ME_ImageBMP bmp;


extern char meImageBMP_Init(ME_ImageBMP* bmp,char* fileName);

/* This kernel is just for testing purposes. The main goal is to share a 
 * Texture between OpenGL and OpenCL, so once we load the Texture with the 
 * OpenGL context we can manipulate it with OpenCL without the need to get 
 * it back to the host just to load again for rendering
 */

const char *program_source[] = {
   "                                                                                                       \n"
   "   __kernel void _kernel(read_only image2d_t srcimg, write_only image2d_t output, sampler_t smp)       \n"
   "   {                                                                                                   \n"
   "       int tidX = get_global_id(0), tidY = get_global_id(1);                                           \n"
   "       float4 color;                                                                                   \n"
   "                                                                                                       \n"
   "       color = read_imagef(srcimg, smp, (int2)(tidX, tidY));                                           \n"
   "       float av = 1.0;                                                                                  \n"
   "       av = (color.x + color.y + color.z)/3.0;                                                       \n"
   "       color.x = av;                                                                                  \n"
   "       color.y = av;                                                                                  \n"
   "       color.z = av;                                                                                   \n"
   "       write_imagef( output, (int2)( tidX, tidY ), color );                                            \n"
   "   }                                                                                                   \n"
   "                                                                                                       \n"
};

int main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );
    //glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);   //Open an OpenGL context with double buffering, RGB colors, and depth buffering
    glutInitWindowSize(1024, 1024);          //Set initial window size
	glutCreateWindow("OpenGL Window - Cristian Troncoso");
	glutDisplayFunc(display);
	glutIdleFunc(glutPostRedisplay);
   
    initCLGL();
    createTextures();
    createCLimageFromGLtext();
	initFrameBuffer();
   
	glutMainLoop();
   
	return 0;
}

void CHECK_FRAMEBUFFER_STATUS()
{
	GLenum status;
	status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	switch(status)
   {
      case GL_FRAMEBUFFER_COMPLETE:
         printf("good buffer\n");
         break;
         
      case GL_FRAMEBUFFER_UNSUPPORTED:
         /* choose different formats */
         break;
         
      default:
         /* programming error; will fail on all hardware */
         fputs("Framebuffer Error\n", stderr);
         exit(-1);
	}
}

/* \Brief   OpenGL objects and OpenCL memory objects created from OpenGL Objects can access
 * the same memory.To allow an OpenCL application to access an OpenGL object, use an OpenCL
 * "context" that is created from an OpenGL sharegroup (CGLGetShareGroup) object. By doing 
 * this OpenCL objects previusly created will reference these OpenGL objects.
 */
 int initCLGL()
{
   cl_int                               num_devices;
   size_t                               ret_size;
   cl_int                               err;
   
   /** To interoperate between OpenCL and OpenGl, first set the sharegroup */
   /** Get current CGL context and CGL Share groupt */
   CGLContextObj curCGLContext         = CGLGetCurrentContext();
   CGLShareGroupObj curCGLShareGroup   = CGLGetShareGroup(curCGLContext);
   
   /** On Apple platform a context with OpenGL interop is created by getting the CGL share
    * group associated to the wished OpenGL context, and passing that to clCreateContext 
    * via the property (in first parameter) CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE.
    */
   
   /** Create CL context properties, add handler and sharegroup enum */
   cl_context_properties properties[]  = {
                                             CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                                             (cl_context_properties)curCGLShareGroup,
                                             0
                                          };
   /** Create the context with device in the CGL sharegroup */
   clcontext     = clCreateContext(properties, 0,0,0,0,0);
   
   if(!clcontext)
   {
      printf("clCreateContext failed!\n");
      return 1;
   }
   
   /** Compute number of devices */
   err         = clGetContextInfo(clcontext, CL_CONTEXT_DEVICES, 0, NULL, &ret_size);
   if(!ret_size || err != CL_SUCCESS)
   {
      printf("clGetDeviceInfo failes!\n");
      return 1;
   }
   num_devices    = ret_size/sizeof(cl_device_id);
   
   /* Get the device list */
   cl_device_id   devices[num_devices];
   err            = clGetContextInfo(clcontext, CL_CONTEXT_DEVICES, ret_size, devices, &ret_size);
   if(err)
   {
      printf("clDeviceInfo failed!\n");
      return FALSE;
      
   }
   
   /** Get the device ID and queue it */
   for(int i = 0; i < num_devices; i++)
   {
      cl_int      device_type, error;
      err = clGetDeviceInfo(devices[i], CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, &ret_size);
      if(err)
      {
         printf("clGetDeviceInfo failed! \n");
         return FALSE;
      }
      if(device_type == CL_DEVICE_TYPE_GPU);
      {
         deviceId    = devices[i];
         cmd_queue   = clCreateCommandQueue(clcontext, deviceId, 0, &error);
         break;
      }
   }
   
   /** Create Program */
   program = clCreateProgramWithSource(clcontext, 1, (const char **) &program_source, NULL, &err);
   if (!program)
   {
      printf("Error: Failed to create compute program!\n");
      return -1;
   }
   err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
   if (err != CL_SUCCESS)
   {
      printf("Error: Failed to build compute program!\n");
      return -1;
   }
   kernel[0] = clCreateKernel(program, "_kernel", &err);
   if (!kernel[0] || err != CL_SUCCESS)
   {
      printf("Error: Failed to create kernel!\n");
      return -1;
   }
   
   return 0;
}

int runTheKernel()
{

   cl_int err;
   err = clSetKernelArg(kernel[0], 0, sizeof(src_image_mem), &src_image_mem);
   if( err)
   {
      printf("clSetKernelArgs failed\n");
   }
   err = clSetKernelArg(kernel[0], 1, sizeof(dst_image_mem), &dst_image_mem);
   if( err)
   {
      printf("clSetKernelArgs failed\n");
   }
   err |= clSetKernelArg(kernel[0],2, sizeof(sampler), &sampler);
   if( err)
   {
      printf("clSetKernelArgs failed\n");
   }
   
   err = clEnqueueAcquireGLObjects(cmd_queue, 1, &src_image_mem, 0, NULL, NULL);
   err|= clEnqueueAcquireGLObjects(cmd_queue, 1, &dst_image_mem, 0, NULL, NULL);
   if(err)
   {
      printf("clEnqueueAcquireGLObjects failed\n");
   }
   
   // execure Kernel
   size_t global[2] = { fbo_width, fbo_height };
   size_t local[2] = { 16,16 };
   
   err = clEnqueueNDRangeKernel(cmd_queue, kernel[0], 2, NULL, global, local, 0, NULL, NULL);
   if(err != CL_SUCCESS)
   {
      printf("clEnqueueNDRangeKernel failed\n");
   }
   
   err = clEnqueueReleaseGLObjects(cmd_queue, 1, &src_image_mem, 0, NULL, NULL);
   err|= clEnqueueReleaseGLObjects(cmd_queue, 1, &dst_image_mem, 0, NULL, NULL);
   if(err)
   {
      printf("clEnequeueReleaseGLObjects failed\n");
   }
   clFlush(cmd_queue);

   return 0;
}

/** Brief	It maps OpenGL textures to OpenCL memory objects (cl_mem). The source texture 
 * containing the original bipmap is mapped to cl_mem in, and cl_mem out is mapped to the 
 * destination texture, which will be finally rendered onto the Quad geometry.
 */
int createCLimageFromGLtext()
{
   cl_int err;
   // Create a OpenCL image from GL texure
   src_image_mem = clCreateFromGLTexture2D(clcontext, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, src_text, &err);
   
   if(!src_image_mem || err)
   {
      printf("clCreateFromGLTexture2D failed\n");
      return 1;
   }
   
   dst_image_mem = clCreateFromGLTexture2D(clcontext, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, dst_text, &err);
   if(!dst_image_mem || err)
   {
      printf("clCreateFromGLTexture2D failed\n");
      return 1;
   }
   
   sampler = clCreateSampler(clcontext, CL_FALSE, CL_ADDRESS_CLAMP_TO_EDGE, CL_FILTER_NEAREST, &err);
   if(!sampler || err)
   {
      printf("clCreateSampler failed\n");
      return 1;
   }
   return 0;
}

/** \Brief  Create the textures in OpenGL and allocate space
 * I create two textures, the first is the the texture binded to the original bitmap.
 * The second texture would contain the output from the proccessing kernel.
 */
void createTextures()
{
   /**< Get raw data and size from BMP files */
   meImageBMP_Init(&bmp,imgname);
   src_image = (char*)bmp.imgData;
   uint32_t imgSize = bmp.imgWidth*bmp.imgHeight*4;
   fbo_height = bmp.imgWidth;
   fbo_width  = bmp.imgHeight;
   
   // Create ID for texture
   glGenTextures(1, &src_text);
   // Set this texture to be the one we are working with
   glBindTexture(GL_TEXTURE_2D, src_text);
   /** Use bilinear interpolation */
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   // Generate the texture
   glTexImage2D(	GL_TEXTURE_2D,0,GL_RGBA,fbo_width, fbo_height,0,GL_RGB,GL_UNSIGNED_BYTE,src_image);
   free(src_image);
   glBindTexture(GL_TEXTURE_2D, 0);
   
   
   glGenTextures(1, &dst_text);
   glBindTexture(GL_TEXTURE_2D, dst_text);
   /** Use bilinear interpolation */
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(	GL_TEXTURE_2D,0,GL_RGBA,fbo_width, fbo_height,0,GL_RGB,GL_UNSIGNED_BYTE,dst_image);
   free(dst_image);
   glBindTexture(GL_TEXTURE_2D, 0);
   
   glFlush();
   
}

/* \Brief Create a buffer object in OpenGL and allocate space
 */
void initFrameBuffer()
{
   
   
	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_text, 0);
   
   //Don't delete the depth buffer just yet, some research is needed about its use.
   
	//glGenRenderbuffers(1, &depth);
   //glBindRenderbuffer(GL_RENDERBUFFER, depth);
	
   //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, fbo_width, fbo_height);
	//glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
   
	CHECK_FRAMEBUFFER_STATUS();
}


void render()
{
    runTheKernel();
    const int win_width  = glutGet(GLUT_WINDOW_WIDTH);
	const int win_height = glutGet(GLUT_WINDOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
    //glDisable(GL_LIGHTING);
	//glClearColor(1.,1.,1.,0.);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    
    glViewport(0,0, win_width, win_height);
	glMatrixMode(GL_PROJECTION);
	
    
    glLoadIdentity();
    
    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	
    glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, dst_text);
   
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
   
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   
    
	/* This shows the back
	float cube[][3]      =  {
                              { 1,-1},
                              {-1,-1},
                              {-1, 1},
                              { 1, 1}
                           };
    */
    
    // shows the front
    float cube[][/*3 for 3D*/2]      =  {
                            { -1,-1},
                            {1,-1},
                            {1, 1},
                            { -1, 1}
    };
    
    
   float textcords[][2] =  {
                              {0,0},
                              {1,0},
                              {1,1},
                              {0,1}
                           };
   
   unsigned int faces[]= { 1,0,3,2 };
   
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   
	glVertexPointer(/*3 for 3D*/2, GL_FLOAT, /*3 for 3D*/2*sizeof(float), &cube[0][0]);
	glTexCoordPointer(2, GL_FLOAT, 2*sizeof(float), &textcords[0][0]);
   
	//glCullFace(GL_BACK);
	//glDrawElements(GL_QUADS, 4, GL_UNSIGNED_INT, faces);
   
	glCullFace(GL_FRONT);
	glDrawElements(GL_QUADS, 4, GL_UNSIGNED_INT, faces);
   
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   
   glDisable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D,0);
   glFlush();
   
}

void display()
{
	render();
	glutSwapBuffers();
}

