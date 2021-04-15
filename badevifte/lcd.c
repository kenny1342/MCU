

// LCD FUNCTIONS =================================

void LCD_Init ( void )
{
	LCD_SetData ( 0x00 );
	delay_ms ( 200 );       // wait enough time after Vdd rise
	output_low ( LCD_RS );
	LCD_SetData ( 0x03 );   // init with specific nibbles to start 4-bit mode
	LCD_PulseEnable();
	LCD_PulseEnable();
	LCD_PulseEnable();
	LCD_SetData ( 0x02 );   // set 4-bit interface
	LCD_PulseEnable();      // send dual nibbles hereafter, MSN first
	LCD_PutCmd ( 0x2C );    // function set (all lines, 5x7 characters)
	LCD_PutCmd ( 0x0C );    // display ON, cursor off, no blink
	LCD_PutCmd ( 0x01 );    // clear display
	LCD_PutCmd ( 0x06 );    // entry mode set, increment
}

void LCD_SetPosition ( unsigned int cX )
{
	// this subroutine works specifically for 4-bit Port A
	LCD_SetData ( swap ( cX ) | 0x08 );
	LCD_PulseEnable();
	LCD_SetData ( swap ( cX ) );
	LCD_PulseEnable();
}

void LCD_PutChar ( unsigned int cX )
{
	// this subroutine works specifically for 4-bit Port A
	output_high ( LCD_RS );
	LCD_SetData ( swap ( cX ) );     // send high nibble
	LCD_PulseEnable();
	LCD_SetData ( swap ( cX ) );     // send low nibble
	LCD_PulseEnable();
	output_low ( LCD_RS );
}

void LCD_PutCmd ( unsigned int cX )
{
	// this subroutine works specifically for 4-bit Port A
	LCD_SetData ( swap ( cX ) );     // send high nibble
	LCD_PulseEnable();
	LCD_SetData ( swap ( cX ) );     // send low nibble
	LCD_PulseEnable();
}

void LCD_PulseEnable ( void )
{
	output_high ( LCD_EN );
	delay_us ( 10 );
	output_low ( LCD_EN );
	delay_ms ( 5 );
}

void LCD_SetData ( unsigned int cX )
{
	output_bit ( LCD_D0, cX & 0x01 );
	output_bit ( LCD_D1, cX & 0x02 );
	output_bit ( LCD_D2, cX & 0x04 );
	output_bit ( LCD_D3, cX & 0x08 );
}



