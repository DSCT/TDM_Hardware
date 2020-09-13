#define _USE_MATH_DEFINES
#include <omp.h>
#include "PhaseExtraction.cuh"
#include "DCT_Unwrapping.cuh"
using namespace std;

/*-------------------------------------------------------------------------*/
/* This computes an in-place complex-to-complex FFT                        */
/* x and y are the real and imaginary arrays of 2^m points.                */
/* dir =  1 gives forward transform                                        */
/* dir = -1 gives reverse transform                                        */
/*                                                                         */
/*  Formula: forward                                                       */
/*                N-1                                                      */
/*                ---                                                      */
/*            1   \          - j k 2 pi n / N                              */
/*    X(n) = ---   >   x(k) e                    = forward transform       */
/*            N   /                                n=0..N-1                */
/*                ---                                                      */
/*                k=0                                                      */
/*                                                                         */
/*    Formula: reverse                                                     */
/*                N-1                                                      */
/*                ---                                                      */
/*                \          j k 2 pi n / N                                */
/*    X(n) =       >   x(k) e                    = forward transform       */
/*                /                                n=0..N-1                */
/*                ---                                                      */
/*                k=0                                                      */
/*-------------------------------------------------------------------------*/
int FFT(int dir, int m, double *x, double *y)
{
	long nn, i, i1, j, k, i2, l, l1, l2;
	double c1, c2, tx, ty, t1, t2, u1, u2, z;

	/* Calculate the number of points */
	nn = 1;
	for (i = 0; i<m; i++)
		nn *= 2;
	/* Do the bit reversal */
	i2 = nn >> 1;
	j = 0;
	for (i = 0; i<nn - 1; i++)
	{
		if (i < j)
		{
			tx = x[i];
			ty = y[i];
			x[i] = x[j];
			y[i] = y[j];
			x[j] = tx;
			y[j] = ty;
		}
		k = i2;
		while (k <= j)
		{
			j -= k;
			k >>= 1;
		}
		j += k;
	}

	/* Compute the FFT */
	c1 = -1.0;
	c2 = 0.0;
	l2 = 1;
	for (l = 0; l<m; l++)
	{
		l1 = l2;
		l2 <<= 1;
		u1 = 1.0;
		u2 = 0.0;
		for (j = 0; j<l1; j++)
		{
			for (i = j; i<nn; i += l2)
			{
				i1 = i + l1;
				t1 = u1 * x[i1] - u2 * y[i1];
				t2 = u1 * y[i1] + u2 * x[i1];
				x[i1] = x[i] - t1;
				y[i1] = y[i] - t2;
				x[i] += t1;
				y[i] += t2;
			}
			z = u1 * c1 - u2 * c2;
			u2 = u1 * c2 + u2 * c1;
			u1 = z;
		}
		c2 = sqrt((1.0 - c1) / 2.0);
		if (dir == 1)
			c2 = -c2;
		c1 = sqrt((1.0 + c1) / 2.0);
	}

	/* Scaling for forward transform */
	if (dir == 1)
	{
		for (i = 0; i<nn; i++)
		{
			x[i] /= (double)nn;
			y[i] /= (double)nn;
		}
	}
	return(true);
}
/*-------------------------------------------------------------------------*/
int DFT(int dir, int m, double *x1, double *y1)
{
	long i, k;
	double arg;
	double cosarg, sinarg;

	double *x2 = (double *)malloc(m*sizeof(double));
	double *y2 = (double *)malloc(m*sizeof(double));

	if (x2 == NULL || y2 == NULL)
		return(false);

	for (i = 0; i<m; i++) {
		x2[i] = 0;
		y2[i] = 0;
		arg = -dir * 2.0 * M_PI * (double)i / (double)m;
		for (k = 0; k<m; k++) {
			cosarg = cos(k * arg);
			sinarg = sin(k * arg);
			x2[i] += (x1[k] * cosarg - y1[k] * sinarg);
			y2[i] += (x1[k] * sinarg + y1[k] * cosarg);
		}
	}

	/* Copy the data back */
	if (dir == 1) {
		for (i = 0; i<m; i++) {
			x1[i] = x2[i] / (double)m;
			y1[i] = y2[i] / (double)m;
		}
	}
	else {
		for (i = 0; i<m; i++) {
			x1[i] = x2[i];
			y1[i] = y2[i];
		}
	}

	free(x2);
	free(y2);
	return(true);
}
/*-------------------------------------------------------------------------*/
/* Butterworth filter                                                      */
/*-------------------------------------------------------------------------*/
void bfilter(complex<float> *filter, int width, int height)
{
	for (int v = 0; v<height; v++)
	for (int u = 0; u<width; u++)
	{
		int pos = u + v*width;
		float temp_v = (u - width / 2)*(u - width / 2) + (v - height / 2)*(v - height / 2);
		double distance = sqrt(temp_v);
		double H = 1 / (1 + pow(distance / 2.0, 0.1));
		filter[u + v*width] = complex<float>(real(filter[u + v*width])*H, imag(filter[u + v*width])*H);
	}
}
/*-------------------------------------------------------------------------*/
void FFT1Dshift(complex<float> *input, int length)
{
	complex<float> tmp;

	for (int i = 0; i < length / 2; i++)
	{
		tmp = input[i];
		input[i] = input[i + length / 2];
		input[i + length / 2] = tmp;
	}
}
/*-------------------------------------------------------------------------*/
__global__ void cuFFT1Dshift(cufftComplex *input, int width)
{
	cufftComplex tmp;

	unsigned int i = blockDim.x * blockIdx.x + threadIdx.x;

	if (i<width / 2)
	{
		// interchange entries in 4 quadrants, 1 <--> 3 and 2 <--> 4
		tmp = input[i];
		input[i] = input[i + width / 2];
		input[i + width / 2] = tmp;
	}
}
/*-------------------------------------------------------------------------*/
/*Ref.: goo.gl/DR9Pqs*/
void FFT2Dshift(complex<float> *input, int width, int height)
{
	complex<float> tmp13, tmp24;

	// interchange entries in 4 quadrants, 1 <--> 3 and 2 <--> 4
	for (int k = 0; k < height / 2; k++)
	for (int i = 0; i < width / 2; i++)
	{
		tmp13 = input[i + k*width];
		input[i + k*width] = input[(i + width / 2) + (k + height / 2)*width];
		input[(i + width / 2) + (k + height / 2)*width] = tmp13;
		tmp24 = input[(i + width / 2) + k*width];
		input[(i + width / 2) + k*width] = input[i + (k + height / 2)*width];
		input[i + (k + height / 2)*width] = tmp24;
	}
}
/*-------------------------------------------------------------------------*/
__global__ void cuFFT2Dshift(cufftComplex *input, int width, int height)
{
	cufftComplex tmp13, tmp24;

	unsigned int i = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int j = blockDim.y * blockIdx.y + threadIdx.y;

	if (i<width / 2 && j<height / 2)
	{
		// interchange entries in 4 quadrants, 1 <--> 3 and 2 <--> 4
		tmp13 = input[i + j*width];
		input[i + j*width] = input[(i + width / 2) + (j + height / 2)*width];
		input[(i + width / 2) + (j + height / 2)*width] = tmp13;
		tmp24 = input[(i + width / 2) + j*width];
		input[(i + width / 2) + j*width] = input[i + (j + height / 2)*width];
		input[i + (j + height / 2)*width] = tmp24;
	}
}
/*-------------------------------------------------------------------------*/
/*Ref.: goo.gl/3ZEKgN*/
void FFT3Dshift(complex<float> *input, int width, int height, int slice)
{
	complex<float> tmp1, tmp2, tmp3, tmp4;

	for (int k = 0; k < slice / 2; k++)
	for (int j = 0; j < height / 2; j++)
	for (int i = 0; i < width / 2; i++)
	{
		tmp1 = input[i + j*width + k*width*height];
		input[i + j*width + k*width*height] = input[(width / 2 + i) + (height / 2 + j)*width + (slice / 2 + k)*width*height];
		input[(width / 2 + i) + (height / 2 + j)*width + (slice / 2 + k)*width*height] = tmp1;

		tmp2 = input[i + (height / 2 + j)*width + k*width*height];
		input[i + (height / 2 + j)*width + k*width*height] = input[(width / 2 + i) + j*width + (slice / 2 + k)*width*height];
		input[(width / 2 + i) + j*width + (slice / 2 + k)*width*height] = tmp2;

		tmp3 = input[(width / 2 + i) + j*width + k*width*height];
		input[(width / 2 + i) + j*width + k*width*height] = input[i + (height / 2 + j)*width + (slice / 2 + k)*width*height];
		input[i + (height / 2 + j)*width + (slice / 2 + k)*width*height] = tmp3;

		tmp4 = input[(width / 2 + i) + (height / 2 + j)*width + k*width*height];
		input[(width / 2 + i) + (height / 2 + j)*width + k*width*height] = input[i + j*width + (slice / 2 + k)*width*height];
		input[i + j*width + (slice / 2 + k)*width*height] = tmp4;
	}
}
/*-------------------------------------------------------------------------*/
void FFT3Dshift_cufftComplex(cufftComplex *input, int width, int height, int slice)
{
	cufftComplex tmp1, tmp2, tmp3, tmp4;

	for (int k = 0; k < slice / 2; k++)
	for (int j = 0; j < height / 2; j++)
	for (int i = 0; i < width / 2; i++)
	{
		tmp1 = input[i + j*width + k*width*height];
		input[i + j*width + k*width*height] = input[(width / 2 + i) + (height / 2 + j)*width + (slice / 2 + k)*width*height];
		input[(width / 2 + i) + (height / 2 + j)*width + (slice / 2 + k)*width*height] = tmp1;

		tmp2 = input[i + (height / 2 + j)*width + k*width*height];
		input[i + (height / 2 + j)*width + k*width*height] = input[(width / 2 + i) + j*width + (slice / 2 + k)*width*height];
		input[(width / 2 + i) + j*width + (slice / 2 + k)*width*height] = tmp2;

		tmp3 = input[(width / 2 + i) + j*width + k*width*height];
		input[(width / 2 + i) + j*width + k*width*height] = input[i + (height / 2 + j)*width + (slice / 2 + k)*width*height];
		input[i + (height / 2 + j)*width + (slice / 2 + k)*width*height] = tmp3;

		tmp4 = input[(width / 2 + i) + (height / 2 + j)*width + k*width*height];
		input[(width / 2 + i) + (height / 2 + j)*width + k*width*height] = input[i + j*width + (slice / 2 + k)*width*height];
		input[i + j*width + (slice / 2 + k)*width*height] = tmp4;
	}
}
/*-------------------------------------------------------------------------*/
__global__ void cuFFT3Dshift(cufftComplex *input, int width, int height, int slice)
{
	cufftComplex tmp1, tmp2, tmp3, tmp4;

	unsigned int i = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int j = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int k = blockDim.z * blockIdx.z + threadIdx.z;

	if ((i<width / 2) && (j<height / 2) && (k<slice / 2))
	{
		tmp1 = input[i + j*width + k*width*height];
		input[i + j*width + k*width*height] = input[(width / 2 + i) + (height / 2 + j)*width + (slice / 2 + k)*width*height];
		input[(width / 2 + i) + (height / 2 + j)*width + (slice / 2 + k)*width*height] = tmp1;

		tmp2 = input[i + (height / 2 + j)*width + k*width*height];
		input[i + (height / 2 + j)*width + k*width*height] = input[(width / 2 + i) + j*width + (slice / 2 + k)*width*height];
		input[(width / 2 + i) + j*width + (slice / 2 + k)*width*height] = tmp2;

		tmp3 = input[(width / 2 + i) + j*width + k*width*height];
		input[(width / 2 + i) + j*width + k*width*height] = input[i + (height / 2 + j)*width + (slice / 2 + k)*width*height];
		input[i + (height / 2 + j)*width + (slice / 2 + k)*width*height] = tmp3;

		tmp4 = input[(width / 2 + i) + (height / 2 + j)*width + k*width*height];
		input[(width / 2 + i) + (height / 2 + j)*width + k*width*height] = input[i + j*width + (slice / 2 + k)*width*height];
		input[i + j*width + (slice / 2 + k)*width*height] = tmp4;
	}
}
/*-------------------------------------------------------------------------*/
int Powerof2(int n, int *m, int *twopm)
{
	if (n <= 1)
	{
		*m = 0;
		*twopm = 1;
		return(false);
	}

	*m = 1;
	*twopm = 2;
	do{
		(*m)++;
		(*twopm) *= 2;
	} while (2 * (*twopm) <= n);

	if (*twopm != n)
		return(false);
	else
		return(true);
}
/*-------------------------------------------------------------------------*/
/* Perform a 2D FFT inplace given a complex 2D array                       */
/* The direction dir, 1 for forward, -1 for reverse                        */
/* The size of the array (nx,ny)                                           */
/* Return false if there are memory problems or                            */
/*    the dimensions are not powers of 2                                   */
/*-------------------------------------------------------------------------*/
int FFT2D(complex<float> *c, int nx, int ny, int dir)
{
	int m, twopm;
	double *realC, *imagC;

	/* Transform the rows */
	realC = (double *)malloc(nx * sizeof(double));
	imagC = (double *)malloc(nx * sizeof(double));
	if (realC == NULL || imagC == NULL)
		return(false);
	if (!Powerof2(nx, &m, &twopm) || twopm != nx)
		return(false);
	for (int j = 0; j<ny; j++)
	{
		for (int i = 0; i<nx; i++)
		{
			realC[i] = (double)real(c[i*ny + j]);
			imagC[i] = (double)imag(c[i*ny + j]);
		}

		FFT(dir, m, realC, imagC);

		for (int i = 0; i<nx; i++)
		{
			c[i*ny + j] = complex<float>((float)realC[i], (float)imagC[i]);
		}
	}

	/* Transform the columns */
	realC = (double *)realloc(realC, nx * sizeof(double));
	imagC = (double *)realloc(imagC, nx * sizeof(double));
	if (realC == NULL || imagC == NULL)
		return(false);
	if (!Powerof2(ny, &m, &twopm) || twopm != ny)
		return(false);
	for (int i = 0; i<nx; i++)
	{
		for (int j = 0; j<ny; j++)
		{
			realC[j] = (double)real(c[i*ny + j]);
			imagC[j] = (double)imag(c[i*ny + j]);
		}

		FFT(dir, m, realC, imagC);

		for (int j = 0; j<ny; j++)
		{
			c[i*ny + j] = complex<float>((float)realC[j], (float)imagC[j]);
		}
	}
	free(realC);
	free(imagC);

	return(true);
}
/*-------------------------------------------------------------------------*/
void FFT3D(complex<float> *c, int nx, int ny, int nz, int dir)
{
#pragma omp parallel for
	for (int z = 0; z<nz; z++)
	{
		complex <float> *temp_f = (complex<float> *)malloc(nx*ny*sizeof(complex<float>));

		for (int i = 0; i < nx*ny; i++)
		{
			temp_f[i] = c[i + z*nx*ny];
		}

		FFT2D(temp_f, nx, ny, dir);
		for (int i = 0; i < nx*ny; i++)
		{
			c[i + z*nx*ny] = temp_f[i];
		}
		free(temp_f);
	}
#pragma omp barrier

	//int m,twopm;	
	//double *realC, *imagC;

	// Transform the rows

	//Transform the slices
#pragma omp parallel for
	for (int i = 0; i<nx*ny; i++)
	{
		double *realC = (double *)malloc(nz * sizeof(double));
		double *imagC = (double *)malloc(nz * sizeof(double));

		int m, twopm;
		Powerof2(nz, &m, &twopm);

		for (int k = 0; k<nz; k++)
		{
			realC[k] = (double)real(c[k*nx*ny + i]);
			imagC[k] = (double)imag(c[k*nx*ny + i]);
		}

		FFT(dir, m, realC, imagC);

		for (int k = 0; k<nz; k++)
		{
			c[k*nx*ny + i] = complex<float>((float)realC[k], (float)imagC[k]);
		}
		free(realC);
		free(imagC);
	}
#pragma omp barrier

}


void cuFFT1D(cufftComplex *ImgArray, int size, int batch, int dir)
{
	//Create a 1D FFT plan. 
	cufftHandle plan;
	cufftPlan1d(&plan, size, CUFFT_C2C, batch);

	if (dir == -1)
	{
		// Use the CUFFT plan to transform the signal out of place. 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_FORWARD);

		//		cudaThreadSynchronize();
	}
	else if (dir == 1)
	{
		// Note: idata != odata indicates an out-of-place transformation to CUFFT at execution time. 
		//Inverse transform the signal in place 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_INVERSE);

		cudaThreadSynchronize();

		int grid = (size + 1024 - 1) / 1024;
		int block = 32 * 32;
		scaleFFT1D << <grid, block >> >(ImgArray, size, 1.f / size);
	}
	else if (dir == 2)
	{
		// Note: idata != odata indicates an out-of-place transformation to CUFFT at execution time. 
		//Inverse transform the signal in place 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_INVERSE);

		cudaThreadSynchronize();
	}

	// Destroy the CUFFT plan.
	cufftDestroy(plan);
}
void cuFFT2D(cufftComplex *ImgArray, int sizeX, int sizeY, int dir)
{
	//Create a 2D FFT plan. 
	cufftHandle plan;
	cufftPlan2d(&plan, sizeX, sizeY, CUFFT_C2C);
	/*const int NRANK = 2;
	const int BATCH = 10;

	int n [NRANK] = {sizeX, sizeY} ;
	cufftPlanMany(&plan , 2 , n ,
	NULL , 1 , sizeX*sizeY ,
	NULL , 1 , sizeX*sizeY ,
	CUFFT_C2C , BATCH );*/


	if (dir == -1)
	{
		// Use the CUFFT plan to transform the signal out of place. 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_FORWARD);

		cudaThreadSynchronize();
	}
	else if (dir == 1)
	{
		// Note: idata != odata indicates an out-of-place transformation to CUFFT at execution time. 
		//Inverse transform the signal in place 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_INVERSE);

		cudaThreadSynchronize();

		int blocksInX = (sizeX + 32 - 1) / 32;
		int blocksInY = (sizeY + 32 - 1) / 32;
		dim3 grid(blocksInX, blocksInY);
		dim3 block(32, 32);
		scaleFFT2D << <grid, block >> >(ImgArray, sizeX, sizeY, 1.f / (sizeX*sizeY));
	}

	// Destroy the CUFFT plan.
	cufftDestroy(plan);
}

void cuFFT2D_Batch(cufftComplex *ImgArray, int sizeX, int sizeY, int sizeZ, int dir)
{
	//Create a 2D FFT plan. 
	cufftHandle plan;
	const int NRANK = 2;
	const int BATCH = sizeZ;

	int n[NRANK] = { sizeX, sizeY };
	cufftPlanMany(&plan, 2, n,
		NULL, 1, sizeX*sizeY,
		NULL, 1, sizeX*sizeY,
		CUFFT_C2C, BATCH);


	if (dir == -1)
	{
		// Use the CUFFT plan to transform the signal out of place. 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_FORWARD);

		cudaThreadSynchronize();
	}
	else if (dir == 1)
	{
		// Note: idata != odata indicates an out-of-place transformation to CUFFT at execution time. 
		//Inverse transform the signal in place 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_INVERSE);

		cudaThreadSynchronize();

		int blocksInX = (sizeX + 32 - 1) / 32;
		int blocksInY = (sizeY + 32 - 1) / 32;
		dim3 grid(blocksInX, blocksInY);
		dim3 block(32, 32);
		//scaleFFT2D << <grid, block >> >(ImgArray, sizeX, sizeY, 1.f / (sizeX*sizeY));
	}

	// Destroy the CUFFT plan.
	cufftDestroy(plan);
}

void cuFFT2Dz(cufftDoubleComplex *ImgArray, int sizeX, int sizeY, int dir)
{
	//Create a 2D FFT plan. 
	cufftHandle plan;
	cufftPlan2d(&plan, sizeX, sizeY, CUFFT_Z2Z);	//cufftSafeCall(cufftPlan2d(&plan, sizeX, sizeY, CUFFT_C2C));


	if (dir == -1)
	{
		// Use the CUFFT plan to transform the signal out of place. 
		cufftExecZ2Z(plan, (cufftDoubleComplex *)ImgArray, (cufftDoubleComplex *)ImgArray, CUFFT_FORWARD);
	}
	else if (dir == 1)
	{
		// Note: idata != odata indicates an out-of-place transformation to CUFFT at execution time. 
		//Inverse transform the signal in place 
		cufftExecZ2Z(plan, (cufftDoubleComplex *)ImgArray, (cufftDoubleComplex *)ImgArray, CUFFT_INVERSE);

		int blocksInX = (sizeX + 32 - 1) / 32;
		int blocksInY = (sizeY + 32 - 1) / 32;
		dim3 grid(blocksInX, blocksInY);
		dim3 block(32, 32);
		scaleFFT2Dz << <grid, block >> >(ImgArray, sizeX, sizeY, 1.f / (sizeX*sizeY));
	}

	// Destroy the CUFFT plan.
	cufftDestroy(plan);
}

void cuFFT3D(cufftComplex *ImgArray, int sizeX, int sizeY, int sizeZ, int dir)
{
	//Create a 3D FFT plan. 
	cufftHandle plan;
	cufftPlan3d(&plan, sizeX, sizeY, sizeZ, CUFFT_C2C);
	//int batch = 10;
	//int dims[] = {sizeZ, sizeY, sizeX}; // reversed order
	//cufftPlanMany(&plan, 3, dims, NULL, 1, 0, NULL, 1, 0, CUFFT_C2C, batch);

	if (dir == -1)
	{
		// Use the CUFFT plan to transform the signal out of place. 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_FORWARD);
	}
	else if (dir == 1)
	{
		// Note: idata != odata indicates an out-of-place transformation to CUFFT at execution time. 
		//Inverse transform the signal in place 
		cufftExecC2C(plan, (cufftComplex *)ImgArray, (cufftComplex *)ImgArray, CUFFT_INVERSE);

		int blocksInX = (sizeX + 8 - 1) / 8;
		int blocksInY = (sizeY + 8 - 1) / 8;
		int blocksInZ = (sizeZ + 8 - 1) / 8;
		dim3 grid(blocksInX, blocksInY, blocksInZ);
		dim3 block(8, 8, 8);

		scaleFFT3D << <grid, block >> >(ImgArray, sizeX, sizeY, sizeZ, 1.f / (sizeX*sizeY*sizeZ));
	}

	// Destroy the CUFFT plan.
	cufftDestroy(plan);
}

__global__ void scaleFFT1D(cufftComplex *cu_F, int nx, double scale)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;

	if (xIndex<nx)
	{
		double tempX = cu_F[xIndex].x * scale;
		double tempY = cu_F[xIndex].y * scale;
		cu_F[xIndex].x = (float)tempX;
		cu_F[xIndex].y = (float)tempY*(-1);
	}
}

__global__ void scaleFFT2D(cufftComplex *cu_F, int nx, int ny, double scale)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;

	if ((xIndex<nx) && (yIndex<ny))
	{
		unsigned int index_out = xIndex + nx*yIndex;
		double tempX = cu_F[index_out].x * scale;
		double tempY = cu_F[index_out].y * scale;
		cu_F[index_out].x = (float)tempX;
		cu_F[index_out].y = (float)tempY*(-1);
	}
}

__global__ void scaleFFT2Dz(cufftDoubleComplex *cu_F, int nx, int ny, double scale)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;

	if ((xIndex<nx) && (yIndex<ny))
	{
		unsigned int index_out = xIndex + nx*yIndex;
		double tempX = cu_F[index_out].x * scale;
		double tempY = cu_F[index_out].y * scale;
		cu_F[index_out].x = (double)tempX;
		cu_F[index_out].y = (double)tempY*(-1);
	}
}


__global__ void scaleFFT2DReal(float *cu_F, int nx, int ny, double scale)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;

	if ((xIndex<nx) && (yIndex<ny))
	{
		unsigned int index_out = xIndex + nx*yIndex;
		double tempX = cu_F[index_out] * scale;
		cu_F[index_out] = (float)tempX;
	}
}


__global__ void scaleFFT3D(cufftComplex *cu_F, int nx, int ny, int nz, double scale)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int zIndex = blockDim.z * blockIdx.z + threadIdx.z;

	if ((xIndex<nx) && (yIndex<ny) && (zIndex<nz))
	{
		unsigned int index_out = xIndex + nx*yIndex + nx*ny*zIndex;
		double tempX = cu_F[index_out].x * scale;
		double tempY = cu_F[index_out].y * scale;
		cu_F[index_out].x = (float)tempX;
		cu_F[index_out].y = (float)tempY;
	}
}



void cuFFT_Real(cufftComplex *freq, float *img, const unsigned int Nx, const unsigned int Ny, int dir)
{
	size_t   Ny_pad = ((Ny >> 1) + 1);
	//size_t   Ny_pad = Ny;
	size_t   N_pad = Nx * Ny_pad;
	size_t   stride = 2 * Ny_pad; // stride on real data	

	// step 1: transfer data to device, sequence by sequence
	cufftReal *img_plane;
	cudaMalloc((void**)&img_plane, sizeof(cufftReal)*Nx*Ny);
	cudaMemcpy(img_plane, img, sizeof(cufftReal)*Nx*Ny, cudaMemcpyDeviceToDevice);

	cufftComplex *FFT_plane;
	cudaMalloc((void**)&FFT_plane, sizeof(cufftComplex)*Nx*Ny_pad);
	cudaMemcpy(FFT_plane, freq, sizeof(cufftComplex)*Nx*Ny_pad, cudaMemcpyDeviceToDevice);

	// step 2: Create a 2D FFT plan. 
	// step 3: Use the CUFFT plan to transform the signal in-place.
	cufftHandle plan;
	cufftResult flag;
	if (dir == -1)
	{
		cufftPlan2d(&plan, Nx, Ny, CUFFT_R2C);
		flag = cufftExecR2C(plan, (cufftReal*)img_plane, (cufftComplex*)FFT_plane);

		cudaMemcpy(freq, FFT_plane, sizeof(cufftComplex)*Nx*Ny_pad, cudaMemcpyDeviceToDevice);
	}
	else if (dir == 1)
	{
		cufftPlan2d(&plan, Nx, Ny, CUFFT_C2R);
		flag = cufftExecC2R(plan, (cufftComplex*)FFT_plane, (cufftReal*)img_plane);

		int blocksInX = (Nx + 32 - 1) / 32;
		int blocksInY = (Ny + 32 - 1) / 32;
		dim3 grid(blocksInX, blocksInY);
		dim3 block(32, 32);
		scaleFFT2DReal << <grid, block >> >(img_plane, Nx, Ny, 1.f / (Nx*Ny));

		cudaMemcpy(img, img_plane, sizeof(cufftReal)*Nx*Ny, cudaMemcpyDeviceToDevice);
	}

	if (flag != CUFFT_SUCCESS)	printf("2D: cufftExec fails\n");

	// make sure that all threads are done
	cudaThreadSynchronize();

	// step 4: copy data to host
	//cudaMemcpy(h_idata, d_idata, sizeof(cufftComplex)*N_pad, cudaMemcpyDeviceToHost);

	// Destroy the CUFFT plan.
	cufftDestroy(plan);
	cudaFree(FFT_plane);
	cudaFree(img_plane);
}

//--------------------------------------------------------------------------------------
__global__ void bilinear_interpolation_kernel(float * __restrict__ d_result, const float * __restrict__ d_data,
	const int M1, const int N1, const int M2, const int N2)
{
	const int i = threadIdx.x + blockDim.x * blockIdx.x;
	const int j = threadIdx.y + blockDim.y * blockIdx.y;

	const float x_ratio = ((float)(M1 - 1)) / M2;
	const float y_ratio = ((float)(N1 - 1)) / N2;

	if ((i<M2) && (j<N2))
	{
		float result_temp1, result_temp2;

		const int    ind_x = (int)(x_ratio * i);
		const float  a = (x_ratio * i) - ind_x;

		const int    ind_y = (int)(y_ratio * j);
		const float  b = (y_ratio * j) - ind_y;

		float d00, d01, d10, d11;
		if (((ind_x)   < M1) && ((ind_y)   < N1))  d00 = d_data[ind_y    *M1 + ind_x];	else	d00 = 0.f;
		if (((ind_x + 1) < M1) && ((ind_y)   < N1))  d10 = d_data[ind_y    *M1 + ind_x + 1];	else	d10 = 0.f;
		if (((ind_x)   < M1) && ((ind_y + 1) < N1))  d01 = d_data[(ind_y + 1)*M1 + ind_x];	else	d01 = 0.f;
		if (((ind_x + 1) < M1) && ((ind_y + 1) < N1))  d11 = d_data[(ind_y + 1)*M1 + ind_x + 1];	else	d11 = 0.f;

		result_temp1 = a * d10 + (-d00 * a + d00);
		result_temp2 = a * d11 + (-d01 * a + d01);
		d_result[i + M2*j] = b * result_temp2 + (-result_temp1 * b + result_temp1);
	}
}
//--------------------------------------------------------------------------------------
void sequence1DFFT(float *ResampleArray, cufftComplex *out_array, int Nx, int Ny)
{
	//host memory
	//cufftComplex *host_FFT = (cufftComplex *)malloc(Nx * (Ny / 4) * sizeof(cufftComplex));
	//cufftComplex *host_out = (cufftComplex *)malloc((Nx / 4)*(Ny / 4) *sizeof(cufftComplex));
	//device memory
	cufftComplex *device_FFT, *out_FFT;
	float *sumFFT_1D;
	cudaMalloc((void **)&device_FFT, sizeof(cufftComplex)*Nx*(Ny / 4));
	cudaMalloc((void **)&out_FFT, sizeof(cufftComplex)*(Nx / 4)*(Ny / 4));
	cudaMalloc((void **)&sumFFT_1D, sizeof(float)*Nx);
	cudaMemset(sumFFT_1D, 0, Nx*sizeof(float));

	//copy the floating array to cufftComplex
	dim3 dimGrid(Nx / TILE_DIM, Ny / 4 / TILE_DIM, 1);
	dim3 dimBlock(TILE_DIM, BLOCK_ROWS, 1);
	real2cufft << <dimGrid, dimBlock >> >(device_FFT, ResampleArray);

	//1D FFT
	cuFFT1D(device_FFT, Nx, Ny / 4, -1);
	//DeviceMemOutFFT("D:\\device_FFT.1024.256.raw", device_FFT, Nx, Ny / 4);
	//crop the component from FT domain
	int blocksInX = (Nx + 32 - 1) / 32;
	int blocksInY = (Ny / 4 + 32 - 1) / 32;
	dim3 grid(blocksInX, blocksInY);
	dim3 block(32, 32);
	shiftArray << <grid, block >> >(device_FFT, Nx, Ny / 4);
	HistogramFT << <grid, block >> >(sumFFT_1D, device_FFT, Nx, Ny / 4);
	//DeviceMemOut("D:\\sumFFT_1D.1024.1.raw", sumFFT_1D, Nx, 1);
	//find out the maximum and its index
	thrust::device_ptr<float> max_ptr = thrust::device_pointer_cast(sumFFT_1D);
	thrust::device_ptr<float> result_offset = thrust::max_element(max_ptr + int(Nx*0.6), max_ptr + Nx);

	//float max_value = result_offset[0];
	int max_idx = &result_offset[0] - &max_ptr[0];
	//printf("\nMininum value = %f\n", max_value);
	//printf("Position = %i\n", &result_offset[0] - &max_ptr[0]);

	int blocksX2 = (Nx / 4 + 32 - 1) / 32;
	int blocksY2 = (Ny / 4 + 32 - 1) / 32;
	dim3 grid2(blocksX2, blocksY2);
	dim3 block2(32, 32);
	CropFTdomain << <grid2, block2 >> >(device_FFT, out_FFT, Nx, Ny, max_idx);
	//DeviceMemOutFFT("D:\\out_FFT.256.256.raw", out_FFT, (Nx / 4), (Ny / 4));
	shiftArray << <grid, block >> >(out_FFT, Nx / 4, Ny / 4);
	//inverse FFT
	cuFFT1D(out_FFT, Nx / 4, Ny / 4, 1);
	//DeviceMemOutFFT("D:\\out_iFFT.256.256.raw", out_FFT, (Nx / 4), (Ny / 4));
	dim3 dimGrid2(Nx / 4 / TILE_DIM, Ny / 4 / TILE_DIM, 1);
	dim3 dimBlock2(TILE_DIM, BLOCK_ROWS, 1);
	copySharedMem << <dimGrid2, dimBlock2 >> >(out_array, out_FFT, Nx / 4);

	cudaFree(device_FFT);
	cudaFree(sumFFT_1D);
	cudaFree(out_FFT);
	//free(host_out);
	//free(host_FFT);
}
//--------------------------------------------------------------------------------------
__global__ void real2cufft(cufftComplex *odata, const float *idata)
{
	__shared__ float tile[TILE_DIM * TILE_DIM];

	int x = blockIdx.x * TILE_DIM + threadIdx.x;
	int y = blockIdx.y * TILE_DIM + threadIdx.y;
	int width = gridDim.x * TILE_DIM;

	for (int j = 0; j < TILE_DIM; j += BLOCK_ROWS)
		tile[(threadIdx.y + j)*TILE_DIM + threadIdx.x] = idata[(y + j)*width + x];

	__syncthreads();

	for (int j = 0; j < TILE_DIM; j += BLOCK_ROWS)
	{
		odata[(y + j)*width + x].x = tile[(threadIdx.y + j)*TILE_DIM + threadIdx.x];
		odata[(y + j)*width + x].y = 0;
	}


}
//--------------------------------------------------------------------------------------
__global__ void copySharedMem(cufftComplex *odata, const cufftComplex *idata, const float scale)
{
	__shared__ cufftComplex tile[TILE_DIM][TILE_DIM];

	int x = blockIdx.x * TILE_DIM + threadIdx.x;
	int y = blockIdx.y * TILE_DIM + threadIdx.y;
	int width = gridDim.x * TILE_DIM;

	for (int j = 0; j < TILE_DIM; j += BLOCK_ROWS)
		tile[threadIdx.y + j][threadIdx.x] = idata[(y + j)*width + x];

	__syncthreads();

	x = blockIdx.y * TILE_DIM + threadIdx.x;  // transpose block offset
	y = blockIdx.x * TILE_DIM + threadIdx.y;

	for (int j = 0; j < TILE_DIM; j += BLOCK_ROWS)
	{
		odata[(y + j)*width + x].x = tile[threadIdx.x][threadIdx.y + j].x * (1 / scale);
		odata[(y + j)*width + x].y = tile[threadIdx.x][threadIdx.y + j].y * (-1 / scale);
	}

}
//--------------------------------------------------------------------------------------
__global__ void HistogramFT(float *sumFFT_1D, cufftComplex *device_FFT, int Nx, int Ny)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;

	if ((xIndex < Nx) && (yIndex < Ny))
	{
		unsigned int idx = xIndex + Nx*yIndex;
		sumFFT_1D[xIndex] += log10(sqrt(device_FFT[idx].x*device_FFT[idx].x + device_FFT[idx].y*device_FFT[idx].y));
	}
}
//--------------------------------------------------------------------------------------
__global__ void shiftArray(cufftComplex *device_FFT, int Nx, int Ny)
{
	cufftComplex tmp;

	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;

	if (xIndex<Nx / 2 && yIndex<Ny)
	{
		tmp = device_FFT[xIndex + Nx*yIndex];
		device_FFT[xIndex + Nx*yIndex] = device_FFT[(xIndex + Nx / 2) + Nx*yIndex];
		device_FFT[(xIndex + Nx / 2) + Nx*yIndex] = tmp;
	}
}
//--------------------------------------------------------------------------------------
__global__ void CropFTdomain(cufftComplex *device_FFT, cufftComplex *device_crop, int Nx, int Ny, int center)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int idx1, idx2;

	if (xIndex < Nx / 4 && yIndex < Ny / 4)
	{
		idx1 = xIndex + (Nx / 4)*yIndex;
		idx2 = (xIndex - (Nx / 8) + center) + Nx*yIndex;
		device_crop[idx1] = device_FFT[idx2];
	}
}

//--------------------------------------------------------------------------------------
__global__ void convertFFT2float(float *dst, cufftComplex *src, int Nx, int Ny)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int idx;

	if (xIndex < Nx && yIndex < Ny)
	{
		idx = xIndex + Nx * yIndex;
		dst[idx] = src[idx].x;
	}
}
//--------------------------------------------------------------------------------------
__global__ void convert2oneByte(uint8_t *dst, float *src, float max, float min, float range, int Nx, int Ny)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int idx = yIndex*Nx + xIndex;
	
	if (xIndex < Nx && yIndex < Ny)
	{
		dst[idx] = (uint8_t)(255.0 * (src[idx] - min) / range);
	}
}
//--------------------------------------------------------------------------------------
void extractQPI(float *SP, cufftComplex *cuSP_FFT, int Nx, int Ny)
{
	int blocksInX = (Nx + 32 - 1) / 32;
	int blocksInY = (Ny + 32 - 1) / 32;
	dim3 grid(blocksInX, blocksInY);
	dim3 block(32, 32);

	float *cuSP_temp, *cuSP_resample;
	cudaMalloc((void **)&cuSP_temp, sizeof(float)*Nx *Ny);
	cudaMalloc((void **)&cuSP_resample, sizeof(float)*Nx*(Ny / 4));

	cudaMemcpy(cuSP_temp, SP, sizeof(float)*Nx*Ny, cudaMemcpyHostToDevice);

	bilinear_interpolation_kernel << <grid, block >> >(cuSP_resample, cuSP_temp, Nx, Ny, Nx, Ny / 4);

	sequence1DFFT(cuSP_resample, cuSP_FFT, Nx, Ny);

	cudaFree(cuSP_temp);
	cudaFree(cuSP_resample);
}

__global__ void estimateWrapPhase(float *SPWrap, cufftComplex *SP, int sizeX, int sizeY)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int i;

	if ((xIndex<sizeX) && (yIndex<sizeY))
	{
		i = xIndex + sizeX * yIndex;
		SPWrap[i] = atan2(SP[i].y, SP[i].x);
	}
}

//--------------------------------------------------------------------------------------
__global__ void estimatePhase(float *Phase, float *UnSPWrap, int sizeX, int sizeY)
{
	unsigned int xIndex = blockDim.x * blockIdx.x + threadIdx.x;
	unsigned int yIndex = blockDim.y * blockIdx.y + threadIdx.y;
	unsigned int idx;
	if ((xIndex<sizeX) && (yIndex<sizeY))
	{
		idx = xIndex + sizeX * yIndex;
		Phase[idx] = (float)(UnSPWrap[idx]);

		if (isnan(Phase[idx]) || isinf(Phase[idx]))
			Phase[idx] = 0;
	}
}

void FastExtraction(uint8_t *dst, uint8_t *src, int Nx, int Ny)
{
	//for original size
	int blocksInX = (Nx + 32 - 1) / 32;
	int blocksInY = (Ny + 32 - 1) / 32;
	dim3 grid(blocksInX, blocksInY);
	dim3 block(32, 32);

	//for original size/4
	int blocksInX3 = (Nx/4 + 32 - 1) / 32;
	int blocksInY3 = (Ny/4 + 32 - 1) / 32;
	dim3 grid3(blocksInX3, blocksInY3);
	dim3 block3(32, 32);

	
	
#pragma omp parallel for
	for (int j = 0; j < Ny; j++)
		for (int i = 0; i < Nx; i++) 
		{
			SP_float[j + i*Nx] = (float)src[i + j*Nx];
		}	
#pragma omp barrier

	extractQPI(SP_float, cuSP2, Nx, Ny);
	//DeviceMemOutFFT("D:\\cuSP2.256.256.raw", cuSP2, Nx2, Ny2);
	
	estimateWrapPhase << <grid3, block3 >> >(SPWrapPhase2, cuSP2, Nx2, Ny2);
	//DeviceMemOut("D:\\SPWrapPhase2.256.256.raw", SPWrapPhase2, Nx2, Ny2);
	//UWLS
	estimatePhase << <grid3, block3 >> >(UnWrapPhaseSP2, SPWrapPhase2, Nx2, Ny2);
	DCT_UWLS_Unwrapped(cuPhaseMap2, UnWrapPhaseSP2, Nx2, Ny2);
	//DeviceMemOut("D:\\UnWrapPhaseSP2.256.256.raw", UnWrapPhaseSP2, Nx2, Ny2);
	//DeviceMemOut("D:\\cuPhaseMap2.256.256.raw", cuPhaseMap2, Nx2, Ny2);
	//resize the Phase & Amp map
	bilinear_interpolation_kernel << <grid, block >> >(cuPhaseMap, cuPhaseMap2, Nx2, Ny2, Nx, Ny);
	//DeviceMemOut("D:\\cuPhaseMap.1024.1024.raw", cuPhaseMap, Nx, Ny);
	//find out the maximum and its index
	thrust::device_ptr<float> ptr = thrust::device_pointer_cast(cuPhaseMap);
	thrust::device_ptr<float> max_result = thrust::max_element(ptr, ptr + Nx*Ny);
	thrust::device_ptr<float> min_result = thrust::min_element(ptr, ptr + Nx*Ny);
	float maxPhi = max_result[0];
	float minPhi = min_result[0];

	//test
	/*float *tmpFloat = (float *)malloc(Nx*Ny*sizeof(float));
	cudaMemcpy(tmpFloat, cuPhaseMap, sizeof(float)*Nx*Ny, cudaMemcpyDeviceToHost);
	FILE *fp = fopen("D:\\buffer.raw", "wb");
	fwrite(tmpFloat, 1024 * 1024, sizeof(float), fp);
	fclose(fp);*/

	//convert to 1 byte	
	convert2oneByte << <grid, block >> >(tmp_uint8, cuPhaseMap, maxPhi, minPhi, maxPhi - minPhi, Nx, Ny);
	cudaMemcpy(dst, tmp_uint8, sizeof(uint8_t)*Nx*Ny, cudaMemcpyDeviceToHost);
}
//--------------------------------------------------------------------------------------
bool is_nan(double dVal)
{
	double dNan = std::numeric_limits<double>::quiet_NaN();

	if (dVal == dNan)
		return true;
	return false;
}
//--------------------------------------------------------------------------------------
bool is_inf(double dVal)
{
	double dNan = std::numeric_limits<double>::infinity();

	if (dVal == dNan)
		return true;
	return false;
}
//--------------------------------------------------------------------------------------
void DeviceMemOut(char *path, float *arr, int sizeX, int sizeY)
{
	int size = sizeX*sizeY;
	float *temp = (float *)malloc(size*sizeof(float));
	cudaMemcpy(temp, arr, size*sizeof(float), cudaMemcpyDeviceToHost);

	FILE *fp;
	fp = fopen(path, "wb");
	fwrite(temp, size, sizeof(float), fp);
	fclose(fp);
	free(temp);
}
//--------------------------------------------------------------------------------------
void DeviceMemOutFFT(char *path, cufftComplex *arr, int sizeX, int sizeY)
{
	int size = sizeX*sizeY;
	cufftComplex *temp = (cufftComplex *)malloc(size*sizeof(cufftComplex));
	float *temp2 = (float *)malloc(size*sizeof(float));
	cudaMemcpy(temp, arr, size*sizeof(cufftComplex), cudaMemcpyDeviceToHost);

	for (int i = 0; i < size; i++)
	{
		temp2[i] = log10(sqrt(temp[i].x*temp[i].x + temp[i].y*temp[i].y));
		if (is_nan(temp2[i]) == true || is_inf(temp2[i]) == true) temp2[i] = 0;
	}

	FILE *fp;
	fp = fopen(path, "wb");
	fwrite(temp2, size, sizeof(float), fp);
	fclose(fp);
	free(temp);
	free(temp2);
}