

/* this function converts the ADC value into an Ohm value of the NTC
 * here AVcc=5V is used as reference voltage
 */
float adc_2_ohm(int adc_value)
{
	float ohm;

	// 10bit adc=0..1023 over a voltage range from 0-5V
	// Uref=5V = VDD
	// 10000 / ( 1024/adc_value  - 1) -> ohm
	//

	// ADCval= Uadc * (1024/Uref)
	// Rntc= 10K * ( 1 / ((5V/Uadc) -1) )
	if (adc_value < 1){
		// never divide by zero:
		adc_value=1;
	}
	ohm=NTC_PULLUP_R / (( 1024.0 / adc_value ) - 1 );
	return(ohm);
}


/* convert a ntc resistance value given in kohm to
 * temperature in celsius. The NTC follows a exponential
 * characteristic. */
float r2temperature(float ohm)
{
	float tcelsius;
	float tmp;


	tcelsius = 100;
	tmp=1;

	if ((ohm/NTC_RN) < 0.1){
		//ERROR: resistance value of NTC is too small (too hot here ;-)
		return(tcelsius);
	}
	tmp = ( (1.0/NTC_B) * log( (ohm/NTC_RN) ) ) + ( 1 / (NTC_TN+273.0) );

	tcelsius = (float) (1.0/tmp) -273;
	return(tcelsius);
}

