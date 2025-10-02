/*
   Toggle this for (d) demo that halts at 1111 (set to 1), or keep 0 for (h)(the full clock with button/switch control):
*/
#define STOP_AFTER_START_SEQUENCE 0

#include <stdint.h>
#include <stdbool.h>

/* ---------------------- Memory-mapped I/O ------------------------- */
#define LEDS_ADDR      0x04000000u
#define SWITCHES_ADDR  0x04000010u
#define DISP_BASE      0x04000050u // is the first 7-segment display, each nest display is +0x10 bytes away
#define DISP_STRIDE    0x10u
#define BUTTON_ADDR    0x040000d0u

#define LEDS     ((volatile unsigned int*) LEDS_ADDR) // volatile tells the compiler: this can change outside the program,->
#define SWITCHES ((volatile unsigned int*) SWITCHES_ADDR)//-> writing/reading through these pointers actually talks to the hardware.
#define BUTTON   ((volatile unsigned int*) BUTTON_ADDR)

// the timer
//each register is spaced by 4 bytes even though each is 16-bit
#define TMR_BASE       0x04000020u
#define TMR_STATUS     (TMR_BASE + 0x00)
#define TMR_CONTROL    (TMR_BASE + 0x04)
#define TMR_PERIODL    (TMR_BASE + 0x08)
#define TMR_PERIODH    (TMR_BASE + 0x0C)

#define STATUS     ((volatile unsigned int*) TMR_STATUS)
#define CONTROL    ((volatile unsigned int*) TMR_CONTROL)
#define PERIODL    ((volatile unsigned int*) TMR_PERIODL)
#define PERIODH    ((volatile unsigned int*) TMR_PERIODH)
/*Status bits*/
#define STATUS_TO     (1u << 0) //TO bit: write 0 or 1 to clear
#define STATUS_RUN    (1u << 1) // read-only
/*control bits*/
#define CONTROL_ITO   (1u << 0)
#define CONTROL_CONT  (1u << 1) // continous mode 
#define CONTROL_START (1u << 2) // write 1-event
#define CONTROL_STOP  (1u << 3) //write 1-event



/* these functions lives in other files, here we call them */
extern void print(const char*);           //prints a tring
extern void print_dec(unsigned int);      //print an unsigned integer in decimal form 
extern void display_string(char*);        /* show text on terminal/console */
extern void time2string(char*, int);      /* convert mytime to string */
extern void tick(int*);                   /* increment mytime by one “second” */
extern void delay(int);                   /* approximate delay “seconds” */
extern int  nextprime(int);               /* not used in A1 */

/* ----------------------  globals ------------------ */
int mytime = 0x5957;// is used by time2string and tick to show a text clock
char textstring[] = "text, more text, and even more text!";

/* ---------------------- A3 placeholder (empty in A1) -------------- */
void handle_interrupt(unsigned cause)
{
  /* Not used in Assignment 1 */
}


//(c) LED output, control the leds
    
void set_leds(int led_mask) {//write the Led register, each bit turns one LED on/off
  /* Only 10 LEDs exist: keep LSB 10 bits */
  *LEDS = (unsigned int)(led_mask & 0x3FF);
}


   //(e) write raw values to a 7-segement display
   
void set_displays(int display_number, int value) {
  if (display_number < 0 || display_number > 5) //display_number 0-5 selects which of the six 7-seg displays
     return;
     //we compute it's adress:base+ index *stride (0x10)
  volatile unsigned int* disp = (volatile unsigned int*)(DISP_BASE + (unsigned)display_number * DISP_STRIDE);
  *disp = (unsigned int)value; // we write value directly. On this board, writing 0 lights a segement (active-low)
  //this is a low level and expects a segment pattern not a digit
}

/* Helper: map 0–9 to active-low 7-seg patterns (bit0=a..bit6=g, bit7=dp).
   Writing 0 lights a segment. These are standard common-anode patterns. */
   //digit--> segment lookup table (active-low)
   //handy table: index 0-9 gives you the 7-seg pattern for that digit, dp =decimal point. 0 turns a segment ON
static const unsigned char SEG_DIGIT[10] = {
  0xC0, /* 0 */
  0xF9, /* 1 */
  0xA4, /* 2 */
  0xB0, /* 3 */
  0x99, /* 4 */
  0x92, /* 5 */
  0x82, /* 6 */
  0xF8, /* 7 */
  0x80, /* 8 */
  0x90  /* 9 */
};

// helper to write a single digit to a display
// validates inputs the calls set_displays with the correct segment value
static void set_display_digit(int display_number, int digit) { 
  if (display_number < 0 || display_number > 5) return;
  if (digit < 0 || digit > 9) return;
  set_displays(display_number, SEG_DIGIT[digit]);
}


   //(f) read the 10 toggle switches (SW0..SW9)
   // reads the switch register and keeps the lowest 10 bits (sw0..Sw9)
    
int get_sw(void) { 
  return (int)(*SWITCHES & 0x3FF); /* keep 10 bits */
}


   //(g) read the second push-button 
   // reads the button register and keeps bit 0. Returns 1 when pressed
    
int get_btn(void) {
  return (int)(*BUTTON & 0x1); /* 1 if pressed, else 0 */
}

/* 
   (d) startup: 4-bit binary counter on LEDs 0–3
   - (d) says “stop when all first 4 LEDs are 1”. We support that with
     STOP_AFTER_START_SEQUENCE. For (h) we return and continue.
    */
static void start_sequence(void) {
  set_leds(0); // clears leds
  for (unsigned i = 0; i < 16; ++i) { // counts from 0-15 on the first 4 leds
    set_leds(i & 0xF);  /* show 0000..1111 on LEDs 0–3 */
    delay(2);           // each step waits " about a second"
  }

#if STOP_AFTER_START_SEQUENCE
  /* (d)stop when 1111 is reached */
  for (;;);
#else
  /* For (h): continue program after intro */
  set_leds(0); // otherwise, it clears the LEDs and continues
#endif
}

/* 
   (h) show HH:MM:SS on displays, button+switch to set fields
   - Displays 0..5 are assumed left..right:
       [0][1] hours, [2][3] minutes, [4][5] seconds
   - Field select via SW9..SW8:
       01 -> seconds, 10 -> minutes, 11 -> hours
       00 -> no change
   - Value via SW5..SW0 (0..63)
   - Use SW7 to exit the program (one of the “remaining switches”).
    */
static void show_time_on_displays(int hours, int minutes, int seconds) {
  /* Rightmost pair (HEX0, HEX1) = seconds */
  set_display_digit(1, (seconds / 10) % 10);  /* HEX1 = tens of seconds */
  set_display_digit(0,  seconds % 10);        /* HEX0 = ones of seconds */

  /* Middle pair (HEX2, HEX3) = minutes */
  set_display_digit(3, (minutes / 10) % 10);  /* HEX3 = tens of minutes */
  set_display_digit(2,  minutes % 10);        /* HEX2 = ones of minutes */

  /* Left pair (HEX4, HEX5) = hours */
  set_display_digit(5, (hours   / 10) % 10);  /* HEX5 = tens of hours */
  set_display_digit(4,  hours   % 10);        /* HEX4 = ones of hours */
}

/*The timer actual period is one cycle greater than the value stored in the period registers. So write 3000000-1
converted to hex 0x002DC6BF
*/
void labinit(void) {
 *PERIODH = 0x002D;
 *PERIODL = 0xC6BF;
 *STATUS = STATUS_TO; // I clear TO by writing to the status register
 *CONTROL = CONTROL_CONT | CONTROL_START; // I put the timer in continous mode, the started
 
}

  int main(void) {
  labinit();          // sets up the timer for 100 ms timeouts
  start_sequence();   // your 1d intro (returns, since STOP_AFTER_START_SEQUENCE=0)

  unsigned hours = 0;               // hours since start (0..99)
  mytime = 0x0000;                  // mm:ss in packed BCD for tick()
  static unsigned timeoutcount = 0; //counts 100 ms ticks

  for (;;) {
    /* ---- inputs: always poll every iteration ---- */
    int sw = get_sw();
    if (sw & (1 << 7)) break;       // use SW7 to exit

    if (get_btn()) {
      unsigned sel  = (unsigned)((sw >> 8) & 0x3);  // SW9..SW8
      unsigned val6 = (unsigned)(sw & 0x3F);        // SW5..SW0 (0..63)

      if (sel == 0x1) {             // 01 -> seconds
        unsigned s = val6 % 60u;
        unsigned s_bcd = ((s / 10u) << 4) | (s % 10u);
        mytime = (mytime & ~0x00FF) | s_bcd;

      } else if (sel == 0x2) {      // 10 -> minutes
        unsigned m = val6 % 60u;
        unsigned m_bcd = ((m / 10u) << 4) | (m % 10u);
        mytime = (mytime & ~0xFF00) | (m_bcd << 8);

      } else if (sel == 0x3) {      // 11 -> hours
        hours = val6 % 100u;
      }
      /* 00 -> no change */
    }

    /* ---- time + display: only on 100 ms timeout ---- */
    if (*STATUS & STATUS_TO) {
      *STATUS = STATUS_TO;          // acknowledge (clear) timeout first

      if (++timeoutcount >= 10){
        timeoutcount = 0;

      tick(&mytime);               // advance mm:ss every timeout (≈10×/sec in 2b)

      // derive seconds/minutes from packed BCD mytime
      unsigned sec_bcd =  (unsigned)( mytime  & 0xFF);
      unsigned min_bcd =  (unsigned)((mytime >> 8) & 0xFF);
      unsigned seconds  = ((sec_bcd >> 4) * 10u) + (sec_bcd & 0xFu);
      unsigned minutes  = ((min_bcd >> 4) * 10u) + (min_bcd & 0xFu);

      if (seconds == 0 && minutes == 0) {
        hours = (hours + 1) % 100u; // 2-digit hours
      }

      // terminal + 7-seg updates only on timeout (prevents flicker)
      time2string(textstring, mytime);
      display_string(textstring);
      show_time_on_displays((int)hours, (int)minutes, (int)seconds);
      print("\n");
    }}
  }

  set_leds(0x3FF);   // signal end
  for (;;);          // halt
}

/*
1.When the time-out event-flag is a "1", how does your code reset it to "0"?
    By writing the TO bit back to the status register: *STatuS = STATUS_TO; 
    this clears the event so the next timeout can be detected


2. What would happen if the time-out event-flag was not reset to "0" by your code? Why?
  It stays set, so the polling loop will think a timeout is present every iteration. You'll keep running the "ontimeout" code continously
  because the condition never goes false


3. Which device-register (or registers) must be written to define the time between time-out
events? Describe the function of that register (or of those registers).
  - PERIODH/PERIODL: the form together the period valu used by the timer. The actual period is cycles, so for 100 ms at 30 MHz you store 3000000-1 = 0x002DC68F
  split into hig/low.
  - Control: I set CONT(continious mode), START to start counting 

• If you press BTN1 quickly, does the time update reliably? Why, or why not? If not, would
that be easy to change? If so, how?
    In this asignment it's a polling code so very short presses can be missed if they happen between polls or during long work. I can fix it
    by polling every iteration, but it still can be bounce.

*/