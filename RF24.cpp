/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include "nRF24L01.h"
#include "RF24_config.h"
#include "RF24.h"

/****************************************************************************/

void RF24::csn(int mode)
{
  // Minimum ideal SPI bus speed is 2x data rate
  // If we assume 2Mbs data rate and 16Mhz clock, a
  // divider of 4 is the minimum we want.
  // CLK:BUS 8Mhz:2Mhz, 16Mhz:4Mhz, or 20Mhz:5Mhz
#ifdef ARDUINO
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
#endif
  digitalWrite(csn_pin,mode);
}

/****************************************************************************/

void RF24::ce(int level)
{
  digitalWrite(ce_pin,level);
}

/****************************************************************************/

uint8_t RF24::read_register(uint8_t reg, uint8_t* buf, uint8_t len)
{
  uint8_t status;

  csn(LOW);
  status = SPI.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  while ( len-- )
    *buf++ = SPI.transfer(0xff);

  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::read_register(uint8_t reg)
{
  csn(LOW);
  SPI.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  uint8_t result = SPI.transfer(0xff);

  csn(HIGH);
  return result;
}

/****************************************************************************/

uint8_t RF24::write_register(uint8_t reg, const uint8_t* buf, uint8_t len)
{
  uint8_t status;

  csn(LOW);
  status = SPI.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  while ( len-- )
    SPI.transfer(*buf++);

  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::write_register(uint8_t reg, uint8_t value)
{
  uint8_t status;

  IF_SERIAL_DEBUG(printf_P(PSTR("write_register(%02x,%02x)\r\n"),reg,value));

  csn(LOW);
  status = SPI.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  SPI.transfer(value);
  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::write_payload(const void* buf, uint8_t len)
{
  uint8_t status;

  const uint8_t* current = reinterpret_cast<const uint8_t*>(buf);

  uint8_t data_len = min(len,payload_size);
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;

  //printf("[Writing %u bytes %u blanks]",data_len,blank_len);

  csn(LOW);
  status = SPI.transfer( W_TX_PAYLOAD );
  while ( data_len-- )
    SPI.transfer(*current++);
  while ( blank_len-- )
    SPI.transfer(0);
  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::read_payload(void* buf, uint8_t len)
{
  uint8_t status;
  uint8_t* current = reinterpret_cast<uint8_t*>(buf);

  uint8_t data_len = min(len,payload_size);
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;

  //printf("[Reading %u bytes %u blanks]",data_len,blank_len);

  csn(LOW);
  status = SPI.transfer( R_RX_PAYLOAD );
  while ( data_len-- )
    *current++ = SPI.transfer(0xff);
  while ( blank_len-- )
    SPI.transfer(0xff);
  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::flush_rx(void)
{
  uint8_t status;

  csn(LOW);
  status = SPI.transfer( FLUSH_RX );
  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::flush_tx(void)
{
  uint8_t status;

  csn(LOW);
  status = SPI.transfer( FLUSH_TX );
  csn(HIGH);

  return status;
}

/****************************************************************************/

uint8_t RF24::get_status(void)
{
  uint8_t status;

  csn(LOW);
  status = SPI.transfer( NOP );
  csn(HIGH);

  return status;
}

/****************************************************************************/

void RF24::print_status(uint8_t status)
{
  printf_P(PSTR("STATUS\t\t = 0x%02x RX_DR=%x TX_DS=%x MAX_RT=%x RX_P_NO=%x TX_FULL=%x\r\n"),
           status,
           (status & _BV(RX_DR))?1:0,
           (status & _BV(TX_DS))?1:0,
           (status & _BV(MAX_RT))?1:0,
           ((status >> RX_P_NO) & B111),
           (status & _BV(TX_FULL))?1:0
          );
}

/****************************************************************************/

void RF24::print_observe_tx(uint8_t value)
{
  printf_P(PSTR("OBSERVE_TX=%02x: POLS_CNT=%x ARC_CNT=%x\r\n"),
           value,
           (value >> PLOS_CNT) & B1111,
           (value >> ARC_CNT) & B1111
          );
}

/****************************************************************************/

void RF24::print_byte_register(const char* name, uint8_t reg, uint8_t qty)
{
  char extra_tab = strlen_P(name) < 8 ? '\t' : 0;
  printf_P(PSTR(PRIPSTR"\t%c ="),name,extra_tab);
  while (qty--)
    printf_P(PSTR(" 0x%02x"),read_register(reg++));
  printf_P(PSTR("\r\n"));
}

/****************************************************************************/

void RF24::print_address_register(const char* name, uint8_t reg, uint8_t qty)
{
  char extra_tab = strlen_P(name) < 8 ? '\t' : 0;
  printf_P(PSTR(PRIPSTR"\t%c ="),name,extra_tab);

  while (qty--)
  {
    uint8_t buffer[5];
    read_register(reg++,buffer,sizeof buffer);

    printf_P(PSTR(" 0x"));
    uint8_t* bufptr = buffer + sizeof buffer;
    while( --bufptr >= buffer )
      printf_P(PSTR("%02x"),*bufptr);
  }

  printf_P(PSTR("\r\n"));
}

/****************************************************************************/

RF24::RF24(uint8_t _cepin, uint8_t _cspin):
  ce_pin(_cepin), csn_pin(_cspin), wide_band(true), p_variant(false),
  payload_size(32), ack_payload_available(false), dynamic_payloads_enabled(false),
  pipe0_reading_address(0)
{
}

/****************************************************************************/

void RF24::setChannel(uint8_t channel)
{
  // TODO: This method could take advantage of the 'wide_band' calculation
  // done in setChannel() to require certain channel spacing.

  const uint8_t max_channel = 127;
  write_register(RF_CH,min(channel,max_channel));
}

/****************************************************************************/

void RF24::setPayloadSize(uint8_t size)
{
  const uint8_t max_payload_size = 32;
  payload_size = min(size,max_payload_size);
}

/****************************************************************************/

uint8_t RF24::getPayloadSize(void)
{
  return payload_size;
}

/****************************************************************************/

static const char rf24_datarate_e_str_0[] PROGMEM = "1MBPS";
static const char rf24_datarate_e_str_1[] PROGMEM = "2MBPS";
static const char rf24_datarate_e_str_2[] PROGMEM = "250KBPS";
static const char * const rf24_datarate_e_str_P[] PROGMEM = {
  rf24_datarate_e_str_0,
  rf24_datarate_e_str_1,
  rf24_datarate_e_str_2,
};
static const char rf24_model_e_str_0[] PROGMEM = "nRF24L01";
static const char rf24_model_e_str_1[] PROGMEM = "nRF24L01+";
static const char * const rf24_model_e_str_P[] PROGMEM = {
  rf24_model_e_str_0,
  rf24_model_e_str_1,
};
static const char rf24_crclength_e_str_0[] PROGMEM = "Disabled";
static const char rf24_crclength_e_str_1[] PROGMEM = "8 bits";
static const char rf24_crclength_e_str_2[] PROGMEM = "16 bits" ;
static const char * const rf24_crclength_e_str_P[] PROGMEM = {
  rf24_crclength_e_str_0,
  rf24_crclength_e_str_1,
  rf24_crclength_e_str_2,
};
static const char rf24_pa_dbm_e_str_0[] PROGMEM = "PA_MIN";
static const char rf24_pa_dbm_e_str_1[] PROGMEM = "PA_LOW";
static const char rf24_pa_dbm_e_str_2[] PROGMEM = "LA_MED";
static const char rf24_pa_dbm_e_str_3[] PROGMEM = "PA_HIGH";
static const char * const rf24_pa_dbm_e_str_P[] PROGMEM = {
  rf24_pa_dbm_e_str_0,
  rf24_pa_dbm_e_str_1,
  rf24_pa_dbm_e_str_2,
  rf24_pa_dbm_e_str_3,
};

void RF24::printDetails(void)
{
  print_status(get_status());

  print_address_register(PSTR("RX_ADDR_P0-1"),RX_ADDR_P0,2);
  print_byte_register(PSTR("RX_ADDR_P2-5"),RX_ADDR_P2,4);
  print_address_register(PSTR("TX_ADDR"),TX_ADDR);

  print_byte_register(PSTR("RX_PW_P0-6"),RX_PW_P0,6);
  print_byte_register(PSTR("EN_AA"),EN_AA);
  print_byte_register(PSTR("EN_RXADDR"),EN_RXADDR);
  print_byte_register(PSTR("RF_CH"),RF_CH);
  print_byte_register(PSTR("RF_SETUP"),RF_SETUP);
  print_byte_register(PSTR("CONFIG"),CONFIG);
  print_byte_register(PSTR("DYNPD/FEATURE"),DYNPD,2);

  printf_P(PSTR("Data Rate\t = %S\r\n"),pgm_read_word(&rf24_datarate_e_str_P[getDataRate()]));
  printf_P(PSTR("Model\t\t = %S\r\n"),pgm_read_word(&rf24_model_e_str_P[isPVariant()]));
  printf_P(PSTR("CRC Length\t = %S\r\n"),pgm_read_word(&rf24_crclength_e_str_P[getCRCLength()]));
  printf_P(PSTR("PA Power\t = %S\r\n"),pgm_read_word(&rf24_pa_dbm_e_str_P[getPALevel()]));
}

/****************************************************************************/

void RF24::begin(void)
{
  // Initialize pins
  pinMode(ce_pin,OUTPUT);
  pinMode(csn_pin,OUTPUT);

  // Initialize SPI bus
  SPI.begin();

  ce(LOW);
  csn(HIGH);

  // Must allow the radio time to settle else configuration bits will not necessarily stick.
  // This is actually only required following power up but some settling time also appears to
  // be required after resets too. For full coverage, we'll always assume the worst.
  // Enabling 16b CRC is by far the most obvious case if the wrong timing is used - or skipped.
  // Technically we require 4.5ms + 14us as a worst case. We'll just call it 5ms for good measure.
  // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.
  delay( 5 ) ;

  // Set 1500uS (minimum for 32B payload in ESB@250KBPS) timeouts, to make testing a little easier
  // WARNING: If this is ever lowered, either 250KBS mode with AA is broken or maximum packet
  // sizes must never be used. See documentation for a more complete explanation.
  write_register(SETUP_RETR,(B0100 << ARD) | (B1111 << ARC));

  // Restore our default PA level
  setPALevel( RF24_PA_MAX ) ;

  // Determine if this is a p or non-p RF24 module and then
  // reset our data rate back to default value. This works
  // because a non-P variant won't allow the data rate to
  // be set to 250Kbps.
  if( setDataRate( RF24_250KBPS ) )
  {
    p_variant = true ;
  }

  // Then set the data rate to the slowest (and most reliable) speed supported by all
  // hardware.
  setDataRate( RF24_1MBPS ) ;

  // Initialize CRC and request 2-byte (16bit) CRC
  setCRCLength( RF24_CRC_16 ) ;

  // Disable dynamic payloads, to match dynamic_payloads_enabled setting
  write_register(DYNPD,0);

  // Reset current status
  // Notice reset and flush is the last thing we do
  write_register(STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Set up default configuration.  Callers can always change it later.
  // This channel should be universally safe and not bleed over into adjacent
  // spectrum.
  setChannel(76);

  // Flush buffers
  flush_rx();
  flush_tx();

  //Enable PTX, do not write CE high so radio will remain in standby I mode ( 130us max to transition to RX or TX instead of 1500us from powerUp )
  write_register(CONFIG, ( read_register(CONFIG) ) & ~_BV(PRIM_RX) );


}

/****************************************************************************/

void RF24::startListening(void)
{
  write_register(CONFIG, read_register(CONFIG) | _BV(PWR_UP) | _BV(PRIM_RX));
  write_register(STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Restore the pipe0 adddress, if exists
  if (pipe0_reading_address)
    write_register(RX_ADDR_P0, reinterpret_cast<const uint8_t*>(&pipe0_reading_address), 5);

  // Flush buffers
  flush_rx();
  flush_tx();

  // Go!
  ce(HIGH);

}

/****************************************************************************/

void RF24::stopListening(void)
{
  ce(LOW);
  flush_tx();
  flush_rx();
  write_register(CONFIG, ( read_register(CONFIG) ) & ~_BV(PRIM_RX) );

}

/****************************************************************************/

void RF24::powerDown(void)
{
  write_register(CONFIG,read_register(CONFIG) & ~_BV(PWR_UP));
}

/****************************************************************************/

//Power up now. Radio will not power down unless instructed by MCU for config changes etc.
void RF24::powerUp(void)
{
  write_register(CONFIG,read_register(CONFIG) | _BV(PWR_UP));
}

/******************************************************************/

//Similar to the previous write, clears the interrupt flags
bool RF24::write( const void* buf, uint8_t len )
{
	//Start Writing
    startFastWrite(buf,len);

	//Wait until complete or failed
	//ACK payloads that are handled improperly will cause this to hang
	//If autoAck is ON, a payload has to be written prior to reading a payload, else write after reading a payload
	while( ! ( get_status()  & ( _BV(TX_DS) | _BV(MAX_RT) ))) { }

	ce(LOW); //Set the radio back to STANDBY-I mode since we can only fill one buffer at a time using this method, and tx is complete

	uint8_t status = write_register(STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  //Max retries exceeded
  if( status & _BV(MAX_RT)){
  	flush_tx(); //Only going to be 1 packet int the FIFO at a time using this method, so just flush
  	return 0;
  }
	//TX OK 1 or 0
  return 1;
}

/****************************************************************************/

//For general use, the interrupt flags are not important to clear
bool RF24::writeBlocking( const void* buf, uint8_t len, unsigned long timeout )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//This way the FIFO will fill up and allow blocking until packets go through
	//The radio will auto-clear everything in the FIFO as long as CE remains high

	unsigned long timer = millis();							  //Get the time that the payload transmission started

	while ( (read_register(FIFO_STATUS) & _BV(FIFO_FULL))){   //Blocking only if FIFO is full. This will loop and block until TX is successful

		if( get_status() & _BV(MAX_RT)){					  //If MAX Retries have been reached
			reUseTX();										  //Set re-transmit and clear the MAX_RT interrupt flag
		}
		if(millis() - timer > timeout){ return 0; }			  //If this payload has exceeded the user-defined timeout, exit and return 0
  	}

  	//Start Writing
	startFastWrite(buf,len);								  //Write the payload if a buffer is clear

	return 1;												  //Return 1 to indicate successful transmission
}


void RF24::reUseTX(){
		write_register(STATUS,_BV(MAX_RT) );			  //Clear max retry flag
		csn(LOW);
  		SPI.transfer( REUSE_TX_PL );
  		csn(HIGH);
		ce(LOW);										  //Re-Transfer packet
		ce(HIGH);
}

/****************************************************************************/

bool RF24::writeFast( const void* buf, uint8_t len )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//Return 0 so the user can control the retrys and set a timer or failure counter if required
	//The radio will auto-clear everything in the FIFO as long as CE remains high


	while ( (read_register(FIFO_STATUS) & _BV(FIFO_FULL))){   //Blocking only if FIFO is full. This will loop and block until TX is successful

		if( get_status() & _BV(MAX_RT)){
			reUseTX();										  //Set re-transmit
			return 0;										  //Return 0. The previous payload has been retransmitted
															  //From the user perspective, if you get a 0, just keep trying to send the same payload
		}

  	}
		     //Start Writing
	startFastWrite(buf,len);

	return 1;
}



/****************************************************************************/

//Per the documentation, we want to set PTX Mode when not listening. Then all we do is write data and set CE high
//In this mode, if we can keep the FIFO buffers loaded, packets will transmit immediately (no 130us delay)
//Otherwise we enter Standby-II mode, which is still faster than standby mode
//Also, we remove the need to keep writing the config register over and over and delaying for 150 us each time if sending a stream of data

void RF24::startFastWrite( const void* buf, uint8_t len ){ //TMRh20

	write_payload( buf,len);
	ce(HIGH);

}


//Added the original startWrite back in so users can still use interrupts, ack payloads, etc
//Allows the library to pass all tests
void RF24::startWrite( const void* buf, uint8_t len )
{
  // Transmitter power-up
  write_register(CONFIG, ( read_register(CONFIG) | _BV(PWR_UP) ) & ~_BV(PRIM_RX) );
  delayMicroseconds(150);

  // Send the payload
  write_payload( buf, len );

  // Allons!
  ce(HIGH);
  delayMicroseconds(15);
  ce(LOW);
}

bool RF24::txStandBy(){
	while( ! (read_register(FIFO_STATUS) & _BV(TX_EMPTY)) ){
		if( get_status() & _BV(MAX_RT)){
			write_register(STATUS,_BV(MAX_RT) );
			flush_tx();    //Non blocking, flush the data
			ce(LOW);	   //Set STANDBY-I mode
			return 0;
		}
	}

	ce(LOW);				   //Set STANDBY-I mode
	return 1;
}

bool RF24::txStandBy(unsigned long timeout){

	unsigned long start = millis();

	while( ! (read_register(FIFO_STATUS) & _BV(TX_EMPTY)) ){
		if( get_status() & _BV(MAX_RT)){
			write_register(STATUS,_BV(MAX_RT) );
			if(timeout > 0){
				ce(LOW);										  //Set re-transmit
				ce(HIGH);
				if(millis() - start > timeout){ ce(LOW); flush_tx(); return 0; }

			}
		}
	}
	ce(LOW);				   //Set STANDBY-I mode
	return 1;

}

/****************************************************************************/

uint8_t RF24::getDynamicPayloadSize(void)
{
  uint8_t result = 0;

  csn(LOW);
  SPI.transfer( R_RX_PL_WID );
  result = SPI.transfer(0xff);
  csn(HIGH);

  if(result > 32) { flush_rx(); return 0; }
  return result;
}

/****************************************************************************/

bool RF24::available(void)
{
  return available(NULL);
}

/****************************************************************************/

bool RF24::available(uint8_t* pipe_num)
{

  //Check the FIFO buffer to see if data is waitng to be read

  if (!( read_register(FIFO_STATUS) & _BV(RX_EMPTY) )){

    // If the caller wants the pipe number, include that
    if ( pipe_num ){
	  uint8_t status = get_status();
      *pipe_num = ( status >> RX_P_NO ) & B111;
  	}
  	return 1;
  }

  return 0;



}

/****************************************************************************/

void RF24::read( void* buf, uint8_t len ){

  // Fetch the payload
  read_payload( buf, len );

  //Clear the two possible interrupt flags with one command
  write_register(STATUS,_BV(RX_DR) | _BV(MAX_RT) | _BV(TX_DS) );

}

/****************************************************************************/

void RF24::whatHappened(bool& tx_ok,bool& tx_fail,bool& rx_ready)
{
  // Read the status & reset the status in one easy call
  // Or is that such a good idea?
  uint8_t status = write_register(STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Report to the user what happened
  tx_ok = status & _BV(TX_DS);
  tx_fail = status & _BV(MAX_RT);
  rx_ready = status & _BV(RX_DR);
}

/****************************************************************************/

void RF24::openWritingPipe(uint64_t value)
{
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.

  write_register(RX_ADDR_P0, reinterpret_cast<uint8_t*>(&value), 5);
  write_register(TX_ADDR, reinterpret_cast<uint8_t*>(&value), 5);

  const uint8_t max_payload_size = 32;
  write_register(RX_PW_P0,min(payload_size,max_payload_size));
}

/****************************************************************************/

static const uint8_t child_pipe[] PROGMEM =
{
  RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5
};
static const uint8_t child_payload_size[] PROGMEM =
{
  RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5
};
static const uint8_t child_pipe_enable[] PROGMEM =
{
  ERX_P0, ERX_P1, ERX_P2, ERX_P3, ERX_P4, ERX_P5
};

void RF24::openReadingPipe(uint8_t child, uint64_t address)
{
  // If this is pipe 0, cache the address.  This is needed because
  // openWritingPipe() will overwrite the pipe 0 address, so
  // startListening() will have to restore it.
  if (child == 0)
    pipe0_reading_address = address;

  if (child <= 6)
  {
    // For pipes 2-5, only write the LSB
    if ( child < 2 )
      write_register(pgm_read_byte(&child_pipe[child]), reinterpret_cast<const uint8_t*>(&address), 5);
    else
      write_register(pgm_read_byte(&child_pipe[child]), reinterpret_cast<const uint8_t*>(&address), 1);

    write_register(pgm_read_byte(&child_payload_size[child]),payload_size);

    // Note it would be more efficient to set all of the bits for all open
    // pipes at once.  However, I thought it would make the calling code
    // more simple to do it this way.
    write_register(EN_RXADDR,read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[child])));
  }
}

/****************************************************************************/

void RF24::toggle_features(void)
{
  csn(LOW);
  SPI.transfer( ACTIVATE );
  SPI.transfer( 0x73 );
  csn(HIGH);
}

/****************************************************************************/

void RF24::enableDynamicPayloads(void)
{
  // Enable dynamic payload throughout the system
  write_register(FEATURE,read_register(FEATURE) | _BV(EN_DPL) );

  // If it didn't work, the features are not enabled
  if ( ! read_register(FEATURE) )
  {
    // So enable them and try again
    toggle_features();
    write_register(FEATURE,read_register(FEATURE) | _BV(EN_DPL) );
  }

  IF_SERIAL_DEBUG(printf("FEATURE=%i\r\n",read_register(FEATURE)));

  // Enable dynamic payload on all pipes
  //
  // Not sure the use case of only having dynamic payload on certain
  // pipes, so the library does not support it.
  write_register(DYNPD,read_register(DYNPD) | _BV(DPL_P5) | _BV(DPL_P4) | _BV(DPL_P3) | _BV(DPL_P2) | _BV(DPL_P1) | _BV(DPL_P0));

  dynamic_payloads_enabled = true;
}

/****************************************************************************/

void RF24::enableAckPayload(void)
{
  //
  // enable ack payload and dynamic payload features
  //

  write_register(FEATURE,read_register(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL) );

  // If it didn't work, the features are not enabled
  if ( ! read_register(FEATURE) )
  {
    // So enable them and try again
    toggle_features();
    write_register(FEATURE,read_register(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL) );
  }

  IF_SERIAL_DEBUG(printf("FEATURE=%i\r\n",read_register(FEATURE)));

  //
  // Enable dynamic payload on pipes 0 & 1
  //

  write_register(DYNPD,read_register(DYNPD) | _BV(DPL_P1) | _BV(DPL_P0));
}

/****************************************************************************/

void RF24::writeAckPayload(uint8_t pipe, const void* buf, uint8_t len)
{
  const uint8_t* current = reinterpret_cast<const uint8_t*>(buf);

  csn(LOW);
  SPI.transfer( W_ACK_PAYLOAD | ( pipe & B111 ) );
  const uint8_t max_payload_size = 32;
  uint8_t data_len = min(len,max_payload_size);
  while ( data_len-- )
    SPI.transfer(*current++);

  csn(HIGH);
}

/****************************************************************************/

bool RF24::isAckPayloadAvailable(void)
{
  if (!( read_register(FIFO_STATUS) & _BV(RX_EMPTY) )){
	  return 1;
  }
  return 0;
}

/****************************************************************************/

bool RF24::isPVariant(void)
{
  return p_variant ;
}

/****************************************************************************/

void RF24::setAutoAck(bool enable)
{
  if ( enable )
    write_register(EN_AA, B111111);
  else
    write_register(EN_AA, 0);
}

/****************************************************************************/

void RF24::setAutoAck( uint8_t pipe, bool enable )
{
  if ( pipe <= 6 )
  {
    uint8_t en_aa = read_register( EN_AA ) ;
    if( enable )
    {
      en_aa |= _BV(pipe) ;
    }
    else
    {
      en_aa &= ~_BV(pipe) ;
    }
    write_register( EN_AA, en_aa ) ;
  }
}

/****************************************************************************/

bool RF24::testCarrier(void)
{
  return ( read_register(CD) & 1 );
}

/****************************************************************************/

bool RF24::testRPD(void)
{
  return ( read_register(RPD) & 1 ) ;
}

/****************************************************************************/

void RF24::setPALevel(rf24_pa_dbm_e level)
{
  uint8_t setup = read_register(RF_SETUP) ;
  setup &= ~(_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;

  // switch uses RAM (evil!)
  if ( level == RF24_PA_MAX )
  {
    setup |= (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;
  }
  else if ( level == RF24_PA_HIGH )
  {
    setup |= _BV(RF_PWR_HIGH) ;
  }
  else if ( level == RF24_PA_LOW )
  {
    setup |= _BV(RF_PWR_LOW);
  }
  else if ( level == RF24_PA_MIN )
  {
    // nothing
  }
  else if ( level == RF24_PA_ERROR )
  {
    // On error, go to maximum PA
    setup |= (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;
  }

  write_register( RF_SETUP, setup ) ;
}

/****************************************************************************/

rf24_pa_dbm_e RF24::getPALevel(void)
{
  rf24_pa_dbm_e result = RF24_PA_ERROR ;
  uint8_t power = read_register(RF_SETUP) & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;

  // switch uses RAM (evil!)
  if ( power == (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) )
  {
    result = RF24_PA_MAX ;
  }
  else if ( power == _BV(RF_PWR_HIGH) )
  {
    result = RF24_PA_HIGH ;
  }
  else if ( power == _BV(RF_PWR_LOW) )
  {
    result = RF24_PA_LOW ;
  }
  else
  {
    result = RF24_PA_MIN ;
  }

  return result ;
}

/****************************************************************************/

bool RF24::setDataRate(rf24_datarate_e speed)
{
  bool result = false;
  uint8_t setup = read_register(RF_SETUP) ;

  // HIGH and LOW '00' is 1Mbs - our default
  wide_band = false ;
  setup &= ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH)) ;
  if( speed == RF24_250KBPS )
  {
    // Must set the RF_DR_LOW to 1; RF_DR_HIGH (used to be RF_DR) is already 0
    // Making it '10'.
    wide_band = false ;
    setup |= _BV( RF_DR_LOW ) ;
  }
  else
  {
    // Set 2Mbs, RF_DR (RF_DR_HIGH) is set 1
    // Making it '01'
    if ( speed == RF24_2MBPS )
    {
      wide_band = true ;
      setup |= _BV(RF_DR_HIGH);
    }
    else
    {
      // 1Mbs
      wide_band = false ;
    }
  }
  write_register(RF_SETUP,setup);

  // Verify our result
  if ( read_register(RF_SETUP) == setup )
  {
    result = true;
  }
  else
  {
    wide_band = false;
  }

  return result;
}

/****************************************************************************/

rf24_datarate_e RF24::getDataRate( void )
{
  rf24_datarate_e result ;
  uint8_t dr = read_register(RF_SETUP) & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

  // switch uses RAM (evil!)
  // Order matters in our case below
  if ( dr == _BV(RF_DR_LOW) )
  {
    // '10' = 250KBPS
    result = RF24_250KBPS ;
  }
  else if ( dr == _BV(RF_DR_HIGH) )
  {
    // '01' = 2MBPS
    result = RF24_2MBPS ;
  }
  else
  {
    // '00' = 1MBPS
    result = RF24_1MBPS ;
  }
  return result ;
}

/****************************************************************************/

void RF24::setCRCLength(rf24_crclength_e length)
{
  uint8_t config = read_register(CONFIG) & ~( _BV(CRCO) | _BV(EN_CRC)) ;

  // switch uses RAM (evil!)
  if ( length == RF24_CRC_DISABLED )
  {
    // Do nothing, we turned it off above.
  }
  else if ( length == RF24_CRC_8 )
  {
    config |= _BV(EN_CRC);
  }
  else
  {
    config |= _BV(EN_CRC);
    config |= _BV( CRCO );
  }
  write_register( CONFIG, config ) ;
}

/****************************************************************************/

rf24_crclength_e RF24::getCRCLength(void)
{
  rf24_crclength_e result = RF24_CRC_DISABLED;
  uint8_t config = read_register(CONFIG) & ( _BV(CRCO) | _BV(EN_CRC)) ;

  if ( config & _BV(EN_CRC ) )
  {
    if ( config & _BV(CRCO) )
      result = RF24_CRC_16;
    else
      result = RF24_CRC_8;
  }

  return result;
}

/****************************************************************************/

void RF24::disableCRC( void )
{
  uint8_t disable = read_register(CONFIG) & ~_BV(EN_CRC) ;
  write_register( CONFIG, disable ) ;
}

/****************************************************************************/
void RF24::setRetries(uint8_t delay, uint8_t count)
{
 write_register(SETUP_RETR,(delay&0xf)<<ARD | (count&0xf)<<ARC);
}

// vim:ai:cin:sts=2 sw=2 ft=cpp

