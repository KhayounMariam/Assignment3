#define STOP_AFTER_START_SEQUENCE 0

#include <stdint.h>
#include <stdbool.h>

/* ---------------------- Memory-mapped I/O ------------------------- */
#define LEDS_ADDR      0x04000000u
#define SWITCHES_ADDR  0x04000010u
#define DISP_BASE      0x04000050u
#define DISP_STRIDE    0x10u
#define GPIO_BASE      0x040000D0u  // Button PIO base

#define LEDS     ((volatile unsigned int*) LEDS_ADDR)
#define SWITCHES ((volatile unsigned int*) SWITCHES_ADDR)


#define BTN_DATA      ((volatile unsigned int*)(GPIO_BASE + 0x0))  // data
#define BTN_DIR       ((volatile unsigned int*)(GPIO_BASE + 0x4))  // direction (may be RO if input-only)
#define BTN_INTMASK   ((volatile unsigned int*)(GPIO_BASE + 0x8))  // interruptmask
#define BTN_EDGECAP   ((volatile unsigned int*)(GPIO_BASE + 0xC))  // edgecapture (write-1-to-clear)

/* ---------------------- Timer registers --------------------------- */
#define TMR_BASE       0x04000020u
#define TMR_STATUS     (TMR_BASE + 0x00)
#define TMR_CONTROL    (TMR_BASE + 0x04)
#define TMR_PERIODL    (TMR_BASE + 0x08)
#define TMR_PERIODH    (TMR_BASE + 0x0C)

#define STATUS     ((volatile unsigned int*) TMR_STATUS)
#define CONTROL    ((volatile unsigned int*) TMR_CONTROL)
#define PERIODL    ((volatile unsigned int*) TMR_PERIODL)
#define PERIODH    ((volatile unsigned int*) TMR_PERIODH)

/* Status bits */
#define STATUS_TO     (1u << 0)
/* Control bits */
#define CONTROL_ITO   (1u << 0)
#define CONTROL_CONT  (1u << 1)
#define CONTROL_START (1u << 2)
#define CONTROL_STOP  (1u << 3)

/* ---------------------- External causes --------------------------- */
#define EXT_CAUSE_BUTTON0  18u   
#define GENERIC_EXT_CAUSE  11u   

/* ---------------------- Globals used by ISR ----------------------- */
static volatile unsigned isr_hours = 0;         // 0..99, shown on left two 7-seg digits
static volatile unsigned isr_timeoutcount = 0;  
int prime = 1234567; // for main loop printing primes

/* ---------------------- Externs from runtime ---------------------- */
extern void print(const char*);
extern void print_dec(unsigned int);
extern void display_string(char*);
extern void time2string(char*, int);
extern void tick(int*);
extern void delay(int);
extern int  nextprime(int);

/* Provided in boot.S */
extern void enable_interrupt(void);

/* ---------------------- Clock state ------------------------------- */
int mytime = 0x5957; // MM:SS (packed BCD)
char textstring[] = "text, more text, and even more text!";

/* ---------------------- 7-seg helpers ----------------------------- */
static const unsigned char SEG_DIGIT[10] = {
  0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90
};

static void set_displays(int display_number, int value) {
  if (display_number < 0 || display_number > 5) return;
  volatile unsigned int* disp =
      (volatile unsigned int*)(DISP_BASE + (unsigned)display_number * DISP_STRIDE);
  *disp = (unsigned int)value;
}

static void set_display_digit(int display_number, int digit) {
  if ((unsigned)display_number > 5u) return;
  if ((unsigned)digit > 9u) return;
  set_displays(display_number, SEG_DIGIT[digit]);
}

static void show_time_on_displays(int hours, int minutes, int seconds) {
  set_display_digit(1, (seconds / 10) % 10);
  set_display_digit(0,  seconds % 10);

  set_display_digit(3, (minutes / 10) % 10);
  set_display_digit(2,  minutes % 10);

  set_display_digit(5, (hours   / 10) % 10);
  set_display_digit(4,  hours   % 10);
}

/* ---------------------- Simple I/O -------------------------------- */
static void set_leds(int led_mask) { *LEDS = (unsigned)(led_mask & 0x3FF); }

/* ---------------------- Start sequence (A1d) ---------------------- */
static void start_sequence(void) {
  set_leds(0);
  for (unsigned i = 0; i < 16; ++i) {
    set_leds(i & 0xF);
    delay(2);
  }
#if STOP_AFTER_START_SEQUENCE
  for (;;);
#else
  set_leds(0);
#endif
}

/* ---------------------- ISR ----------------------------- */
void handle_interrupt(unsigned cause)
{
  /* --- External interrupt from GPIO PIO (Button0) --- */
  if (cause == EXT_CAUSE_BUTTON0 || cause == GENERIC_EXT_CAUSE) {
    unsigned ec = *BTN_EDGECAP;          
    if (ec & 0x1u) {                      // bit 0 is Button0
      
      if ((*BTN_DATA & 0x1u) != 0u) {
        unsigned sec_bcd =  (unsigned)( mytime        & 0xFFu);
        unsigned min_bcd =  (unsigned)((mytime >> 8)  & 0xFFu);
        unsigned seconds  = ((sec_bcd >> 4) * 10u) + (sec_bcd & 0xFu);
        unsigned minutes  = ((min_bcd >> 4) * 10u) + (min_bcd & 0xFu);
        unsigned total    = minutes * 60u + seconds;
        if (total + 2u >= 3600u) {        // MM:SS will wrap to 00:00 => bump hours
          isr_hours = (isr_hours + 1u) % 100u;
        }
        // +2 seconds = reuse tick() twice
        tick(&mytime);
        tick(&mytime);
      }
      *BTN_EDGECAP = 0x1u;               
    }
    return;                             
  }

  
  if (*STATUS & STATUS_TO) {
    *STATUS = STATUS_TO;

    if (++isr_timeoutcount >= 10) {
      isr_timeoutcount = 0;

      unsigned sec_bcd =  (unsigned)( mytime        & 0xFFu);
      unsigned min_bcd =  (unsigned)((mytime >> 8)  & 0xFFu);
      unsigned seconds  = ((sec_bcd >> 4) * 10u) + (sec_bcd & 0xFu);
      unsigned minutes  = ((min_bcd >> 4) * 10u) + (min_bcd & 0xFu);

      show_time_on_displays((int)isr_hours, (int)minutes, (int)seconds);

      tick(&mytime);

      if (seconds == 0 && minutes == 0) {
        isr_hours = (isr_hours + 1u) % 100u;
      }
    }
  }
}

/* ---------------------- Init ------------------------------ */
void labinit(void) {
  *PERIODH = 0x002D;
  *PERIODL = 0xC6BF;
  *STATUS  = STATUS_TO;
  *CONTROL = CONTROL_CONT | CONTROL_ITO | CONTROL_START;

  
  *BTN_INTMASK = 0x1u;        
  *BTN_EDGECAP = 0xFFFFFFFFu; 

  enable_interrupt();
}

/* ---------------------- Main ------------------------------ */
int main(void) {
  labinit();
  start_sequence();

  while (1) {
    print("Prime: ");
    prime = nextprime(prime);
    print_dec(prime);
    print("\n");
  }
}