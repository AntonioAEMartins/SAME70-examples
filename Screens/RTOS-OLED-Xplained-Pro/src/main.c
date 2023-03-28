#include <asf.h>
#include "conf_board.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/* Botao da placa */
#define BUT_PIO     PIOA
#define BUT_PIO_ID  ID_PIOA
#define BUT_PIO_PIN 11
#define BUT_PIO_PIN_MASK (1 << BUT_PIO_PIN)

#define TR_PIO PIOA
#define TR_ID ID_PIOA
#define TR_IDX 6
#define TR_MASK (1 << TR_IDX)

#define EC_PIO PIOD
#define EC_ID ID_PIOD
#define EC_IDX 30
#define EC_MASK (1 << EC_IDX)

/** RTOS  */
#define TASK_OLED_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_OLED_STACK_PRIORITY            (tskIDLE_PRIORITY)
#define TASK_SENSOR_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_SENSOR_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);
extern float lista_distancia[10] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
extern int lista_eixo_max[10] = {12,24,36,48,60,72,84,96,108};
extern int lista_eixo_min[10] = {0,12,24,36,48,60,72,84,96,108};

QueueHandle_t xQueuePing;
QueueHandle_t xQueueEcho;
QueueHandle_t xQueueOled;

BaseType_t xHigherPriorityTaskWoken = pdTRUE;

/** prototypes */
void but_callback(void);
static void BUT_init(void);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
}

void RTT_Handler(void){
	uint32_t ul_status;
	ul_status = rtt_get_status(RTT);
	
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS){
	}
}

void but_callback(void) {
	int active = 1;
	xQueueSendFromISR(xQueuePing, &active, &xHigherPriorityTaskWoken);
}

void echo_callback(void){
	if(pio_get(EC_PIO, PIO_INPUT, EC_MASK)){
		RTT_init(32768, 0, 0);
	} else {
		float ul_result = rtt_read_timer_value(RTT) / 32768.0;
		xQueueSendFromISR(xQueueEcho, &ul_result, &xHigherPriorityTaskWoken);
	}
}

void trigger (void){
	pio_set(TR_PIO, TR_MASK);
	delay_us(10);
	pio_clear(TR_PIO, TR_MASK);
}


void reset_oled(){
	gfx_mono_draw_filled_rect(0, 10, 118, 22, GFX_PIXEL_CLR);
}

void write_oled(char string[], int x, int y){
	gfx_mono_draw_string(string, x, y, &sysfont);
}

int devolve_y(float valor){
	if (valor < 0.1){
		return 25;
	}
	else if (valor >= 0.1 & valor < 0.3){
		return 24;
	}
	else if (valor >= 0.3 & valor < 0.5){
		return 23;
	}
	else if (valor >= 0.5 & valor < 0.7){
		return 22;
	}
	else if (valor >= 0.7 & valor < 0.9){
		return 19;
	}
	else if (valor >= 0.9 & valor < 1.1){
		return 18;
	}
	else if (valor >= 1.1 & valor < 1.3){
		return 17;
	}
	else if (valor >= 1.3 & valor < 1.5){
		return 16;
	}
	else if (valor >= 1.5 & valor < 1.7){
		return 15;
	}
}

void graph(int w, int h){
	for (int l = 0; l < 10; l++){
		int valor = devolve_y(lista_distancia[l]);
		printf("%d\n", valor);
		printf("%f\n", lista_distancia[l]);
		for (int i = lista_eixo_min[l]; i < lista_eixo_max[l]; i+=2){
			gfx_mono_draw_rect(i, valor, w, h, GFX_PIXEL_SET);
		}
	}
}

void add_lista(float distance){
	for (int i = 0; i < 10; i++){
		lista_distancia[i] = lista_distancia[i + 1];
	}
	lista_distancia[10 - 1] = distance;
}

static void task_oled(void *pvParameters){
	gfx_mono_ssd1306_init();
	
	float distance = 0;
	char string[3];
	
	for(;;){
		if (xQueueReceive(xQueueOled, &distance, 0)){
			add_lista(distance);
			sprintf(string, "%f", distance);
			write_oled(string, 10 , 0);
			reset_oled();
			graph(2, 2);
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

static void task_sensor(void *pvParameters){
	int ping = 0;
	float ec = 0.0;
	float distance = 0.0;
	
	for (;;)  {
		trigger();
		if (xQueueReceive(xQueueEcho, &ec, 0)){
			distance = (float)(ec * 340.0) / (2.0);
			xQueueSendFromISR(xQueueOled, &distance, &xHigherPriorityTaskWoken);
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}


static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

static void BUT_init(void) {
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);

	pio_configure(BUT_PIO, PIO_INPUT, BUT_PIO_PIN_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT_PIO, BUT_PIO_PIN_MASK, 60);
	pio_enable_interrupt(BUT_PIO, BUT_PIO_PIN_MASK);
	pio_handler_set(BUT_PIO, BUT_PIO_ID, BUT_PIO_PIN_MASK, PIO_IT_FALL_EDGE , &but_callback);
}

static void io_init(void){
	pmc_enable_periph_clk(TR_ID);
	pmc_enable_periph_clk(EC_ID);
	
	pio_configure(TR_PIO, PIO_OUTPUT_0, TR_MASK, PIO_DEFAULT);
	pio_configure(EC_PIO, PIO_INPUT, EC_MASK, PIO_DEFAULT);
	
	pio_handler_set(EC_PIO, EC_ID, EC_MASK, PIO_IT_EDGE, &echo_callback);

	
	pio_enable_interrupt(EC_PIO, EC_MASK);
	pio_get_interrupt_status(EC_PIO);
	
	NVIC_EnableIRQ(EC_ID);
	
	NVIC_SetPriority(EC_ID, 4);
}

int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	BUT_init();
	io_init();
	/* Initialize the console uart */
	configure_console();
	printf("Comecou\n");
	
	xQueuePing = xQueueCreate(32, sizeof(uint32_t));
	xQueueEcho = xQueueCreate(32, sizeof(float));
	xQueueOled = xQueueCreate(32, sizeof(float));
	
	/* Create task to control oled */
	if (xTaskCreate(task_oled, "oled", TASK_OLED_STACK_SIZE, NULL, TASK_OLED_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create oled task\r\n");
	}
	
	if (xTaskCreate(task_sensor, "sensor", TASK_SENSOR_STACK_SIZE, NULL, TASK_SENSOR_STACK_PRIORITY, NULL) != pdPASS){
		printf("Failed to create sensor task\r\n");
	}
	
	if (xQueuePing == NULL){
		printf("Failed to create Ping List\n");
	}
	
	if (xQueueEcho == NULL){
		printf("Failed to create Echo List\n");
	}
	
	if (xQueueOled == NULL){
		printf("Failed to create Oled List\n");
	}

	/* Start the scheduler. */
	vTaskStartScheduler();
	/* RTOS não deve chegar aqui !! */
	while(1){}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
