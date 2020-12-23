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
        if(poweredup_secs < 255) poweredup_secs++;
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
    
    if(LEDs.COM == LED_AMBER)
    {
        if(LEDs.DL1 == LED_BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, !LEDs.DL1);
        if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, !LEDs.DL2);
        if(LEDs.DL3 == LED_BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, !LEDs.DL3);
    }
    else
    {    
        if(LEDs.DL1 == LED_BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, LEDs.DL1);
        if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, LEDs.DL2);
        if(LEDs.DL3 == LED_BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
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
    unsigned int8 restart_reason;
    
    restart_reason = restart_cause();        
    //can be fex RESTART_WATCHDOG, RESTART_POWER_UP
   //findout cause of re-start
    //start_check(); 
    
    Temps.freezer = NOT_READY;
    Temps.fridge = NOT_READY;
      
    initialize_hardware();
    initialize_software();
      
    delay_ms(1000);
  
    
  #ifdef DEBUG sprintf(s_debug, "** Startup, reason: %u", restart_reason); debug(); #endif
     
  while(TRUE)
  {
    restart_wdt();
    
    if(do_read_temp && !sampling)
    {        
        
        do_read_temp = 0;
        readTempFreezer();
        readTempFridge();                 
        readPots();    
        updateAlarms();
        delay_ms(2000); 
        doDecision();
        if(do_actions && poweredup_secs > 50) 
            doActions();
        
        #ifdef DEBUG
        //sprintf(s_debug, "SW2: %u, SW3: %u", SW_fridge_pressed, SW_freezer_pressed); debug();
        //sprintf(s_debug, "mode_supercool: %u, mode_deepfreeze: %u", mode_supercool, mode_deepfreeze); debug();
        sprintf(s_debug, "start count freezer/fridge: %u/%u", started1, started2); debug();        
        #endif
    }
                    
            
        
  }

}

#separate
void initialize_hardware(void)
{
    setup_wdt(WDT_ON); // settings/Postscaler set in fuses
    
    setup_timer_0(RTCC_INTERNAL|RTCC_DIV_4|RTCC_8_BIT); // ISR 1ms
    setup_timer_1(T1_INTERNAL|T1_DIV_BY_8); // ISR 524ms
    //setup_timer_2(T2_DIV_BY_1,255,16); // OF 256us, ISR 4ms
    setup_timer_2(T2_DIV_BY_4,255,1); // OF 1ms, ISR 1ms
    setup_timer_3(T3_INTERNAL|T3_DIV_BY_1); // OF 65,5ms
    
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
    LEDs.COM = LED_GREEN; 
    Stop_Fridge_Motor;
    Stop_Freezer_Motor;
    fridge_stopped_mins = FRIDGE_MIN_STOP_MIN; // instant turn-on at power on
    freezer_stopped_mins = FREEZER_MIN_STOP_MIN; // instant turn-on at power on
    config.freezer_degrees_under = FREEZER_DEGREES_UNDER;
    config.fridge_degrees_under = FRIDGE_DEGREES_UNDER;
    
    DisableSensors; 
    
}    

/**************** Logic/Decisions *************************/            
#separate
void doDecision(void)
{
     //readTempFreezer();
     //readTempFridge();
        
    if( (Temps.fridge == 0 && Temps.freezer == 0) || (Temps.fridge == NOT_READY || Temps.freezer == NOT_READY)) return; // not fully initialized yet
    
    /******* Logic for start/stop/keep running the FREEZER *******/    
    
    if(Temps.pot_freezer == POT_OFF) // pot turned all off
    {
        run_freezer = NO;
    }    
    else if(freezer_run_mins >= FREEZER_MAX_RUN_MIN) // run too long
    {        
        run_freezer = NO;
        LEDs.DL1=LED_BLINK;
        #ifdef DEBUG2 sprintf(s_debug, "freezer_run_mins > FREEZER_MAX_RUN_MIN: %Lu > %u", freezer_run_mins, FREEZER_MAX_RUN_MIN); debug(); #endif
    }    
    else if(Alarms.freezer_sensor_fault)
    {
        run_freezer = NO;
        LEDs.DL1=LED_BLINK;
    }    
        if(freezer_running)
        {            
            if(Temps.freezer > (Temps.pot_freezer - config.freezer_degrees_under))
            {
                run_freezer = YES;
                //#ifdef DEBUG sprintf(s_debug, "DECIDE freezer running: %d > (%d-%u)= %d > %d", Temps.freezer, Temps.pot_freezer, FREEZER_DEGREES_UNDER, Temps.freezer, Temps.pot_freezer-FREEZER_DEGREES_UNDER); debug(); #endif   
            }
            else
                run_freezer = NO;
        }
        else
        {
            if(Temps.freezer >= (Temps.pot_freezer + FREEZER_DEGREES_OVER))
            {
                run_freezer = YES;
                //#ifdef DEBUG sprintf(s_debug, "DECIDE freezer running: %d >= (%d+%u)= %d >= %d", Temps.freezer, Temps.pot_freezer, FREEZER_DEGREES_OVER, Temps.freezer, Temps.pot_freezer+FREEZER_DEGREES_OVER); debug(); #endif   
            }
            else
                run_freezer = NO;            
        }        
        
        if(Temps.freezer_to_warm) run_freezer = YES;            
    
        
    if(freezer_running && !run_freezer) 
        if(freezer_run_mins < FREEZER_MIN_RUN_MIN)
            run_freezer = YES; // abort stop if not run minimum time
        
    
    if(!freezer_running && run_freezer) // we are supposed to start
        if(freezer_stopped_mins < FREEZER_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            run_freezer = NO; // abort start            
        }         
    
    
    
    /******************** Deepfreeze might override *******************************/
    if(run_freezer == NO && mode_deepfreeze)
    {
        if(Temps.freezer > M_DEEPFREEZE_TEMP)
        {
            if(freezer_run_mins <= FREEZER_MAX_RUN_MIN*2) // still under max runtime * 2
            {
                run_freezer = YES;   
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

    if(run_freezer)
        if(mode_deepfreeze) 
            LEDs.DL1=LED_BLINK; 
        else 
            LEDs.DL1=LED_ON;
    
    /******* Logic for start/stop/keep running the FRIDGE *******/
        
    if(Temps.pot_fridge == POT_OFF) // pot turned all off
    {
        run_fridge = NO;
    }    
    else if(fridge_run_mins >= FRIDGE_MAX_RUN_MIN) // run too long
    {
        run_fridge = NO;
        LEDs.DL2=LED_BLINK;
        #ifdef DEBUG2 sprintf(s_debug, "fridge_run_mins > FRIDGE_MAX_RUN_MIN: %Lu > %u", fridge_run_mins, FRIDGE_MAX_RUN_MIN); debug(); #endif
    }    
    else if(Alarms.fridge_sensor_fault)
    {
        run_fridge = NO;
        LEDs.DL2=LED_BLINK;
    }    
    //else if( ((Temps.fridge >= (Temps.pot_fridge + FRIDGE_DEGREES_OVER) && Temps.fridge >= (Temps.pot_fridge - FRIDGE_DEGREES_UNDER)) || Temps.fridge_to_warm) ) 
    else 
    {
        if(fridge_running)
        {
            if(Temps.fridge > (Temps.pot_fridge - config.fridge_degrees_under))
            {
                run_fridge = YES;
                //#ifdef DEBUG sprintf(s_debug, "DECIDE fridge running: %d > (%d-%u)= %d > %d", Temps.fridge, Temps.pot_fridge, FRIDGE_DEGREES_UNDER, Temps.fridge, Temps.pot_fridge-FRIDGE_DEGREES_UNDER); debug(); #endif   
            }
            else
                run_fridge = NO;
        }
        else
        {
            if(Temps.fridge >= (Temps.pot_fridge + FRIDGE_DEGREES_OVER))
            {
                run_fridge = YES;
                //#ifdef DEBUG sprintf(s_debug, "DECIDE fridge running: %d >= (%d+%u)= %d >= %d", Temps.fridge, Temps.pot_fridge, FRIDGE_DEGREES_OVER, Temps.fridge, Temps.pot_fridge+FRIDGE_DEGREES_OVER); debug(); #endif   
            }
            else
                run_fridge = NO;            
        }        
        
        if(Temps.fridge_to_warm) run_fridge = YES;        
    }    

    if(fridge_running && !run_fridge) 
        if(fridge_run_mins < FRIDGE_MIN_RUN_MIN) //if(fridge_stopped_mins < FRIDGE_MIN_RUN_MIN)
            run_fridge = YES; // abort stop if not run minimum time
        
 
    if(!fridge_running && run_fridge) // we are supposed to start
        if(fridge_stopped_mins < FRIDGE_MIN_STOP_MIN) // minimum XX secs between stop and start of motor, no hysteresis...
        {    
            run_fridge = NO; // abort start                        
        }         
    
        
    /********* SuperCool might override ******************/
    if(run_fridge == NO && mode_supercool)
    {
        if(Temps.fridge > M_SUPERCOOL_TEMP)
        {
            if(fridge_run_mins <= FRIDGE_MAX_RUN_MIN*2) // still under max runtime * 2
            {
                run_fridge = YES;                
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
    
    if(run_fridge)
        if(mode_supercool) 
            LEDs.DL2=LED_BLINK; 
        else 
            LEDs.DL2=LED_ON;    
    
    
/********************* Alarming ********************************************/           
    if(Alarms.fridge_temp || Alarms.freezer_temp)
        LEDs.DL3 = LED_BLINK;    
    else if(Alarms.fridge_sensor_fault || Alarms.freezer_sensor_fault)
        LEDs.DL3 = LED_ON;    
    else    
        LEDs.DL3 = LED_OFF;    


    
    
}    

/**************** Actions *************************/            
void doActions(void)
{       
    do_actions = NO;
        
    if(run_fridge && !fridge_running) 
    {           
        Start_Fridge_Motor;
        LEDs.DL2=LED_ON;
        #ifdef DEBUG2 sprintf(s_debug, "Starting fridge"); debug(); #endif
    }    
    
    if(!run_fridge && fridge_running)
    {
        Stop_Fridge_Motor;
        LEDs.DL2=LED_OFF;
        fridge_stopped_mins = 0;        
        #ifdef DEBUG2 sprintf(s_debug, "Stopping fridge"); debug();  #endif
    }    

    if(run_freezer && !freezer_running) 
    {           
        Start_Freezer_Motor;
        LEDs.DL1=LED_ON;
        #ifdef DEBUG2 sprintf(s_debug, "Starting freezer"); debug();       #endif
    }    
    
    if(!run_freezer && freezer_running) 
    {
        Stop_Freezer_Motor;
        LEDs.DL1=LED_OFF;
        freezer_stopped_mins = 0;
        #ifdef DEBUG2 sprintf(s_debug, "Stopping freezer"); debug();       #endif
    }
    
}
    
/**
* 
*/
//#separate
void readTempFreezer(void)
{
    //unsigned int16 adc_val;
    //signed int8 min_val, max_val;
    unsigned int8 x;
                
    EnableSensors;
    set_tris_a(TRISA_SAMPLE_SENSORS); 
    set_tris_b(TRISB_SAMPLE_SENSORS); 
    delay_ms(10);               
    adc_val = getAvgADC(ADC_FREEZER, 100);
        
                
        
    SampleFreezer.samples[SampleFreezer.index] = SensorADToDegrees(adc_val, NO);
    sprintf(s_debug, "FRYS sample #%u:%d", SampleFreezer.index, SampleFreezer.samples[SampleFreezer.index]); debug();        
    SampleFreezer.index++;
    
    if(SampleFreezer.index == SAMPLE_COUNT)  
    {

        #ifdef DEBUG
        sprintf(s_debug, "** FRYS START"); debug();    
        #endif

        for(x=0; x<SampleFreezer.index; x++)
        {
            if(min_val > SampleFreezer.samples[x]) min_val = SampleFreezer.samples[x];
            if(max_val < SampleFreezer.samples[x]) max_val = SampleFreezer.samples[x];
        }    
        
        if(Temps.freezer != NOT_READY && min_val < max_val)
            if(max_val - min_val >=3 && SampleFreezer.tries < 3) // too big diff, but not too many tries, reject samples!
            {
                SampleFreezer.index = 0;            
                SampleFreezer.tries++;
                sprintf(s_debug, "To big diff (%d-%d), rejecting samples!", max_val, min_val); debug();    
            }
        
        if(SampleFreezer.index == SAMPLE_COUNT) // We got a complete sample set, calc avg                        
        {                
            Temps.freezer = 0;
            for(x=0; x<SampleFreezer.index; x++)
            {
                Temps.freezer += SampleFreezer.samples[x];                
            }    
            Temps.freezer /= (signed int8)(SAMPLE_COUNT+1);
            
            if(freezer_running) 
                Temps.freezer += 4; //7 pga sensor plassert i grillene        
            else
                Temps.freezer += 0; //5 avg når grillen tempereres

            SampleFreezer.index = 0;
            SampleFreezer.tries = 0;
            max_val = 0;
            min_val = 0;
        }
    }
            
    #ifdef DEBUG
    sprintf(s_debug, "Freezer Temp, Stop/Run-mins: %d, %u/%Lu", Temps.freezer, freezer_stopped_mins, freezer_run_mins); debug();                           
    //sprintf(s_debug, "** FRYS STOPP"); debug();    
    #endif
      
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    
    DisableSensors;
    
    updateStructs();
    
}


//#separate
void readTempFridge(void)
{
    //unsigned int16 adc_val2;
    //signed int8 min_val, max_val;
    unsigned int8 x;
                
    EnableSensors;
    set_tris_a(TRISA_SAMPLE_SENSORS); 
    set_tris_b(TRISB_SAMPLE_SENSORS); 
    delay_ms(10);    
    adc_val = getAvgADC(ADC_FRIDGE, 100);

    
    SampleFridge.samples[SampleFridge.index] = SensorADToDegrees(adc_val, YES);
    sprintf(s_debug, "KJØL sample #%u:%d", SampleFridge.index, SampleFridge.samples[SampleFridge.index]); debug();        
    SampleFridge.index++;
    
    
    if(SampleFridge.index == SAMPLE_COUNT)  
    {
        sprintf(s_debug, "** KJØL START"); debug();    

        for(x=0; x<SampleFridge.index; x++)
        {
            if(min_val_fridge > SampleFridge.samples[x]) min_val_fridge = SampleFridge.samples[x];
            if(max_val_fridge < SampleFridge.samples[x]) max_val_fridge = SampleFridge.samples[x];
        }    
        
        if(Temps.fridge != NOT_READY && min_val_fridge < max_val_fridge)
            if(max_val_fridge - min_val_fridge >=3 && SampleFridge.tries < 3) // too big diff, but not too many tries, reject samples!
            {
                SampleFridge.index = 0;            
                SampleFridge.tries++;
                sprintf(s_debug, "To big diff (%d-%d), rejecting samples!", max_val_fridge, min_val_fridge); debug();        
            }
        
        if(SampleFridge.index == SAMPLE_COUNT) // We got a complete sample set, calc avg                        
        {                
            Temps.fridge = 0;
            for(x=0; x<SampleFridge.index; x++)
            {
                Temps.fridge += SampleFridge.samples[x];                
                //sprintf(s_debug, "x:%u %d,s:%d", x, Temps.fridge, SampleFridge.samples[x]); debug();
            }    
            Temps.fridge /= (signed int8)(SAMPLE_COUNT+1);           
            

            if(fridge_running)
                Temps.fridge += 0; //-=0 5 pga sensor plassert på varmeste sted...vifte blåser kulde ned
            else
                Temps.fridge -= 3; //+=1
                        
            sprintf(s_debug, "%d/%u", Temps.fridge, SAMPLE_COUNT); debug();
            
            SampleFridge.index = 0;
            SampleFridge.tries = 0;
            max_val_fridge = 0;
            min_val_fridge = 0;            
        }
    }

    #ifdef DEBUG
    sprintf(s_debug, "Fridge Temp, Stop/Run-mins: %d, %u/%Lu", Temps.fridge, fridge_stopped_mins, fridge_run_mins); debug();            
    //sprintf(s_debug, "** KJØL STOPP"); debug();            
    #endif

    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    
    DisableSensors;
    
    updateStructs();
    
}    



void updateAlarms(void)
{
    if(Temps.fridge == NOT_READY || Temps.freezer == NOT_READY) return;
    
    Alarms.fridge_temp = NO;
    
    if(Temps.fridge_to_warm) 
    { 
        Alarms.fridge_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "Alarm: Fridge to WARM!!"); debug(); #endif
    }
    else if(Temps.fridge_to_cold)
    {
        Alarms.fridge_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "Alarm: Fridge to COLD!!"); debug(); #endif        
    }        
    
        

    
    Alarms.freezer_temp = NO;
    
    if(Temps.freezer_to_warm) 
    { 
        Alarms.freezer_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "Alarm: Freezer to WARM!!"); debug(); #endif
    }
    else if(Temps.freezer_to_cold)
    {
        Alarms.freezer_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "Alarm: Freezer to COLD!!"); debug(); #endif
    }    
    
    
    #ifdef DEBUG
    if(Alarms.fridge_sensor_fault) { sprintf(s_debug, "Alarm: Fridge SENSOR_FAULT!!"); debug(); }
    if(Alarms.freezer_sensor_fault) { sprintf(s_debug, "Alarm: Freezer SENSOR_FAULT!!"); debug(); }
    #endif        
    
        
    #ifdef DEBUG
    sprintf(s_debug, "Fridge/Freezer running: %u/%u", fridge_running, freezer_running); debug();            
    sprintf(s_debug, "------------------------------------------"); debug();
    #endif
    
    
}

    
/**
* 
* temp øker ved lavere VDD
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
        if(ad_val < 400 || ad_val > 800) Alarms.fridge_sensor_fault = YES; else Alarms.fridge_sensor_fault = NO;
    else
        if(ad_val < 400 || ad_val > 800) Alarms.freezer_sensor_fault = YES; else Alarms.freezer_sensor_fault = NO;
    
    return temp;
      
}


/**
* Read the pot's value, this is only used for alarm and turning on/off a motor
* We need to pull port down after reading it, because the pot's (via 7K resistor) fine-tunes the sensors in the circuit, when 
* not reading them via the 1K resistor
*/
void readPots(void)
{
    //unsigned int16 adc_val;// adc_val_fridge_pot, adc_val_freezer_pot;    
    
    // tris also set pulldown's to output
    set_tris_a(TRISA_SAMPLE_POTS); 
    set_tris_b(TRISB_SAMPLE_POTS); 
    delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        
    EnableSensors; // we need to power the pots via transistor (and R6 1K) to read them, just like the sensors...
    output_low(POT_FRIDGE_PULLDOWN); // and pull down voltage divider resistors
    output_low(POT_FREEZER_PULLDOWN);   
    
    delay_ms(1);

    adc_val = getAvgADC(ADC_POT_FRIDGE, 30);
    output_float(POT_FRIDGE_PULLDOWN);
    delay_ms(1);
    
    // 238 - 674
    if(adc_val > 800L)      Temps.pot_fridge = POT_OFF;
    else if(adc_val > 600L) Temps.pot_fridge = 6;
    else if(adc_val > 510L) Temps.pot_fridge = 5;
    else if(adc_val > 410L) Temps.pot_fridge = 4;
    else if(adc_val > 340L) Temps.pot_fridge = 3;        
    else if(adc_val > 310L) Temps.pot_fridge = 2;
    else
        Temps.pot_fridge = 1;

    // not allow temp go under 1C, if so ignore FRIDGE_DEGREES_UNDER 
    if((Temps.pot_fridge - FRIDGE_DEGREES_UNDER) < 1)
        config.fridge_degrees_under = 0;
    else
    {
        switch(Temps.pot_fridge)
        {
            case 1: config.fridge_degrees_under = 0; break;   
            case 2: config.fridge_degrees_under = 1; break;
            default: config.fridge_degrees_under = FRIDGE_DEGREES_UNDER; 
        }
    }        

    #ifdef DEBUG       
    sprintf(s_debug, "POT Fridge: %d, UNDER: %u", Temps.pot_fridge, config.fridge_degrees_under); debug();       
    #endif


    
    adc_val = getAvgADC(ADC_POT_FREEZER, 30);
    output_float(POT_FREEZER_PULLDOWN);   
    
    //pot_freezer = FridgeLookup[1/ adc_val_freezer_pot / 8];
    //259 - 752
    if(adc_val > 870L) Temps.pot_freezer = POT_OFF;
    else if(adc_val > 650L) Temps.pot_freezer = -18;
    else if(adc_val > 570L) Temps.pot_freezer = -19;
    
    else if(adc_val > 450L) Temps.pot_freezer = -21;
    else if(adc_val > 638L) Temps.pot_freezer = -25; //OK
    
    else if(adc_val > 250L) Temps.pot_freezer = -22;
    else if(adc_val > 200L) Temps.pot_freezer = -23;
    else if(adc_val > 300L) Temps.pot_freezer = -24; // TODO: lower to 200...
        
                                                        
    // tris also set pulldown's to input, so we don't need to set them float.
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    // not needed, many cycles before tris is used again  //delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
    
    #ifdef DEBUG       
    //sprintf(s_debug, "POT Freezer Temp/ADC %d/%Lu", Temps.pot_freezer, adc_val); debug();   
    sprintf(s_debug, "POT Freezer: %d, UNDER: %u", Temps.pot_freezer, config.freezer_degrees_under); debug();   
    sprintf(s_debug, "Q5: %d", input(TRANSISTOR_Q5C)); debug();
    sprintf(s_debug, "------------------------------------------"); debug();
    #endif
                
        
    DisableSensors;
    
}

void updateStructs(void)
{
    // Update FRIDGE temp struct
    if(Temps.fridge >= FRIDGE_MAX_TEMP) Temps.fridge_to_warm = YES; else Temps.fridge_to_warm = NO;
    if(Temps.fridge <= FRIDGE_MIN_TEMP) Temps.fridge_to_cold = YES; else Temps.fridge_to_cold = NO;

    // Update FREEZER temp struct    
    if(Temps.freezer >= FREEZER_MAX_TEMP) Temps.freezer_to_warm = YES; else Temps.freezer_to_warm = NO;
    if(Temps.freezer <= FREEZER_MIN_TEMP-5) Temps.freezer_to_cold = YES; else Temps.freezer_to_cold = NO;        
        
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


/*
void PUT_SERIAL(unsigned int8 *str)
{
  //using SW UART we can't interrupt while writing...
  disable_interrupts(GLOBAL);
  printf("%s\r\n", *str);
  delay_ms(5);
  enable_interrupts(GLOBAL);
    
}
*/

void debug(void)
{
  //using SW UART we can't interrupt while writing...
  disable_interrupts(GLOBAL);
  printf("%s\r\n", s_debug);
  delay_ms(5);
  //strcpy(s_debug, '\0');
  enable_interrupts(GLOBAL);
    
}    