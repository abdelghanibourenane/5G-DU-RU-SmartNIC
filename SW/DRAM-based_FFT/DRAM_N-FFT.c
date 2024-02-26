
#include <stdio.h>
#include <stdbool.h>
#include "xaxidma.h"
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include <stdlib.h>
#include <complex.h>
#include <time.h>
#include <xtime_l.h>
#include <math.h>
#define N 1024
XAxiDma AxiDma;
#define DDR_BASE_ADDR  XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define TX_BASE_ADDR   DDR_BASE_ADDR + 0x01000000
#define RX_BASE_ADDR   TX_BASE_ADDR + 0x04000000 // 64M


int init_DMA() {
    XAxiDma_Config *CfgPtr;
    int status;

    //Get DMA Configuration
    CfgPtr = XAxiDma_LookupConfig(XPAR_AXI_DMA_0_DEVICE_ID);

    if (!CfgPtr) {
       xil_printf("No configuration found for AXI DMA with device ID %d\r\n", XPAR_AXI_DMA_0_DEVICE_ID);
       return XST_FAILURE;
    }
   //Set up DMA Configuration
    status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (status != XST_SUCCESS) {
       xil_printf("ERROR: DMA configuration failed\r\n");
       return XST_FAILURE;
    }
    //Check for scatter-gather configuration
    if (XAxiDma_HasSg(&AxiDma)) {
       xil_printf("INFO: Device configured in SG.\r\n");
       return XST_FAILURE;
    }
    return XST_SUCCESS;
}
u32 checkIdle(u32 baseAddress, u32 offset){
	u32 status;
	status = (XAxiDma_ReadReg(baseAddress, offset)) & XAXIDMA_IDLE_MASK;
	return status;
}
void compute(float complex *tx_buff, float complex *rx_buff){
	int status_transfer;
	status_transfer = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)rx_buff, (sizeof(float complex)*N), XAXIDMA_DEVICE_TO_DMA);
	if(status_transfer !=XST_SUCCESS){
		xil_printf("writing data to fft ip via dma failed\r\n");
	}
	status_transfer = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)tx_buff, (sizeof(float complex)*N), XAXIDMA_DMA_TO_DEVICE);
	if(status_transfer !=XST_SUCCESS){
		xil_printf("reading data from fft ip via dma failed\r\n");
	}
    while ((XAxiDma_Busy(&AxiDma,XAXIDMA_DEVICE_TO_DMA)) || (XAxiDma_Busy(&AxiDma,XAXIDMA_DMA_TO_DEVICE)));
}
float launch(int repeat, bool print){
	XTime start, end;
    srand(6);
	// Array to store complex values
	float complex values[N];
	// Generate random complex values
	for (int i = 0; i < N; i++) {
		float real_part = (float)rand() / RAND_MAX * 100.0;
		float imag_part = (float)rand() / RAND_MAX * 100.0;
		values[i] = real_part + imag_part * I;
	}
	float complex *tx_buff, *rx_buff;
	tx_buff = (float complex *)TX_BASE_ADDR;
	rx_buff = (float complex *)RX_BASE_ADDR;
	for (int i = 0; i < N; i++) {
		tx_buff[i] = values[i];
	}
	int status_dma = init_DMA();
	if(status_dma != XST_SUCCESS){
			xil_printf("could not initialize DMA\r\n");
			return XST_FAILURE;
	}
	//Any changes made to the data in tx_buff are reflected in the main DDR memory.
	Xil_DCacheFlushRange((UINTPTR)tx_buff, (sizeof(float complex)*N));
	Xil_DCacheFlushRange((UINTPTR)rx_buff, (sizeof(float complex)*N));
	float timing=0;
	for(int i=0; i<repeat; i++){
		XTime_GetTime(&start);
		compute(tx_buff, rx_buff);
		XTime_GetTime(&end);
		timing+=((end-start)*1000000.0000)/COUNTS_PER_SECOND;
	}
	if(print==1){
		for (int i = 0; i < N; ++i) {
			printf("Element %d: %.2f + %.2f*I\r\n", i + 1, crealf(tx_buff[i]), cimagf(tx_buff[i]));
			printf("Result %d: %.2f + %.2f*I\r\n", i + 1, crealf(rx_buff[i]), cimagf(rx_buff[i]));
		}
	}
	return timing/(repeat*1.0000);
}
int main()
{
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(1000,0));
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(3000,0));
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(7000,0));

	return XST_SUCCESS;
}

