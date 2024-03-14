/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <string.h>
#include "lwip/err.h"
#include "lwip/tcp.h"
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
#define N 256
XAxiDma AxiDma;
int recv_count = 0;
float received_data_real[N];
float received_data_imaginary[N];
#define DDR_BASE_ADDR  XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define TX_BASE_ADDR   DDR_BASE_ADDR + 0x01000000
#define RX_BASE_ADDR   TX_BASE_ADDR + 0x04000000 // 64M
float complex *tx_buff= (float complex *)TX_BASE_ADDR;
float complex *rx_buff= (float complex *)RX_BASE_ADDR;
float time_diff=0;
XTime start=0;
XTime end=0;
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif
u16_t bytes_to_send = sizeof(float)*N*2;
u8_t test_buf[sizeof(float)*N*2];
u8_t *write_head;

void copy_to_tx_buff(float received_data_real[N], float received_data_imaginary[N]) {
    // Iterate over the arrays and copy each pair of real and imaginary values to tx_buff
    for (int i = 0; i < N; i++) {
        // Create a complex number from the real and imaginary values
        float complex data = received_data_real[i] + I * received_data_imaginary[i];
        // Copy the complex number to tx_buff
        tx_buff[i] = data;
    }
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

void fft(){
	recv_count =0;
	XTime start, end;
	XTime_GetTime(&start);
	copy_to_tx_buff(received_data_real, received_data_imaginary);
	int status_dma = init_DMA();
	if(status_dma != XST_SUCCESS){
			xil_printf("could not initialize DMA\r\n");
			return XST_FAILURE;
	}
	//Any changes made to the data in tx_buff are reflected in the main DDR memory.
	Xil_DCacheFlushRange((UINTPTR)tx_buff, (sizeof(float complex)*N));
	Xil_DCacheFlushRange((UINTPTR)rx_buff, (sizeof(float complex)*N));

	compute(tx_buff, rx_buff);
	// Pointer to iterate over tx_buff
	float complex *tx_ptr = rx_buff;
	// Pointer to iterate over test_buff
	u8_t *test_ptr = test_buf;

	// Process the received array of complex numbers as needed
	for (int i = 0; i < N; i++) {
		// Extract real and imaginary parts of the complex number
		float real_part = creal(*tx_ptr);
		float imag_part = cimag(*tx_ptr);

		// Serialize real and imaginary parts into bytes
		uint8_t *real_bytes = (uint8_t *)&real_part;
		uint8_t *imag_bytes = (uint8_t *)&imag_part;

		// Write real and imaginary bytes into test_buff
		for (int j = 0; j < sizeof(float); j++) {
			*test_ptr++ = real_bytes[j];
		}
		for (int j = 0; j < sizeof(float); j++) {
			*test_ptr++ = imag_bytes[j];
		}
		// Move to the next complex number in tx_buff
		tx_ptr++;
	}
	XTime_GetTime(&end);
	write_head = test_buf;
}
int transfer_data() {
	return 0;
}

void print_app_header()
{
#if (LWIP_IPV6==0)
	xil_printf("\n\r\n\r----- fft compuation ------\n\r");
#else
	xil_printf("\n\r\n\r----- server FPGA  ------\n\r");
#endif
	xil_printf("TCP packets of input and output FFT samples\n\r");
}
err_t push_data(struct tcp_pcb *tpcb)
{
	u16_t max_bytes = tcp_sndbuf(tpcb);\
	u16_t packet_size = N*8;
    err_t status;

    // if there's nothing left to send, exit early
    if (bytes_to_send == 0) {
        return ERR_OK;
    }

    // if adding all bytes to the buffer would make it overflow,
    // only fill the available space
    if (packet_size > max_bytes) {
        packet_size = max_bytes;
    }

    // write to the LWIP library's buffer
    status = tcp_write(tpcb, (void*)write_head, packet_size, 1);

    if(packet_size > bytes_to_send){
    	bytes_to_send = 0;
    }else{
    	bytes_to_send -= packet_size;
    }

    write_head += packet_size;
    bytes_to_send = sizeof(float)*N*2;
    return status;
}


err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    // Check if the packet is NULL or not in the ESTABLISHED state
	if (!p) {
			tcp_close(tpcb);
			tcp_recv(tpcb, NULL);
			return ERR_OK;
	}
    tcp_recved(tpcb, p->tot_len);
    if(recv_count==0){
        XTime_GetTime(&start);
    }
    // Check if the received data length matches the size of the array
    if (p->tot_len != sizeof(float) * N) {
        // If the received data length is incorrect, close the connection
        xil_printf("received data length is incorrect, it is: %d\r\n",p->tot_len);
        tcp_close(tpcb);
        return ERR_OK;
    }

    switch(recv_count){
    	case 0:
    	    memcpy(received_data_real, p->payload, sizeof(float) * (N));
    		break;
    	case 1:
			memcpy(received_data_imaginary, p->payload, sizeof(float) * (N));
			break;
    }

	recv_count++;

    if(recv_count == 2){
    	fft();
    	if (bytes_to_send > 0) {
			err = push_data(tpcb);
		} // else, nothing to send
    }
    pbuf_free(p);

    return ERR_OK;
}
err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	// log to USBUART for debugging purposes
	/*
    xil_printf("entered sent_callback\r\n");

	xil_printf("    bytes_to_send = %d\r\n", bytes_to_send);
	xil_printf("    len = %d\r\n", len);
	xil_printf("    free space = %d\r\n", tcp_sndbuf(tpcb));
	XTime_GetTime(&end);
	time_diff+=((end-start)*1000000.00)/COUNTS_PER_SECOND;
	xil_printf("time difference is: %d\r\n",time_diff);
	*/
	// if all bytes have been sent, we're done
	if (bytes_to_send <= 0){
    	return ERR_OK;
	}
    return push_data(tpcb);
}
err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	 /* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* set the sent callback for this connection */

	tcp_sent(newpcb, sent_callback); // <- add this

	/* just use an integer number indicating the connection id as the
       callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	/* increment for subsequent accepted connections */
	connection++;
	//xil_printf("connection n: %d\r\n", connection);
	return ERR_OK;
}

// global variables used to pass data between callbacks, tcp_arg
// could potentially be used instead

int start_application()
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP echo server started @ port %d\n\r", port);

	return 0;
}


