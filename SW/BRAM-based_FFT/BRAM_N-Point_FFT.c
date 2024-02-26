
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
#include <xbram.h>
#define N 1024

#define DDR_BASE_ADDR  XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define TX_BASE_ADDR   DDR_BASE_ADDR + 0x01000000
#define RX_BASE_ADDR   TX_BASE_ADDR + 0x04000000 // 64M
#define BRAM1_DEVICE_ID		XPAR_BRAM_0_DEVICE_ID
#define BRAM2_DEVICE_ID		XPAR_BRAM_1_DEVICE_ID


XAxiDma AxiDma;
XBram Bram;	/* The Instance of the BRAM Driver */
int init_BRAM1(){
	int Status;
	XBram_Config *ConfigPtr;

	/*
	 * Initialize the BRAM driver. If an error occurs then exit
	 */

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = XBram_LookupConfig(BRAM1_DEVICE_ID);
	if (ConfigPtr == (XBram_Config *) NULL) {
		return XST_FAILURE;
	}

	Status = XBram_CfgInitialize(&Bram, ConfigPtr,
					 ConfigPtr->CtrlBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


		//InitializeECC(ConfigPtr, ConfigPtr->CtrlBaseAddress);


	/*
	 * Execute the BRAM driver selftest.
	 */
	Status = XBram_SelfTest(&Bram, 0);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}
int init_BRAM2(){
	int Status;
	XBram_Config *ConfigPtr;

	/*
	 * Initialize the BRAM driver. If an error occurs then exit
	 */

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = XBram_LookupConfig(BRAM2_DEVICE_ID);
	if (ConfigPtr == (XBram_Config *) NULL) {
		return XST_FAILURE;
	}

	Status = XBram_CfgInitialize(&Bram, ConfigPtr,
					 ConfigPtr->CtrlBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


		//InitializeECC(ConfigPtr, ConfigPtr->CtrlBaseAddress);


	/*
	 * Execute the BRAM driver selftest.
	 */
	Status = XBram_SelfTest(&Bram, 0);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}
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
void compute(){
	int status_transfer;
	status_transfer = XAxiDma_SimpleTransfer(&AxiDma, XPAR_BRAM_1_BASEADDR, (8*N), XAXIDMA_DEVICE_TO_DMA);
	if(status_transfer !=XST_SUCCESS){
		xil_printf("writing data to fft ip via dma failed\r\n");
	}
	status_transfer = XAxiDma_SimpleTransfer(&AxiDma, XPAR_BRAM_0_BASEADDR, (8*N), XAXIDMA_DMA_TO_DEVICE);
	if(status_transfer !=XST_SUCCESS){
		xil_printf("reading data from fft ip via dma failed\r\n");
	}
	while ((XAxiDma_Busy(&AxiDma,XAXIDMA_DEVICE_TO_DMA)) || (XAxiDma_Busy(&AxiDma,XAXIDMA_DMA_TO_DEVICE)));
}
float launch(int repeat, bool print){
	XTime start, end;
    srand(6);
    u_int32_t values[N * 2];

	// Seed the random number generator
	srand(432);  // You can use a different seed if you want
	// Generate random values
	for (int i = 0; i < N * 2; i++) {
		values[i] = rand();
	}
	int counter=0;
	init_BRAM1();
	init_BRAM2();
	for(int i=0; i<N*8; i=i+4){
		XBram_WriteReg(XPAR_BRAM_0_BASEADDR,i,values[counter]);
		counter++;
	}
	int status_dma = init_DMA();
	if(status_dma != XST_SUCCESS){
			xil_printf("could not initialize DMA\r\n");
			return XST_FAILURE;
	}
	float timing=0;
	for(int i=0; i<repeat; i++){
		XTime_GetTime(&start);
		compute();
		XTime_GetTime(&end);
		timing+=((end-start)*1000000.0000)/COUNTS_PER_SECOND;
	}
	if(print==1){
		u32 results[N*2];
		for(int j=0;j<N*8; j=j+4){
			results[counter]=XBram_ReadReg(XPAR_BRAM_1_BASEADDR+N*8,j);
			xil_printf("i %d:%x\n\r",j,results[counter]);
			counter++;
		}
	}
	return timing/(repeat*1.0000);
}
int main()
{
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(100,0));
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(2000,0));
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(5000,0));
	printf("Average Computation time for %d Point FFT is: %f mircoseconds\r\n",N,launch(8000,0));

	return XST_SUCCESS;
}
