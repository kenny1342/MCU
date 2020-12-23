#include "18F1320.h"
/* BUGS
- FIXED - slår av og på rele, mens de er på
- FIXED - bytt max_run_min til int16
- FIXED - ikke sensor_fault når åpen krets

TODO's
- Prøv å få et system i green/amber led håndtering
- Flytt mer vars til structs
- Revurder hvordan debug blir gjort
- Add blinkrate on red led
- FIXED - Add supercool, kjør kjøl til 0C (kun om innenfor max_run_min, evt juster denne under denne modusen)
- FIXED - Add deepfreeze, kjør frys ned til -26 (kun om innenfor max_run_min, evt juster denne under denne modusen)
- FIXED - Add #IFDEF DEBUG på alle debug statements
- FIXED - Add #IFDEF DEBUG på aktuelle defines som har med timing
- FIXED - Få switches mer responsive også med delay's i koden


*/
/****** LEDS *************
com=low and DL1=high: green
com=high and DL1/DL2=low: orange
com=low and DL3=high: red
- com usually low = green
***************************/

/****** SWITCHES *************
Alltid lav på en side
For å teste SW2:
- Kjør B2 høy
***************************/


//#include <math.h>
#include "main.h"


#int_TIMER0
void  TIMER0_isr(void) // OF every 1ms
{
    
    
    if(++ISR_counter0 == 60000) // 60 sec
    {        
        if(!fridge_running) fridge_stopped_mins++; else fridge_stopped_mins = 0;
        if(!freezer_running) freezer_stopped_mins++; else freezer_stopped_mins = 0;            

        if(fridge_running) fridge_run_mins++; else fridge_run_mins = 0;
        if(freezer_running) freezer_run_mins++; else freezer_run_mins = 0;

        ISR_counter0 = 0;
    }

    if(!sampling)
    {
        disable_interrupts(INT_TIMER2); // No LED controlling on PORTB now!

        set_tris_b(TRISB_SAMPLE_SWITCHES); 
        
        delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        if(bit_test(PORTB, 5))
        {             
            SW_fridge_time = 0;
        }
        else 
        {
                    
            SW_fridge_time++; 
            if(SW_fridge_time < 500)
                mode_supercool = !mode_supercool;
                
            if(SW_fridge_time > 2000 && SW_fridge_time < 4000)
                reset_cpu();
        }
                        
        if(bit_test(PORTB, 2))
        {             
            SW_freezer_time = 0;
        }
        else 
        {
                    
            SW_fridge_time++; 
            if(SW_freezer_time < 500)
                mode_deepfreeze = !mode_deepfreeze;
                
            if(SW_freezer_time > 2000 && SW_freezer_time < 4000)
                reset_cpu();
        }
        
                
        set_tris_b(TRISB_NORMAL); 
        enable_interrupts(INT_TIMER2);

    }          
    
}

#int_TIMER1
void  TIMER1_isr(void) // OF 524ms
{
        
    if(++ISR_counter1>=255) ISR_counter1 = 0;
    
    if((ISR_counter1 % 2) == 0) // ~every 1 sec
    {    
        
        do_read_temp = 1;
        do_actions = 1;                  
    }    
}

// This Timer ONLY deals with LEDs, because it's disabled/enabled frequently, for reading AD and switches
#int_TIMER2
void  TIMER2_isr(void)  // OF 1ms
{
    
    if(++ISR_counter2 == 1000) // 1 sec
    {
        ISR_counter2 = 0;
        blink_toggle ^=1;           
    }    
    
    output_bit(LED_COM, LEDs.COM);
    
    /*// TEST
    output_bit(LED_COM, AMBER);
    output_bit(LED_DL1, 0);
    output_bit(LED_DL2, 0);
    output_bit(LED_COM, GREEN);
    output_bit(LED_DL1, 0);
    output_bit(LED_DL2, 1);
*/
    
    if(LEDs.COM == AMBER)
    {
        if(LEDs.DL1 == BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, !LEDs.DL1);
        if(LEDs.DL2 == BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, !LEDs.DL2);
        if(LEDs.DL3 == BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, !LEDs.DL3);
    }
    else
    {    
        if(LEDs.DL1 == BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, LEDs.DL1);
        if(LEDs.DL2 == BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, LEDs.DL2);
        if(LEDs.DL3 == BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
    }            
    
}

#int_TIMER3
void  TIMER3_isr(void) // 65.5ms
{
//do_read_temp = 1;
}

#zero_ram
void main()
{

   
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_BIT); // ISR 1ms
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
   //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
   setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
   setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // OF 65,5ms
   
          
   
//   setup_adc_ports(sAN4|sAN5|VSS_VDD); // Not the POTs
   setup_adc_ports(sAN1|sAN4|sAN5|sAN6|VSS_VDD); // Inc the POTs   
   //setup_adc(ADC_CLOCK_INTERNAL|ADC_TAD_MUL_0);
   
   //setup_adc(ADC_CLOCK_DIV_32|ADC_TAD_MUL_20);
   setup_adc(ADC_CLOCK_DIV_64|ADC_TAD_MUL_20);
   
   setup_ccp1(CCP_OFF);
   port_b_pullups(FALSE);   
   
   enable_interrupts(INT_TIMER0);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_TIMER2);
   enable_interrupts(INT_TIMER3);   
   enable_interrupts(GLOBAL);
   
     
  LEDs.COM = GREEN; 
  Stop_Fridge_Motor;
  Stop_Freezer_Motor;
  fridge_stopped_mins = FRIDGE_MIN_STOP_MIN; // instant turn-on at power on
  freezer_stopped_mins = FREEZER_MIN_STOP_MIN; // instant turn-on at power on
  do_actions = 1;
    
  DisableSensors; 
  set_tris_a(TRISA_NORMAL); 
  set_tris_b(TRISB_NORMAL); 
  
    
  #ifdef DEBUG strcpy(s_debug, "Startup complete"); debug(); #endif
     
  while(TRUE)
  {
   
    if(do_read_temp && !sampling)
    {        
        
        do_read_temp = 0;
        readTemp(); 
        readPots();     
        doDecision();
        
        #ifdef DEBUG
        //sprintf(s_debug, "SW2: %u, SW3: %u", SW_fridge_pressed, SW_freezer_pressed); debug();
        sprintf(s_debug, "mode_supercool: %u, mode_deepfreeze: %u", mode_supercool, mode_deepfreeze); debug();
        sprintf(s_debug, "started/stopped freezer/fridge: %u/%u, %u/%u", started1, stopped1-1, started2, stopped2-1); debug();
        //sprintf(s_debug, "started/stopped fridge: %u, %u", started2, stopped2-1); debug();
        #endif
    }
                    
            
        
  }

}

/**************** Logic/Decisions *************************/            
void doDecision(void)
{
    if(Temps.fridge == 0 && Temps.freezer == 0) return; // not fully initialized yet
    
    /******* Logic for start/stop/keep running the FREEZER *******/    
    
    if(Temps.pot_freezer == POT_OFF) // pot turned all off
    {
        run_freezer = NO;
    }    
    else if(freezer_run_mins >= FREEZER_MAX_RUN_MIN) // run too long
    {        
        run_freezer = NO;
        LEDs.DL1=BLINK;
        //#ifdef DEBUG sprintf(s_debug, "freezer_run_mins > FREEZER_MAX_RUN_MIN: %u > %u", freezer_run_mins, FREEZER_MAX_RUN_MIN); debug(); #endif
    }    
    else if(Alarms.freezer_sensor_fault)
    {
        run_freezer = NO;
        LEDs.DL1=BLINK;
    }    
    else if( (Temps.freezer > (Temps.pot_freezer + FREEZER_MAX_DIFF) || Temps.freezer_to_warm) ) 
    {
        run_freezer = YES;        
    }    
    else
    {
        run_freezer = NO;
    }
        
    if(freezer_running && !run_freezer) 
        if(freezer_run_mins < FREEZER_MIN_RUN_MIN)
            run_freezer = YES; // abort stop if not run minimum time
        
    
    if(!freezer_running && run_freezer) // we are supposed to start
        if(freezer_stopped_mins < FREEZER_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            run_freezer = NO; // abort start            
        }         
    
    if(run_freezer) LEDs.DL1=ON;
    
    /******************** Deepfreeze might override *******************************/
    if(run_freezer == NO && mode_deepfreeze)
    {
        if(Temps.freezer > M_DEEPFREEZE_TEMP)
        {
            if(freezer_run_mins <= FREEZER_MAX_RUN_MIN*2) // still under max runtime * 2
            {
                run_freezer = YES;   
                LEDs.DL1=ON;
                #ifdef DEBUG sprintf(s_debug, "Deepfreeze mode, force running"); debug(); #endif
            }
            else
            {                
                #ifdef DEBUG sprintf(s_debug, "Failed to reach Deepfreeze temp on given time!"); debug(); #endif
                mode_deepfreeze = OFF; // SHOULD WE DO THIS ? OR KEEP RUNNING AFTER THE BREAK ?
            }        
        }
        else // reached deepfreeze temp
        {
            mode_deepfreeze = OFF;
        }            
    }    

    
    
    
    if(Temps.freezer > FREEZER_MAX_TEMP)
        Alarms.freezer_temp = ON;
    else
        Alarms.freezer_temp = OFF;

    /******* Logic for start/stop/keep running the FRIDGE *******/
    // TODO: USE FRXXXX_TO_COLD
    
    
    if(Temps.pot_fridge == POT_OFF) // pot turned all off
    {
        run_fridge = NO;
    }    
    else if(fridge_run_mins >= FRIDGE_MAX_RUN_MIN) // run too long
    {
        run_fridge = NO;
        LEDs.DL2=BLINK;
        //#ifdef DEBUG sprintf(s_debug, "fridge_run_mins > FRIDGE_MAX_RUN_MIN: %u > %u", fridge_run_mins, FRIDGE_MAX_RUN_MIN); debug(); #endif
    }    
    else if(Alarms.fridge_sensor_fault)
    {
        run_fridge = NO;
        LEDs.DL2=BLINK;
    }    
    else if( (Temps.fridge > (Temps.pot_fridge + FRIDGE_MAX_DIFF) || Temps.fridge_to_warm) ) 
    {
        run_fridge = YES;        
    }    
    else
    {
        run_fridge = NO;
    }
         
    if(fridge_running && !run_fridge) 
        if(fridge_run_mins < FRIDGE_MIN_RUN_MIN) //if(fridge_stopped_mins < FRIDGE_MIN_RUN_MIN)
            run_fridge = YES; // abort stop if not run minimum time
        
 
    if(!fridge_running && run_fridge) // we are supposed to start
        if(fridge_stopped_mins < FRIDGE_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            run_fridge = NO; // abort start                        
        }         
    
    if(run_fridge) LEDs.DL2=ON;
        
    /********* SuperCool might override ******************/
    if(run_fridge == NO && mode_supercool)
    {
        if(Temps.fridge > M_SUPERCOOL_TEMP)
        {
            if(fridge_run_mins <= FRIDGE_MAX_RUN_MIN*2) // still under max runtime * 2
            {
                run_fridge = YES;
                LEDs.DL2=ON;   
                #ifdef DEBUG sprintf(s_debug, "Supercool mode, force running"); debug(); #endif
            }
            else
            {                
                #ifdef DEBUG sprintf(s_debug, "Failed to reach Supercool temp on given time!"); debug(); #endif
                mode_supercool = OFF; // SHOULD WE DO THIS ? OR KEEP RUNNING AFTER THE BREAK ?
            }        
        }
        else // reached Supercool temp
        {
            mode_supercool = OFF;
        }            
    }    

    

    if(Temps.fridge > FRIDGE_MAX_TEMP)
        Alarms.fridge_temp = ON;
    else
        Alarms.fridge_temp = OFF;
        
        
        

/********************* Alarming ********************************************/           
    if(Alarms.fridge_temp || Alarms.freezer_temp)
        LEDs.DL3 = BLINK;    
    else if(Alarms.fridge_sensor_fault || Alarms.freezer_sensor_fault)
        LEDs.DL3 = ON;    
    else    
        LEDs.DL3 = OFF;    

    if(do_actions) doActions();
    
}    

/**************** Actions *************************/            
void doActions(void)
{
    //if(!do_actions) return;    
    
    do_actions = NO;
    
// TEST
//run_fridge = YES;
//run_freezer = YES;

    
    if(run_fridge && !fridge_running) 
    {           
        Start_Fridge_Motor;
        LEDs.DL2=ON;
        #ifdef DEBUG sprintf(s_debug, "Starting fridge"); debug(); #endif
    }    
    
    if(!run_fridge && fridge_running)
    {
        Stop_Fridge_Motor;
        LEDs.DL2=OFF;
        fridge_stopped_mins = 0;        
        #ifdef DEBUG sprintf(s_debug, "Stopping fridge"); debug();  #endif
    }    

    if(run_freezer && !freezer_running) 
    {           
        Start_Freezer_Motor;
        LEDs.DL1=ON;
        #ifdef DEBUG sprintf(s_debug, "Starting freezer"); debug();       #endif
    }    
    
    if(!run_freezer && freezer_running) 
    {
        Stop_Freezer_Motor;
        LEDs.DL1=OFF;
        freezer_stopped_mins = 0;
        #ifdef DEBUG sprintf(s_debug, "Stopping freezer"); debug();       #endif
    }
    
}
    
/**
* R synker ved varme, må være NTC basert.
* samples:
* 1,4 = 8,2K
* 1 = 9K
* -17 = 21,4K
* -18 = 22K
* 
*/
void readTemp(void)
{
    unsigned int16 adc_val;
            
    EnableSensors;
    set_tris_a(TRISA_SAMPLE_SENSORS); 
    set_tris_b(TRISB_SAMPLE_SENSORS); 
           
    adc_val = getAvgADC(ADC_FRIDGE, 100);
    fridge_avg += adc_val;    

    delay_ms(10);    
    
    adc_val = getAvgADC(ADC_FREEZER, 100);
    freezer_avg += adc_val;
        
    if(++freezer_avg_count == SAMPLE_COUNT)
    {
        freezer_avg_count = 0;        
        freezer_avg /= SAMPLE_COUNT;
        
        #ifdef DEBUG
        sprintf(s_debug, "--------- FRYS START ------"); debug();    
        //sprintf(s_debug, "----FREEZE ad_val: %Lu ----", freezer_avg); debug();          
        #endif
        
        Temps.freezer = SensorADToDegrees(freezer_avg, NO);
        
        if(freezer_running) 
            Temps.freezer += 4; //7 pga sensor plassert i grillene
        
        else
            Temps.freezer += 1; // avg når grillen tempereres
            
        #ifdef DEBUG
        sprintf(s_debug, "Freezer Temp/AVG, Stop/Run-mins: %d/%Lu, %u/%Lu", Temps.freezer, freezer_avg, freezer_stopped_mins, freezer_run_mins); debug();                   
        sprintf(s_debug, "--------- FRYS STOPP ------"); debug();    
        #endif
        freezer_avg = 0;
    }    


    if(++fridge_avg_count == SAMPLE_COUNT)
    {
        fridge_avg_count = 0;        
        fridge_avg /= SAMPLE_COUNT;

        sprintf(s_debug, "--------- KJØL START ------"); debug();    
        //sprintf(s_debug, "----KJØL ad_val: %Lu----", fridge_avg); debug();            

        Temps.fridge = SensorADToDegrees(fridge_avg, YES);
        
        Temps.fridge -= 9; // pga sensor plassert på varmeste sted...
                
        
        #ifdef DEBUG
        sprintf(s_debug, "Fridge Temp/AVG, Stop/Run-mins: %d/%Lu, %u/%Lu", Temps.fridge, fridge_avg, fridge_stopped_mins, fridge_run_mins); debug();            
        sprintf(s_debug, "--------- KJØL STOPP ------"); debug();            
        #endif
        
        fridge_avg = 0;
    }    
  
    
    //sprintf(s_debug, "Fridge Temp/ADC/Volt: %d/%Lu/%03.2w", Temps.fridge, adc_val, volt); debug();    
    
    // Update FRIDGE temp struct
    if(Temps.fridge >= FRIDGE_MAX_TEMP) 
    { 
        Temps.fridge_to_warm = YES;        
    }
    else
    {
        Temps.fridge_to_warm = NO;
    }    
    
    if(Temps.fridge >= FRIDGE_TO_WARM) 
    { 
        Alarms.fridge_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "Alarm: Fridge to warm!!"); debug(); #endif
    }
    else
    {
        Alarms.freezer_temp = NO;
    }        
    
        
    // Update FREEZER temp struct    
    if(Temps.freezer >= FREEZER_MAX_TEMP) 
    { 
        Temps.freezer_to_warm = YES;        
    }
    else
    {
        Temps.freezer_to_warm = NO;
    }    
    
    if(Temps.freezer >= FREEZER_TO_WARM) 
    { 
        Alarms.freezer_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "Alarm: Freezer to warm!!"); debug(); #endif
    }
    else
    {
        Alarms.freezer_temp = NO;
    }        
    
    
    #ifdef DEBUG
    if(Alarms.fridge_sensor_fault) { sprintf(s_debug, "Alarm: Fridge SENSOR_FAULT!!"); debug(); }
    if(Alarms.freezer_sensor_fault) { sprintf(s_debug, "Alarm: Freezer SENSOR_FAULT!!"); debug(); }
    #endif        
    
    
    //sprintf(s_debug, "Freezer Temp/ADC/Volt: %d/%Lu/%03.2w", Temps.freezer, adc_val, volt); debug();       

    //if(Temps.freezer == FREEZER_TO_WARM) { sprintf(s_debug, "Freezer to warm!!"); debug(); }
    //if(Alarms.freezer_sensor_fault) { sprintf(s_debug, "Freezer SENSOR_FAULT!!"); debug(); }
    
    #ifdef DEBUG
    sprintf(s_debug, "Fridge/Freezer ruuning: %u/%u", fridge_running, freezer_running); debug();            
    sprintf(s_debug, "------------------------------------------"); debug();
    #endif

    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    
    DisableSensors;
    
    delay_ms(2000);
}


/**
* 
* temp øker 2-3 grader når 240V kobles fra, dvs temp øker ved lavere VDD
*/
signed int8 SensorADToDegrees(int16 ad_val, int1 is_fridge)
{
    signed int8 temp;
    float adc_voltage;    
    //unsigned int32 volt;


adc_voltage = ( ( (float) ad_val / 1023 ) * VDD );
//sprintf(s_debug, "float voltage: %02.3f", adc_voltage); debug();    
//sprintf(s_debug, "float temp:    %02.3f", (adc_voltage*100.0) - 273.0); debug();    
temp = (signed int8) ( (adc_voltage*100.0) - 273.0); // convert Kelvin to Celcius

/* int32 attempt...
volt = ((int32)ad_val*(int32)VDD_SF) / 100000 ;
sprintf(s_debug, "int32 voltage: %04.3w", volt); debug();    
temp = (signed int8) (((volt *100L) - 273)/100); //100
sprintf(s_debug, "int32 temp: %d", temp); debug();    
*/

    if(is_fridge)
    {
        if(temp >= FRIDGE_TO_WARM) Temps.fridge_to_warm = YES; else Temps.fridge_to_warm = NO;
        if(temp <= FRIDGE_TO_COLD) Temps.fridge_to_cold = YES; else Temps.fridge_to_cold = NO;
        if(ad_val < 400 || ad_val > 800) Alarms.fridge_sensor_fault = YES; else Alarms.fridge_sensor_fault = NO;
    }
    else
    {
        if(temp >= FREEZER_TO_WARM) Temps.freezer_to_warm = YES; else Temps.freezer_to_warm = NO;
        if(temp <= FREEZER_TO_COLD) Temps.freezer_to_cold = YES; else Temps.freezer_to_cold = NO;        
        if(ad_val < 400 || ad_val > 800) Alarms.freezer_sensor_fault = YES; else Alarms.freezer_sensor_fault = NO;
    }        

    return temp;
      
}


/**
* Read the pot's value, this is only used for alarm and turning on/off a motor
* We need to pull port down after reading it, because the pot's (via 7K resistor) fine-tunes the sensors in the circuit, when 
* not reading them via the 1K resistor
*/
void readPots(void)
{
    unsigned int16 adc_val_fridge_pot, adc_val_freezer_pot;    
    
    // tris also set pulldown's to output
    set_tris_a(TRISA_SAMPLE_POTS); 
    set_tris_b(TRISB_SAMPLE_POTS); 
    delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        
    EnableSensors; // we need to power the pots via transistor (and R6 1K) to read them, just like the sensors...
    output_low(POT_FRIDGE_PULLDOWN); // and pull down voltage divider resistors
    output_low(POT_FREEZER_PULLDOWN);   
    
    delay_ms(1);

    adc_val_fridge_pot = getAvgADC(ADC_POT_FRIDGE, 30);
    output_float(POT_FRIDGE_PULLDOWN);
    delay_ms(1);
    
    adc_val_freezer_pot = getAvgADC(ADC_POT_FREEZER, 30);
    output_float(POT_FREEZER_PULLDOWN);   
    
    //pot_freezer = FridgeLookup[1/ adc_val_freezer_pot / 8];
    //259 - 752
    if(adc_val_freezer_pot > 870L) Temps.pot_freezer = POT_OFF;
    else if(adc_val_freezer_pot > 650L) Temps.pot_freezer = -18;
    else if(adc_val_freezer_pot > 570L) Temps.pot_freezer = -19;
    
    else if(adc_val_freezer_pot > 450L) Temps.pot_freezer = -21;
    else if(adc_val_freezer_pot > 638L) Temps.pot_freezer = -25; //OK
    
    else if(adc_val_freezer_pot > 250L) Temps.pot_freezer = -22;
    else if(adc_val_freezer_pot > 200L) Temps.pot_freezer = -23;
    else if(adc_val_freezer_pot > 300L) Temps.pot_freezer = -24; // TODO: lower to 200...
        
    // 238 - 674
    if(adc_val_fridge_pot > 800L)      Temps.pot_fridge = POT_OFF;
    else if(adc_val_fridge_pot > 600L) Temps.pot_fridge = 6;
    else if(adc_val_fridge_pot > 510L) Temps.pot_fridge = 5;
    else if(adc_val_fridge_pot > 410L) Temps.pot_fridge = 4;
    else if(adc_val_fridge_pot > 340L) Temps.pot_fridge = 3;        
    else if(adc_val_fridge_pot > 310L) Temps.pot_fridge = 2;
    else
        Temps.pot_fridge = 1;
                                                        
    // tris also set pulldown's to input, so we don't need to set them float.
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    // not needed, many cycles before tris is used again  //delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
    
    #ifdef DEBUG   
    sprintf(s_debug, "POT Fridge  Temp/ADC %d/%Lu", Temps.pot_fridge, adc_val_fridge_pot); debug();       
    sprintf(s_debug, "POT Freezer Temp/ADC %d/%Lu", Temps.pot_freezer, adc_val_freezer_pot); debug();   
    sprintf(s_debug, "Transistor Q5: %d", input(TRANSISTOR_Q5C)); debug();
    sprintf(s_debug, "------------------------------------------"); debug();
    #endif
                
    DisableSensors;
    
}

        
/**
* Read X samples from ADC, average them and return result
* If samples=1, just one reading is done (rearly used)
**/
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples)
{
   unsigned int16 temp_adc_value=0;
   unsigned int8 x;
   
   if(samples == 0) samples = 20;
   
   for (x=0;x<samples;x++)
   {
      set_adc_channel ( channel );
      delay_us(50);
      temp_adc_value += read_adc();
   }

   temp_adc_value /= samples;

   if (temp_adc_value < 1) temp_adc_value=1; // never divide by zero:

   return temp_adc_value;
}



void debug(void)
{
  //using SW UART we can't interrupt while writing...
  disable_interrupts(GLOBAL);
  printf("%s\r\n", s_debug);
  delay_ms(5);
  enable_interrupts(GLOBAL);
    
}    