#define _USE_MATH_DEFINES

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <complex>
#include <cuda.h>
#include <cufft.h>
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/device_ptr.h>
#include <thrust/device_malloc.h>
#include <thrust/device_delete.h>
#include <thrust/device_free.h>
#include <thrust/extrema.h>

using namespace std;

extern int Nx, Ny, Nx2, Ny2;
extern float *SP_float;
extern cufftComplex *cuSP2;
extern float *SPWrapPhase2, *UnWrapPhaseSP2, *cuPhaseMap2, *cuPhaseMap;
extern uint8_t *tmp_uint8;

//Fast fourier transform
int FFT(int,int,double *,double *);
int FFT2D(complex<float> *, int, int, int);
void FFT3D(complex<float> *, int, int, int, int);
void FFT1Dshift(complex<float> *, int);
void FFT2Dshift(complex<float> *, int, int);
void FFT3Dshift(complex<float> *, int, int, int);

void FFT3Dshift_cufftComplex(cufftComplex *, int, int, int);
void cuFFT1D(cufftComplex *ImgArray, int size, int batch, int dir);
void cuFFT2D(cufftComplex *ImgArray, int sizeX, int sizeY, int dir);
void cuFFT2D_Batch(cufftComplex *ImgArray, int sizeX, int sizeY, int sizeZ, int dir);
void cuFFT2Dz(cufftDoubleComplex *ImgArray, int sizeX, int sizeY, int dir);
void cuFFT3D(cufftComplex *ImgArray, int sizeX, int sizeY, int sizeZ, int dir);
__global__ void cuFFT1Dshift(cufftComplex *input, int width);
__global__ void cuFFT2Dshift(cufftComplex *input, int width, int height);
__global__ void cuFFT3Dshift(cufftComplex *input, int width, int height, int slice);
__global__ void scaleFFT1D(cufftComplex *cu_F, int nx, double scale);
__global__ void scaleFFT2D(cufftComplex *cu_F, int nx, int ny, double);
__global__ void scaleFFT2Dz(cufftDoubleComplex *cu_F, int nx, int ny, double);
__global__ void scaleFFT2DReal(float *cu_F, int nx, int ny, double scale);
__global__ void scaleFFT3D(cufftComplex *cu_F, int nx, int ny, int nz, double);

void cuFFT_Real(cufftComplex *freq, float *img, const unsigned int Nx, const unsigned int Ny, int dir);

int DFT(int ,int ,double *,double *);
int Powerof2(int, int *, int *);
void bfilter(complex<float> *, int, int);



__global__ void bilinear_interpolation_kernel(float * __restrict__ d_result, const float * __restrict__ d_data,
	const int M1, const int N1, const int M2, const int N2);


const int TILE_DIM = 32;
const int BLOCK_ROWS = 8;
void extractQPI(float *SP, float *BG, cufftComplex *cuSP_FFT, cufftComplex *cuBG_FFT, int Nx, int Ny);
void sequence1DFFT(float *ResampleArray, cufftComplex *out_array, int Nx, int Ny);
__global__ void real2cufft(cufftComplex *odata, const float *idata);
__global__ void HistogramFT(float *sumFFT_1D, cufftComplex *device_FFT, int Nx, int Ny);
__global__ void CropFTdomain(cufftComplex *device_FFT, cufftComplex *device_crop, int Nx, int Ny, int center);
__global__ void shiftArray(cufftComplex *device_FFT, int Nx, int Ny);
int FindMaxIndex(float *sum, int size);
__global__ void copySharedMem(cufftComplex *odata, const cufftComplex *idata, const float scale);
__global__ void estimateWrapPhase(float *SPWrap, cufftComplex *SP, int sizeX, int sizeY);
__global__ void estimatePhase(float *Phase, float *UnSPWrap, int sizeX, int sizeY);
void FastExtraction(uint8_t *dst, uint8_t *src, int Nx, int Ny);

void DeviceMemOut(char *path, float *arr, int sizeX, int sizeY);
void DeviceMemOutFFT(char *path, cufftComplex *arr, int sizeX, int sizeY);
bool is_nan(double dVal);
bool is_inf(double dVal);