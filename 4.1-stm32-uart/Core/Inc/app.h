#ifndef APP_H
#define APP_H

/* Starts UART reception and sends the start-up banner. Call once after MX_*_Init. */
void App_Start(void);

/* Handles a received command line and the KEY button. Call from the main loop. */
void App_Poll(void);

#endif /* APP_H */
