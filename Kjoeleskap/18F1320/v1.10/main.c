#include "18F1320.h"
/* BUGS
- FIXED - slår av og på rele, mens de er på
- FIXED - bytt max_run_min til int16
- FIXED - ikke sensor_fault når åpen krets

TODO's
- Kalibrer frys POT, helt kaos i koden (og sjekk kjøl POT)
- Prøv å få et system i green/amber led håndtering
- Flytt mer vars til structs
- Revurder hvordan debug blir gjort
- FIXED - Add blinkrate on red led
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


#zero_ram
void main()
{
    
    
    prev_restart_reason = read_eeprom (MEM_ADDR_RESTART);
    write_eeprom (MEM_ADDR_RESTART, restart_cause()); 
    curr_restart_reason = read_eeprom (MEM_ADDR_RESTART);        
    
    //can be fex RESTART_WATCHDOG, RESTART_POWER_UP
          
    initialize_hardware();
    initialize_software();
      
    delay_ms(1000);
  
    
  #ifdef DEBUG printf(STREAM_DEBUG, "** Startup, curr/prev reason: %u,%u", curr_restart_reason, prev_restart_reason);  #endif
  
  
       
  while(TRUE)
  {
    restart_wdt();

    
    delay_ms(2000);
    
    if(do_read_temp && !Status.sampling)
    {
        readTempFreezer();
        readTempFridge();                 
        readPotFridge();
        readPotFreezer();            
        do_read_temp = 0;
    }        
    
    updateAlarms();             
    doDecision();
    
    if((SampleFridge.do_actions || SampleFreezer.do_actions) && poweredup_secs > 60) 
        doActions();
    
    checkAlarms();
    
    #ifdef DEBUG

    //free(ptrstr);
    //ptrstr=NULL;    
    //ptrstr = malloc(15);
    //strcpy(ptrstr, "Fridge/Freezer");
            
    printf(STREAM_DEBUG, "supercool: %u, deepfreeze: %u\r\n", Status.mode_supercool, Status.mode_deepfreeze); 
    //printf(STREAM_DEBUG, "curr/prev boot reason: %u,%u\r\n", curr_restart_reason, prev_restart_reason); 
    
    printf(STREAM_DEBUG, "Fridge FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u\r\n", Temps.fridge_calc, Temps.fridge_real, Status.fridge_stopped_mins, Status.fridge_run_mins);
    printf(STREAM_DEBUG, "Freezer FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u\r\n\r\n", Temps.freezer_calc, Temps.freezer_real, Status.freezer_stopped_mins, Status.freezer_run_mins);        
    printf(STREAM_DEBUG, "POT Fridge %d/%Lu\r\n", Temps.pot_fridge, SampleFridgePot.avg_adc_value);    
    printf(STREAM_DEBUG, "POT Freezer %d/%Lu\r\n", Temps.pot_freezer, SampleFreezerPot.avg_adc_value);
    printf(STREAM_DEBUG, "\r\n");    
    printf(STREAM_DEBUG, "Fridge/Freezer Starts: %u/%u\r\n", started2, started1);         
    printf(STREAM_DEBUG, "Fridge/Freezer running: %u/%u\r\n", Status.fridge_running, Status.freezer_running);

    printf(STREAM_DEBUG, "Q5: %d\r\n", input(TRANSISTOR_Q5C));         

    printf(STREAM_DEBUG, "Fridge/Freezer total run: %Lu/%Lu\r\n", Status.fridge_total_run_mins, Status.freezer_total_run_mins);
    printf(STREAM_DEBUG, "**END CYCLE**\r\n");
    #endif
    
    
                    
            
        
  }

}
/*
unsigned int8 *color2str(unsigned int1 value)
{
   free(ptrstr);
   ptrstr=NULL;

   if(value)
   {
      ptrstr = malloc(6); // Allocate enough memory to hold the string
      strcpy(ptrstr, "AMBER");
   }
   else
   {
      ptrstr = malloc(6); // Allocate enough memory to hold the string
      strcpy(ptrstr, "GREEN");
   } 

   return &ptrstr[0]; // &str[0] is the same as just specifying str!
}
*/

#separate
void initialize_hardware(void)
{
    setup_wdt(WDT_ON); // settings/Postscaler set in fuses
    
    setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_BIT); // ISR 1ms
    setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
    //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
    setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
    //setup_timer_2(T2_DIV_BY_1,9,10); // OF 10uS, ISR 100Us
    //setup_timer_2(T2_DIV_BY_4,100,8); 
    
    //setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // OF 65,5ms
    setup_timer_3(T3_INTERNAL|T3_DIV_BY_8); // ISR 524ms

    
    setup_adc_ports(sAN1|sAN4|sAN5|sAN6|VSS_VDD); // Inc the POTs   
        
    //setup_adc(ADC_CLOCK_DIV_32|ADC_TAD_MUL_20);
    setup_adc(ADC_CLOCK_DIV_64|ADC_TAD_MUL_20);
    
    setup_ccp1(CCP_OFF);
    port_b_pullups(FALSE);   
    
    enable_interrupts(INT_TIMER0);
    enable_interrupts(INT_TIMER1);
    enable_interrupts(INT_TIMER2);
    enable_interrupts(INT_TIMER3);   
    enable_interrupts(GLOBAL);

    set_tris_a(TRISA_NORMAL); 
    set_tris_b(TRISB_NORMAL); 
    
}

#separate    
void initialize_software(void)
{    
    //LEDs = 0;
    Stop_Fridge_Motor;
    Stop_Freezer_Motor;
    Temps.freezer_real = NOT_READY;
    Temps.fridge_real = NOT_READY;
    Temps.pot_freezer = NOT_READY;
    Temps.pot_fridge = NOT_READY;
    
    Status.fridge_stopped_mins = FRIDGE_MIN_STOP_MIN; // instant turn-on at power on
    Status.freezer_stopped_mins = FREEZER_MIN_STOP_MIN; // instant turn-on at power on
    config.freezer_degrees_under = FREEZER_DEGREES_UNDER;
    config.fridge_degrees_under = FRIDGE_DEGREES_UNDER;

    config.freezer_run_temp_offset = FREEZER_RUN_TEMP_OFFSET; 
    config.freezer_stop_temp_offset = FREEZER_STOP_TEMP_OFFSET; 
    
    config.fridge_run_temp_offset = FRIDGE_RUN_TEMP_OFFSET; 
    config.fridge_stop_temp_offset = FRIDGE_STOP_TEMP_OFFSET; 
    
    DisableSensors; 
    
}    

#separate
void setLEDs(void)
{
    /*********** FRIDGE LEDs ***********/        
    if(Status.fridge_running)
    {
        LEDs.DL2=LED_ON;
        LEDs.DL2_Color = LED_GREEN;
    }
    else
        LEDs.DL2=LED_OFF;
        
    if(Status.fridge_running && Status.mode_supercool)
    { 
        LEDs.DL2=LED_BLINK;     
        LEDs.DL2_Color = LED_AMBER;
        LEDs.DL1_Color = LED_AMBER; // if on is amber, both needs to be....HW bug
    }
    
    if(Alarms.fridge_run_to_long || Alarms.fridge_sensor_fault || Alarms.fridge_temp)
        LEDs.DL2=LED_BLINK_FAST;

    /*********** FREEZER LEDs ***********/        

    if(Status.freezer_running)
    {
        LEDs.DL1=LED_ON;
        LEDs.DL1_Color = LED_GREEN;
    }
    else
        LEDs.DL1=LED_OFF;
            
    if(Status.freezer_running && Status.mode_deepfreeze)
    { 
        LEDs.DL1=LED_BLINK;     
        LEDs.DL1_Color = LED_AMBER;
        LEDs.DL2_Color = LED_AMBER; // if on is amber, both needs to be....HW bug
    }
    
    
    if(Alarms.freezer_run_to_long || Alarms.freezer_sensor_fault || Alarms.freezer_temp)
        LEDs.DL1=LED_BLINK_FAST;

    /*********** ALARM LED ***********/        
     
    if(Alarms.freezer_temp || Alarms.fridge_temp)
        LEDs.DL3 = LED_ON;
    else if (Alarms.fridge_sensor_fault || Alarms.freezer_sensor_fault || Alarms.fridge_run_to_long || Alarms.freezer_run_to_long)
        LEDs.DL3 = LED_BLINK;    
    else    
        LEDs.DL3 = LED_OFF;        
    
}
    
/**************** Logic/Decisions *************************/            
#separate
void doDecision(void)
{
    //signed int8 temp;
/*    
    if(Status.freezer_run_mins == 1) sample_run_f = Temps.freezer_real;
    if(Status.fridge_run_mins == 1) sample_run_k = Temps.fridge_real;
    if(Status.freezer_stopped_mins == 1) sample_stop_f = Temps.freezer_real;
    if(Status.fridge_stopped_mins == 1) sample_stop_k = Temps.fridge_real;
    printf(STREAM_DEBUG, "**R/S F/K %d %d %d %d\r\n", sample_run_f, sample_run_k, sample_stop_f, sample_stop_k); 
  */  
        
    if( (Temps.fridge_real == 0 && Temps.freezer_real == 0) || Temps.fridge_real == NOT_READY || Temps.freezer_real == NOT_READY || Temps.pot_fridge == NOT_READY || Temps.pot_freezer == NOT_READY) return; // not fully initialized yet
    
    Alarms.freezer_run_to_long = 0;
    Alarms.fridge_run_to_long = 0;
    /******* Logic for start/stop/keep running the FREEZER *******/    

    if(Status.freezer_running && (Temps.freezer_real > -32 && !Status.mode_deepfreeze)) // safety, real -35=-28 i skapet, etter kjørt 240 min...
        Temps.freezer_calc = Temps.freezer_real + config.freezer_run_temp_offset;
    else
        Temps.freezer_calc = Temps.freezer_real + config.freezer_stop_temp_offset;

    //printf(STREAM_DEBUG, "**Freezer FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u\r\n", Temps.freezer_calc, Temps.freezer, Status.freezer_stopped_mins, Status.freezer_run_mins);                                
                
    if(Temps.pot_freezer == POT_OFF) // pot turned all off
    {
        Status.run_freezer = NO;
    }    
    else if(Status.freezer_run_mins >= FREEZER_MAX_RUN_MIN) // run too long
    {        
        Status.run_freezer = NO;        
        Alarms.freezer_run_to_long = YES;
        Status.mode_deepfreeze = OFF;
        #ifdef DEBUG2 printf(STREAM_DEBUG, "freezer_run > FREEZER_MAX_RUN\r\n");  #endif
    }    
    else if(Alarms.freezer_sensor_fault)
    {
        Status.run_freezer = NO;        
    }    
    
    if(Status.freezer_running)
    {            
        if(Temps.freezer_calc > (Temps.pot_freezer - config.freezer_degrees_under))
        {
            Status.run_freezer = YES;
            //#ifdef DEBUG printf(STREAM_DEBUG, "DECIDE freezer running: %d > (%d-%u)= %d > %d", Temps.freezer_calc, Temps.pot_freezer, FREEZER_DEGREES_UNDER, Temps.freezer, Temps.pot_freezer-FREEZER_DEGREES_UNDER);  #endif   
        }
        else
            Status.run_freezer = NO;
    }
    else
    {
        if(Temps.freezer_calc >= (Temps.pot_freezer + FREEZER_DEGREES_OVER))
        {
            Status.run_freezer = YES;
            //#ifdef DEBUG printf(STREAM_DEBUG, "DECIDE freezer running: %d >= (%d+%u)= %d >= %d", Temps.freezer_calc, Temps.pot_freezer, FREEZER_DEGREES_OVER, Temps.freezer, Temps.pot_freezer+FREEZER_DEGREES_OVER);  #endif   
        }
        else
            Status.run_freezer = NO;            
    }        
    
    if(Temps.freezer_to_warm) Status.run_freezer = YES;            
    if(Temps.freezer_to_cold) Status.run_freezer = NO;
        
    if(Status.freezer_running && !Status.run_freezer) 
        if(Status.freezer_run_mins < FREEZER_MIN_RUN_MIN)
            Status.run_freezer = YES; // abort stop if not run minimum time
        
    
    if(!Status.freezer_running && Status.run_freezer) // we are supposed to start
        if(Status.freezer_stopped_mins < FREEZER_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            Status.run_freezer = NO; // abort start            
        }         
    
    
    
    /******************** Deepfreeze might override *******************************/
    if(Status.run_freezer == NO && Status.mode_deepfreeze)
    {
        if(Temps.freezer_calc <= M_DEEPFREEZE_TEMP) // reached deepfreeze temp                
            Status.mode_deepfreeze = OFF;                    
        else 
            Status.run_freezer = YES;            
    }    
    
    /****** final check, nothing can override this! *************/
    if(Alarms.freezer_run_to_long) 
        Status.run_freezer = NO;
        
        
    /******* Logic for start/stop/keep running the FRIDGE *******/

    if(Status.fridge_running)
        Temps.fridge_calc = Temps.fridge_real + config.fridge_run_temp_offset;
    else
        Temps.fridge_calc = Temps.fridge_real + config.fridge_stop_temp_offset;
    
    //printf(STREAM_DEBUG, "**Fridge FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u\r\n", Temps.fridge_calc, Temps.fridge, Status.fridge_stopped_mins, Status.fridge_run_mins);                 

    if(Temps.pot_fridge == POT_OFF) // pot turned all off
    {
        Status.run_fridge = NO;
    }    
    else if(Status.fridge_run_mins >= FRIDGE_MAX_RUN_MIN) // run too long
    {
        Status.run_fridge = NO;        
        Alarms.freezer_run_to_long = YES;
        Status.mode_supercool = OFF;
        #ifdef DEBUG2 printf(STREAM_DEBUG, "fridge_run > FRIDGE_MAX_RUN\r\n");  #endif
    }    
    else if(Alarms.fridge_sensor_fault)
    {
        Status.run_fridge = NO;        
    }        
    else 
    {
        if(Status.fridge_running)
        {
            if(Temps.fridge_calc > (Temps.pot_fridge - config.fridge_degrees_under))
            {
                Status.run_fridge = YES;
                //#ifdef DEBUG printf(STREAM_DEBUG, "DECIDE fridge running: %d > (%d-%u)= %d > %d", Temps.fridge_calc, Temps.pot_fridge, FRIDGE_DEGREES_UNDER, Temps.fridge, Temps.pot_fridge-FRIDGE_DEGREES_UNDER);  #endif   
            }
            else
                Status.run_fridge = NO;
        }
        else
        {
            if(Temps.fridge_calc >= (Temps.pot_fridge + FRIDGE_DEGREES_OVER))
            {
                Status.run_fridge = YES;
                //#ifdef DEBUG printf(STREAM_DEBUG, "DECIDE fridge running: %d >= (%d+%u)= %d >= %d", Temps.fridge_calc, Temps.pot_fridge, FRIDGE_DEGREES_OVER, Temps.fridge, Temps.pot_fridge+FRIDGE_DEGREES_OVER);  #endif   
            }
            else
                Status.run_fridge = NO;            
        }        
        
        if(Temps.fridge_to_warm) Status.run_fridge = YES;        
        if(Temps.fridge_to_cold) Status.run_fridge = NO;
    }    

    if(Status.fridge_running && !Status.run_fridge) 
        if(Status.fridge_run_mins < FRIDGE_MIN_RUN_MIN) //if(fridge_stopped_mins < FRIDGE_MIN_RUN_MIN)
            Status.run_fridge = YES; // abort stop if not run minimum time
        
 
    if(!Status.fridge_running && Status.run_fridge) // we are supposed to start
        if(Status.fridge_stopped_mins < FRIDGE_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            Status.run_fridge = NO; // abort start                        
        }         
    
        
    /********* SuperCool might override ******************/
    if(Status.run_fridge == NO && Status.mode_supercool)
    {
        if(Temps.fridge_calc <= M_SUPERCOOL_TEMP) // reached Supercool temp        
            Status.mode_supercool = OFF;
        else
            Status.run_fridge = YES;
    }
    
    /****** final check, nothing can override this! *************/
    if(Alarms.fridge_run_to_long) 
        Status.run_fridge = NO;        
}    

/**************** Actions *************************/            
void doActions(void)
{       
    
    if(SampleFridge.do_actions)
    {   
        SampleFridge.do_actions = NO;
        if(Status.run_fridge && !Status.fridge_running) 
        {           
            Start_Fridge_Motor;            
        }    
        
        if(!Status.run_fridge && Status.fridge_running)
        {
            Stop_Fridge_Motor;
            Status.fridge_stopped_mins = 0;                    
        }    
    }   
    if(SampleFreezer.do_actions)
    {
        SampleFreezer.do_actions = NO;
        if(Status.run_freezer && !Status.freezer_running) 
        {           
            Start_Freezer_Motor;            
        }    
        
        if(!Status.run_freezer && Status.freezer_running) 
        {
            Stop_Freezer_Motor;
            Status.freezer_stopped_mins = 0;            
        }
    }
    
    #ifdef DEBUG    
    //                 
    //printf(STREAM_DEBUG, "** %s running: %u/%u\r\n", STR_FRIDGE_FREEZER, Status.fridge_running, Status.freezer_running);                 
    #endif
    
}

void checkAlarms(void)
{
/********************* Alarming ********************************************/           

    if(Alarms.fridge_run_to_long)
        if(Status.fridge_stopped_mins >= WAIT_MIN_AFTER_LONG_RUN)
            Alarms.fridge_run_to_long = NO;

    if(Alarms.freezer_run_to_long)
        if(Status.freezer_stopped_mins >= WAIT_MIN_AFTER_LONG_RUN)
            Alarms.freezer_run_to_long = NO;

}        





void updateAlarms(void)
{
    signed int8 tmp;
    
    // Update FRIDGE temp struct    
    tmp = Temps.fridge_real + config.fridge_stop_temp_offset; // for now we assume this is accurate enough to avoid alarm when motors run...    
    if(tmp > FRIDGE_MAX_TEMP) Temps.fridge_to_warm = YES; else Temps.fridge_to_warm = NO;    
    if(tmp < FRIDGE_MIN_TEMP) Temps.fridge_to_cold = YES; else Temps.fridge_to_cold = NO;

    // Update FREEZER temp struct    
    tmp = Temps.freezer_real + config.freezer_stop_temp_offset; // for now we assume this is accurate enough to avoid alarm when motors run...    
    if(tmp > FREEZER_MAX_TEMP) Temps.freezer_to_warm = YES; else Temps.freezer_to_warm = NO;
    //if(tmp < FREEZER_MIN_TEMP) Temps.freezer_to_cold = YES; else Temps.freezer_to_cold = NO;
    
    
    if(Temps.fridge_real == NOT_READY || Temps.freezer_real == NOT_READY) return;
    
    Alarms.fridge_temp = NO;
    
    if(Temps.fridge_to_warm) 
    { 
        Alarms.fridge_temp = YES;
        #ifdef DEBUG_ALARMS printf(STREAM_DEBUG, "A: Fridge to WARM\r\n");  #endif
    }
    else if(Temps.fridge_to_cold)
    {
        Alarms.fridge_temp = YES;
        #ifdef DEBUG_ALARMS printf(STREAM_DEBUG, "A: Fridge to COLD\r\n");  #endif        
    }        
           
    
    Alarms.freezer_temp = NO;
    
    if(Temps.freezer_to_warm) 
    { 
        Alarms.freezer_temp = YES;
        #ifdef DEBUG printf(STREAM_DEBUG, "A: Freezer to WARM\r\n");  #endif
    }
    /*else if(Temps.freezer_to_cold)
    {
        Alarms.freezer_temp = YES;
        #ifdef DEBUG_ALARMS printf(STREAM_DEBUG, "A: Freezer to COLD\r\n");  #endif
    }    
    */
    
    #ifdef DEBUG_ALARMS
    if(Alarms.fridge_sensor_fault) { printf(STREAM_DEBUG, "A: Fridge SENSOR_FAULT\r\n");  }
    if(Alarms.freezer_sensor_fault) { printf(STREAM_DEBUG, "A: Freezer SENSOR_FAULT\r\n");  }
    #endif        
        
    
}

    
/**
* 
* temp øker ved lavere VDD
*/
#separate
signed int8 SensorADToDegrees(int16 ad_val, int1 is_fridge)
{
    signed int8 temp;
    float adc_voltage;    
    //unsigned int32 volt;

adc_voltage = ( ( (float) ad_val / 1023 ) * VDD );
//printf(STREAM_DEBUG, "float voltage: %02.3f\r\n", adc_voltage);     
//printf(STREAM_DEBUG, "float temp:    %02.3f\r\n", (adc_voltage*100.0) - 273.0);     
temp = (signed int8) ( (adc_voltage*100.0) - 273.0); // convert Kelvin to Celcius

/* int32 attempt...
volt = ((int32)ad_val*(int32)VDD_SF) / 100000 ;
printf(STREAM_DEBUG, "int32 voltage: %04.3w", volt);     
temp = (signed int8) (((volt *100L) - 273)/100); //100
printf(STREAM_DEBUG, "int32 temp: %d", temp);     
*/

    if(is_fridge)    
        if(ad_val < 400 || ad_val > 800) Alarms.fridge_sensor_fault = YES; else Alarms.fridge_sensor_fault = NO;
    else
        if(ad_val < 400 || ad_val > 800) Alarms.freezer_sensor_fault = YES; else Alarms.freezer_sensor_fault = NO;
    
    return temp;
      
}



#separate            
void STREAM_DEBUG(unsigned int8 c)
{
    disable_interrupts(GLOBAL);
    
    putc(c);
    //printf("C:%c,U:%u,H:%X", c, c, c);
    enable_interrupts(GLOBAL);

}

/*
void debug(void)
{
  //using SW UART we can't interrupt while writing...
  disable_interrupts(GLOBAL);
  printf("%s\r\n", STREAM_DEBUG);
  delay_ms(5);
  //strcpy(STREAM_DEBUG, '\0');
  enable_interrupts(GLOBAL);
    
} 
*/   