#include <avr/io.h>      // Include AVR IO definitions
#include <stdlib.h>      // Include standard library definitions
#include <util/delay.h>  // Include delay utilities


// Define pins for DATA, CLK, and FSYNC
#define DATA 3
#define CLK 5
#define FSYNC 2
//rotary encoder
#define PIN_A PC2  ////green
#define PIN_B PC0  ///grey
#define PIN_C PC3  ///red
//blue is for mode

// Declare a global variable to store the frequency


float _freq = 0;
uint64_t Factor = 0;

// Declare a global variables
int mode = 1;
int val = 0;
int x = 0;
int z = 0;
char buffer[100];
char buffer1[100];
char buffer2[100];
char buffer3[100];

// Function to write 16-bit data to the AD9833
void AD_write16(uint16_t data) {
  PORTB &= ~(1 << FSYNC);  // Pull FSYNC low to start communication
  SPDR = (data >> 8);      // Send the high byte
  while (!(SPSR & (1 << SPIF)))
    ;                    // Wait for transmission complete, checks status of SPIF
  SPDR = (data & 0xFF);  // Send the low byte
  while (!(SPSR & (1 << SPIF)))
    ;                     // Wait for transmission complete
  PORTB |= (1 << FSYNC);  // Pull FSYNC high to end communication
}

// Function to set the frequency of the AD9833
void AD_setFrequency(float freq) {
  _freq = freq;                                                // Store the frequency in the global variable
  uint32_t _regFreq = freq * 10.73742 + 0.5;                   // Calculate register value for frequency
  AD_write16(0x4000 | (uint16_t)(_regFreq & 0x3FFF));          // Write lower 14 bits of frequency
  AD_write16(0x4000 | (uint16_t)((_regFreq >> 14) & 0x3FFF));  // Write upper 14 bits of frequency
}

// Function to get the current frequency
const float AD_getFrequency() {
  return _freq;  // Return the stored frequency
}

// Function to set the phase of the AD9833
void AD_setPhase(uint16_t phase) {
  uint32_t _regPhase = (51.2 * phase) / 45 + 0.5;      // Calculate register value for phase
  AD_write16(0xC000 | (uint16_t)(_regPhase & 0xFFF));  // Write phase register
}


// Function to set the waveform mode of the AD9833
void AD_setMode(int wave) {
  switch (wave) {
    case 1:
      AD_write16(0x2000);  // Set to sine wave mode
      break;
    case 2:
      AD_write16(0x2028);  // Set to square wave mode
      break;
    case 3:
      AD_write16(0x2002);  // Set to triangle wave mode
      break;
  }
}

void AD_init(void) {
  DDRB = (1 << DATA) | (1 << CLK) | (1 << FSYNC);  // Set DATA, CLK, FSYNC as outputs
  PORTB &= ~(1 << CLK);                            // Set CLK low
  PORTB |= (1 << FSYNC);                           // Set FSYNC high
  SPCR = 0x59;                                     // Set SPI control register
  AD_write16(0x2100);                              // Reset AD9833
  AD_setFrequency(1000);                           // Set initial frequency to 1000 Hz
  AD_setPhase(0);                                  // Set initial phase to 0
}


//LCD
void i2c_init() {
  TWBR = 0x62;         //Setting bit rate
  TWCR = (1 << TWEN);  //Enable I2C
  TWSR = 0x00;         //Prescaler set to 1
}
//Start condition
void i2c_start() {
  TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA);
  //start condition
  while (!(TWCR & (1 << TWINT)))
    ;
  //check for start condition
}
void lcd_init() {
  _delay_ms(100);  //wait initialization
  lcd_cmd(0x30);   //-----Sequence for initializing LCD
  _delay_ms(10);
  lcd_cmd(0x30);
  _delay_ms(5);
  lcd_cmd(0x30);
  _delay_ms(5);
  lcd_cmd(0x02);
  _delay_ms(1);
  lcd_cmd(0x28);
  _delay_ms(1);
  lcd_cmd(0x0C);  //-----Display ON Cursor OFF
  lcd_cmd(0x06);  //-----Cursor Auto Increment
  lcd_cmd(0x01);  //-----Clear display
  _delay_ms(4);   //2msec delay
}

//I2C write (for sending address and data)
void i2c_write(char x) {
  TWDR = x;  //Move value to I2C
  TWCR = (1 << TWINT) | (1 << TWEN);
  //Enable I2C and clear interrupt
  while (!(TWCR & (1 << TWINT)))
    ;
}

//-----LCD 4-bit Mode functions using I2C-------//
void toggle() {
  i2c_write((TWDR |= 0x04));
  _delay_us(1);  //Enable pulse
  i2c_write((TWDR &= ~0x04));
  _delay_us(100);  //make pulse longer or wait
  //for at least 100usec after sending each command
}

void lcd_cmd(char v2) {
  i2c_write((TWDR &= ~0x03));
  i2c_write((TWDR &= 0x0F));
  i2c_write((TWDR |= (v2 & 0xF0)));
  toggle();

  i2c_write((TWDR &= 0x0F));
  i2c_write((TWDR |= ((v2 & 0x0F) << 4)));
  toggle();
}

void lcd_dwr(char v3) {
  i2c_write((TWDR |= 0x01));
  i2c_write((TWDR &= 0x0F));
  i2c_write((TWDR |= (v3 & 0xF0)));
  toggle();

  i2c_write((TWDR &= 0x0F));
  i2c_write((TWDR |= ((v3 & 0x0F) << 4)));
  toggle();
}

void lcd_msg(char *c) {
  while (*c != 0)   //Wait till all String are passed
    lcd_dwr(*c++);  //----Send the String to LCD
}
void intro() {
  lcd_cmd(0x80);  //Cursor start of Line 1
  lcd_msg("* Function *");
  lcd_cmd(0xC0);  //Move cursor to Line 2
  lcd_msg("* Generator *");
  _delay_us(2000000);
  lcd_cmd(0x01);
}

void lcd_gotoxy(unsigned char a, unsigned char b) {
  char firstCharAdr[] = { 0x80, 0xC0, 0x94, 0xD4 };  //Table
  lcd_cmd(firstCharAdr[b - 1] + a - 1);
  _delay_us(100);
}
// Main function
int main(void) {
  uint8_t adc_value;
  uint32_t frequency;

  // DDRD |= 0xFF;           // Set PORTB as output
  DDRC &= 0x00;  // Set PC3, PC4, PC5 as input

  i2c_init();   //initialize i2c
  i2c_start();  // start i2c
  i2c_write(0x4E);
  //01001110=4E
  //(Device Address)
  lcd_init();  //initialize LCD





  // converting float to string


  intro();
  AD_init();

  while (1) {

    if (PINC & 0b00000010) {  // Check if PC3 is high
      _delay_ms(50);          // Simple debounce delay
      // Only proceed if still pressed after delay
      if (PINC & 0b00000010) {
        x = (x + 1) % 4;  // Update x and cycle through 0-3

        switch (x) {
          case 0:
            mode = 1;
            AD_setMode(mode);
            lcd_cmd(0x80);     //Cursor start of Line 1
            lcd_msg("Wave:");  //Message
            lcd_gotoxy(9, 1);

            lcd_msg("    ");  //Message
            lcd_msg("Sine");  //Message


            break;
          case 1:
            mode = 2;
            AD_setMode(mode);

            lcd_cmd(0x80);     //Cursor start of Line 1
            lcd_msg("Wave:");  //Message
            lcd_gotoxy(9, 1);
            lcd_msg("  ");      //Message
            lcd_msg("Square");  //Message
            break;
          case 2:
            mode = 3;
            AD_setMode(mode);
            lcd_cmd(0x80);     //Cursor start of Line 1
            lcd_msg("Wave:");  //Message
            lcd_gotoxy(9, 1);
            lcd_msg("Triangle");  //Message
            break;
        }

        // Wait until the button is released
        while (PINC & 0b00001000)
          ;
      }
    }


    //////
    if (PINC & 0b00000001) {  // Check if PC3 is high
      _delay_ms(50);          // Simple debounce delay
      // Only proceed if still pressed after delay
      if (PINC & 0b00000001) {
        z = (z + 1) % 4;  // Update x and cycle through 0-3

        switch (z) {

          case 1:
            Factor = 1;


            lcd_cmd(0xC0);     //Move cursor to Line 2
            lcd_msg("Freq:");  //Message
            lcd_gotoxy(15, 2);
            lcd_msg("HZ");  //Message
            _freq = Factor;

            break;
          case 2:
            Factor = 1000;
            lcd_cmd(0xC0);     //Move cursor to Line 2
            lcd_msg("Freq:");  //Message

            lcd_gotoxy(15, 2);
            lcd_msg("KHZ");  //Message
            _freq = Factor;
            break;
          case 3:
            Factor = 1000000;

            lcd_cmd(0xC0);     //Move cursor to Line 2
            lcd_msg("Freq:");  //Message
            lcd_gotoxy(15, 2);
            lcd_msg("MHZ");  //Message
            _freq = Factor;
            break;
        }

        // Wait until the button is released
        while (PINC & 0b00001000)
          ;
      }
    }

    if (PINC & 0b00000100) {
      _delay_ms(50);  // Simple debounce delay
      // Only proceed if still pressed after delay

      if (PINC & 0b00000100) {

        if (_freq > 100000) {
          _freq += 100;
          lcd_cmd(0xC0);  //Move cursor to Line 2
          dtostrf(_freq, 8, 0, buffer2);
          lcd_gotoxy(7, 2);
          lcd_msg(buffer2);  //Message
          AD_setFrequency(_freq);
        }
        if (_freq > 999 && _freq < 100000) {
          _freq += 100;
          itoa(_freq, buffer, 10);
          lcd_cmd(0xC0);  //Move cursor to Line 2
          lcd_gotoxy(7, 2);
          lcd_msg("    ");  //Message
          lcd_gotoxy(11, 2);
          lcd_msg(buffer);  //Message
          AD_setFrequency(_freq);
        }
        if (_freq > 0 && _freq < 1000) {
          _freq += 100;
          itoa(_freq, buffer, 10);
          lcd_cmd(0xC0);  //Move cursor to Line 2
          lcd_gotoxy(7, 2);
          lcd_msg("     ");  //Message
          lcd_gotoxy(12, 2);
          lcd_msg(buffer);  //Message
          AD_setFrequency(_freq);
        }
        // Wait until the button is released
        while (PINC & 0b00000100)
          ;
      }
    }
    if (PINC & 0b00001000) {  // Check if PC3 is high
      _delay_ms(50);          // Simple debounce delay
                              // Only proceed if still pressed after delay
      if (PINC & 0b00001000) {
        if (_freq > 100000 && _freq > 1000000) {
          _freq -= 100;
          lcd_cmd(0xC0);  //Move cursor to Line 2
          dtostrf(_freq, 8, 0, buffer3);
          lcd_gotoxy(7, 2);
          lcd_msg(buffer3);  //Message
          AD_setFrequency(_freq);
        }
        if (_freq > 1000 && _freq < 100000) {
          _freq -= 100;
          itoa(_freq, buffer, 10);
          lcd_cmd(0xC0);  //Move cursor to Line 2
          lcd_gotoxy(7, 2);
          lcd_msg("    ");  //Message
          lcd_gotoxy(11, 2);
          lcd_msg(buffer);  //Message
          AD_setFrequency(_freq);
        }
        if (_freq > 10 && _freq < 1000) {
          _freq -= 100;
          itoa(_freq, buffer, 10);
          lcd_cmd(0xC0);  //Move cursor to Line 2
          lcd_gotoxy(7, 2);
          lcd_msg("     ");  //Message
          lcd_gotoxy(12, 2);
          lcd_msg(buffer);  //Message
          AD_setFrequency(_freq);
        }


        while (PINC & 0b00001000)
          ;
      }
    }
  }

  return 0;
}