#define RCC_BASE_ADDRESS            0x40023800
#define RCC_AHB1ENR                 RCC_BASE_ADDRESS + 0x30  // RCC AHB1 peripheral clock enable register (pg 116)
#define RCC_APB1ENR                 RCC_BASE_ADDRESS + 0x40  // RCC APB1 peripheral clock enable register (pg 117)

#define GPIOA_BASE_ADDRESS          0x40020000
#define GPIOA_MODER                 GPIOA_BASE_ADDRESS + 0x00 // GPIO port mode register
#define GPIOA_OSPEEDR               GPIOA_BASE_ADDRESS + 0x08 // GPIO port output speed register
#define GPIOA_PUPR                  GPIOA_BASE_ADDRESS + 0x0C // GPIO port pull-up/pull-down register
#define GPIOA_ODR                   GPIOA_BASE_ADDRESS + 0x14 // GPIO port output data register
#define GPIOA_AFRL                  GPIOA_BASE_ADDRESS + 0x20 // GPIO alternate function low register

#define USART2_BASE_ADDRESS         0x40004400
#define USART2_SR                   USART2_BASE_ADDRESS + 0x00 // Status register
#define USART2_DR                   USART2_BASE_ADDRESS + 0x04 // Data register
#define USART2_BRR                  USART2_BASE_ADDRESS + 0x08 // Baud rate register
#define USART2_CR1                  USART2_BASE_ADDRESS + 0x0C // Control register 1
#define USART2_CR3                  USART2_BASE_ADDRESS + 0x14 // Control register 3

#define DMA1_BASE_ADDRESS           0x40026000
#define DMA1_HISR                   DMA1_BASE_ADDRESS + 0x04 // DMA high interrupt status register
#define DMA1_HIFCR                  DMA1_BASE_ADDRESS + 0x0C // DMA high interrupt flag clear register
#define DMA1_S6CR                   DMA1_BASE_ADDRESS + (0x10 + (6 * 0x18)) // DMA stream 6 FIFO control register
#define DMA1_S6NDTR                 DMA1_BASE_ADDRESS + (0x14 + (6 * 0x18)) // DMA stream 6 number of data register
#define DMA1_S6PAR                  DMA1_BASE_ADDRESS + (0x18 + (6 * 0x18)) // DMA stream 6 peripheral address register
#define DMA1_S6M0AR                 DMA1_BASE_ADDRESS + (0x1C + (6 * 0x18)) // DMA stream 6 memory 0 address register
#define DMA1_S6FCR                  DMA1_BASE_ADDRESS + (0x24 + (6 * 0x24)) // DMA stream 6 FIFO control register

#define NVIC_BASE_ADDRESS           0xE000E100  // See pg 218 of PM2014
#define NVIC_ISER0                  NVIC_BASE_ADDRESS + 0x00  // "Interrupt set-enable registers" See pg 209 of PM2014
#define NVIC_ISER1                  NVIC_BASE_ADDRESS + 0x04  // "Interrupt set-enable registers" See pg 209 of PM2014

#define ACCESS(address)             *((volatile unsigned int*)(address))

#define MAX_BUFFER_SIZE             2000
unsigned char dmaBuffer[MAX_BUFFER_SIZE];

volatile int uartBusy = 0;

void DMA1_Stream6_IRQHandler(void)
{
	// See pg 187.  Bit 21 is the "Stream 6 transfer complete interrupt flag".  This bit
	// will be set when the DMA transfer is complete.
	if((ACCESS(DMA1_HISR) & (1 << 21)) != 0)
	{
		// See pg 188.  Here we clear the transfer complete interrupt.
		ACCESS(DMA1_HIFCR) |= (1 << 21);

		// See pg 552.  Here we specify that we want an interrupt generated once the
		// USART transmission is complete.
		ACCESS(USART2_CR1) |= (1 << 6);
	}
}

void USART2_IRQHandler(void)
{
	// See pg 549.  Bit 6 of the status register will be set when the UART
	// transmission has completed.
	if((ACCESS(USART2_SR) & (1 << 6)) != 0)
	{
		// Clear the interrupt. (...So that it doesn't continually trigger)
		ACCESS(USART2_CR1) &= ~(1 << 6);

		// Clear the busy flag to allow the next transmission.
		uartBusy = 0;
	}
}

void UartGpioInit()
{
	// Give a clock to port A as we'll be using one of its pins for transfer of data.
	ACCESS(RCC_AHB1ENR) |= 1;

	// See pg 19 of the ST UM1842 document.  We'll be using USART2.  USART2 TX occurs on
	// PA2 and USART2 RX occurs on PA3 so we set this pin to alternate mode.
	ACCESS(GPIOA_MODER) |= ((1 << 5) | (1 << 7));

	// See pg 149 of ST RM0383 document.  USART2 is AF7.  And pg 160 of the same document
	// shows alternate function for pins 2 and 3 are set using alternate function low register
	// bits 8-thru-11.
	ACCESS(GPIOA_AFRL) |= ((7 << 8) | (7 << 12));

	// Set PA2 and PA3 to high speed
	ACCESS(GPIOA_OSPEEDR) |= ((3 << 4) | (3 << 6));
}

void UartDmaInit()
{
	// Enable a clock for DMA1
	ACCESS(RCC_AHB1ENR) |= (1 << 21);

	// See pg 189 for details of the DMA stream configuration register
	// Setting bit 6 specifies memory-to-peripheral communication.
	// Setting bit 10 specifies to incrememnt the memory pointer after each data transfer.  This
	// allows the DMA device to progressively step through the dmaBuffer array.
	// Setting bit 27 specifies to use channel 4 of stream 6.
	ACCESS(DMA1_S6CR) |= ((1 << 6) | (1 << 10) | (1 << 27));

	// Enable interrupt for DMA1_Stream6
	ACCESS(NVIC_ISER0) |= (1 << 17);
}

void UartInit()
{
	// Give a clock USART2.  See pg 117.
	ACCESS(RCC_APB1ENR) |= (1 << 17);

	// Here we set the baud rate.  This is explained on 519 of ST RM0383.  The peripheral
	// clock is 16 MHz by default.  So, the calculation for the USARTDIV is:
	// DesiredBaudRate = 16MHz / 16*USARTDIV
	// Note that the fractional part of USARTDIV is represented with only 4 bits.  So
	// if we use 9600 this will result in a small error of 0.02% (see pg 522).  Therefore
	// the baud rate is actually 9,598.  Again, see pg 522.
	ACCESS(USART2_BRR) |= 0x683;

	// Enable USART2 for transmitting data.
	ACCESS(USART2_CR1) |= ((1 << 3) | (1 << 13));

	// See pg 200 of RM0383 ("STM32F411xC/E Reference Manual") for the interrupt
	// vector table.  You'll see TIM3 is interrupt 29.
	// See pg 209 of PM0214 ("STM32F4 Series Programming Manual") for info on "ISER"
	// which is one of the "interrupt set-enable registers".  We enable interrupt
	// number 29.
	ACCESS(NVIC_ISER1) |= (1 << (38 - 32));
}

void SendString(char* string)
{
	// Here we block until the previous transfer completes.
	while(uartBusy == 1);
	uartBusy = 1;

	// Copy the string into the DMA buffer and also calculate its length.
	int lengthOfString = 0;
	while(lengthOfString < MAX_BUFFER_SIZE && string[lengthOfString])
	{
		dmaBuffer[lengthOfString] = string[lengthOfString];
		++lengthOfString;
	}

	// See pg 192.  This register holds the number of data items to transfer.
	ACCESS(DMA1_S6NDTR) = lengthOfString;

	// See pg 193.  This register holds the peripheral data register.  Since we're
	// using USART2 we set it to its data register.
	ACCESS(DMA1_S6PAR) = USART2_DR;

	// See pg 193.  This register holds the memory address of the data we want to transfer.
	ACCESS(DMA1_S6M0AR) = (unsigned int)dmaBuffer;

	// Enable interrupt to occur upon completion of transfer
	ACCESS(DMA1_S6CR) |= (1 << 4);

	// Enable the stream
	ACCESS(DMA1_S6CR) |= 1;

	// Clear the transfer complete flag in the UART SR
	ACCESS(USART2_SR) &= ~(1 << 6);

	// Enable DMA transmission
	ACCESS(USART2_CR3) |= (1 << 7);
}

int main(void)
{
	UartGpioInit();
	UartDmaInit();
	UartInit();

	SendString("Hello world\r\n");
	SendString("...and goodbye\r\n");
	SendString("...and hello again\r\n");

	while(1);
}
