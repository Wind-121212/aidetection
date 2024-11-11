#include <SDHCI.h>
#include <NetPBM.h>
#include <DNNRT.h>

#include <stdio.h>  

#include <Camera.h>
#define BAUDRATE                (115200)
#define TOTAL_PICTURE_COUNT     (10)
SDClass SD; 
DNNRT dnnrt; 

typedef struct   class_result {
  uint32_t size = 0;
  float * data = NULL;
} class_result_t;


bool ErrEnd = false;
int take_picture_count = 0;
int index_prob;


void printError(enum CamErr err)
{  
  Serial.print("Error: ");
  switch (err)
    {
      case CAM_ERR_NO_DEVICE:
        Serial.println("No Device");
        break;
      case CAM_ERR_ILLEGAL_DEVERR:
        Serial.println("Illegal device error");
        break;
      case CAM_ERR_ALREADY_INITIALIZED:
        Serial.println("Already initialized");
        break;
      case CAM_ERR_NOT_INITIALIZED:
        Serial.println("Not initialized");
        break;
      case CAM_ERR_NOT_STILL_INITIALIZED:
        Serial.println("Still picture not initialized");
        break;
      case CAM_ERR_CANT_CREATE_THREAD:
        Serial.println("Failed to create thread");
        break;
      case CAM_ERR_INVALID_PARAM:
        Serial.println("Invalid parameter");
        break;
      case CAM_ERR_NO_MEMORY:
        Serial.println("No memory");
        break;
      case CAM_ERR_USR_INUSED:
        Serial.println("Buffer already in use");
        break;
      case CAM_ERR_NOT_PERMITTED:
        Serial.println("Operation not permitted");
        break;
      default:
        break;
    }
}

/**
 * Callback from Camera library when video frame is captured.
 */

void CamCB(CamImage img)
{

  /* Check the img instance is available or not. */

  if (img.isAvailable())
    {

      /* If you want RGB565 data, convert image data format to RGB565 */

      img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
      /* You can use image data directly by using getImgSize() and getImgBuff().
       * for displaying image to a display, etc. */

      Serial.print("Image data size = ");
      Serial.print(img.getImgSize(), DEC);
      Serial.print(" , ");

      Serial.print("buff addr = ");
      Serial.print((unsigned long)img.getImgBuff(), HEX);
      Serial.println("");
    }
  else
    {
      Serial.println("Failed to get video stream image");
    }
}

/**
 * @brief Initialize camera
 */
void setup()
{
  CamErr err;

  /* Open serial communications and wait for port to open */

  Serial.begin(BAUDRATE);
  
  while (!Serial)
    {
      ; /* wait for serial port to connect. Needed for native USB port only */
    }

  /* Initialize SD */
  while (!SD.begin()) 
    {
      /* wait until SD card is mounted. */
      Serial.println("Insert SD card.");
    }

  /* begin() without parameters means that
   * number of buffers = 1, 30FPS, QVGA, YUV 4:2:2 format */

  Serial.println("Prepare camera");
  err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }

  /* Start video stream.
   * If received video stream data from camera device,
   *  camera library call CamCB.
   */

  Serial.println("Start streaming");
  err = theCamera.startStreaming(true, CamCB);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }

  /* Auto white balance configuration */

  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_DAYLIGHT);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
 
  /* Set parameters about still picture.
   * In the following case, QUADVGA and JPEG.
   */

  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(160,120,CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }

}



void loop()
{
  sleep(0.5); /* wait for one second to take still picture. */
  /* You can change the format of still picture at here also, if you want. */

  /* theCamera.setStillPictureImageFormat(
   *   CAM_IMGSIZE_HD_H,
   *   CAM_IMGSIZE_HD_V,
   *   CAM_IMAGE_PIX_FMT_JPG);
   */

  /* This sample code can take pictures in every one second from starting. */

  if (take_picture_count < TOTAL_PICTURE_COUNT)
    {

      /* Take still picture.
      * Unlike video stream(startStreaming) , this API wait to receive image data
      *  from camera device.
      */
  
      Serial.println("call takePicture()");
      CamImage img = theCamera.takePicture();

      if (img.isAvailable())
        {
          /* Create file name */
    
          char filename[16] = {0};
          sprintf(filename, "PICT%03d.JPG", take_picture_count);
          Serial.print("Save taken picture as ");
          Serial.print(filename);
          Serial.println("");
          //img.resizeImageByHW(CamImage&, 28, 28);
          /* Remove the old file with the same file name as new created file,
           * and create new file.
           */

          SD.remove(filename);
          File myFile = SD.open(filename, FILE_WRITE);
          myFile.write(img.getImgBuff(), img.getImgSize());
          myFile.close();
        }
      else
        {
          /* The size of a picture may exceed the allocated memory size.
           * Then, allocate the larger memory size and/or decrease the size of a picture.
           * [How to allocate the larger memory]
           * - Decrease jpgbufsize_divisor specified by setStillPictureImageFormat()
           * - Increase the Memory size from Arduino IDE tools Menu
           * [How to decrease the size of a picture]
           * - Decrease the JPEG quality by setJPEGQuality()
           */

          Serial.println("Failed to take picture");
        }
    }
  else if (take_picture_count == TOTAL_PICTURE_COUNT)
    {
      Serial.println("End.");
      theCamera.end();

      File nnbfile = SD.open("model.nnb");
  if (!nnbfile) {
    Serial.print("nnb not found");
    return;
  }

  int ret = dnnrt.begin(nnbfile);
  if (ret < 0) {
    Serial.println("Runtime initialization failure.");
    return;
  }
  File pgmfile("PICT008.JPG");//→2

  NetPBM pgm(pgmfile);

  unsigned short width, height;
  pgm.size(&width, &height);

  DNNVariable input(width * height);
  float *buf = input.data();
  int i = 0;

  /* Normalize data into between 0.0 and 1.0.
   *  PGM file is gray scale pixel map, so devide by 255.
   *  This normalization depends on the network model.
   */
  
  for (int x = 0; x < height; x++){
    for (int y = 0; y < width; y++){
      buf[i] = float(pgm.getpixel(x,y))/255.0f;
      i++;
    }
  }

  dnnrt.inputVariable(input, 0);
  dnnrt.forward();
  DNNVariable output = dnnrt.outputVariable(0);

  index_prob = output.maxIndex();

  
  float value = output[index_prob];


  
  Serial.print("water level is ");
  switch (index_prob){
    case 0:
      Serial.print("safe");
      break;
    case 1:
      Serial.print("safe, but a little high");
      break;
    case 2:
      Serial.print("high ! start to get the position data and disaster info !");
  }
  //Serial.print(index_prob);
  Serial.println();
  Serial.print("probability is ");
  Serial.print(output[index_prob]);
  Serial.println();

  dnnrt.end();

    }

  take_picture_count++;
}
