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

#int_TIMER0
void  TIMER0_isr(void) // OF every 1ms
{
    
    if(++ISR_counter0 == 60000) // 60 sec
    {        
        if(Status.fridge_running)
        {
            Status.fridge_stopped_mins = 0;
            Status.fridge_run_mins++;
        }    
        else
        {
            Status.fridge_stopped_mins++;
            Status.fridge_run_mins = 0;

        }    
        
        if(Status.freezer_running)              
        {
            Status.freezer_stopped_mins = 0;
            Status.freezer_run_mins++;
        }
        else
        {
            Status.freezer_stopped_mins++;
            Status.freezer_run_mins = 0;
        }        

        ISR_counter0 = 0;
    }
    
    if(++ISR_counter0_2 == 500) 
    {
        blink_toggle_fast ^= 1;
        //ISR_counter0_2 = 0;
    }    
    else if(++ISR_counter0_2 == 1000) 
    {
        blink_toggle_fast ^= 1;
        blink_toggle ^= blink_toggle_fast;
        ISR_counter0_2 = 0;
    }
    
    if(!Status.sampling)
    {
        
        //disable_interrupts(INT_TIMER2); // No LED controlling on PORTB now!
        Status.sampling_switches = 1;        
        
        delay_us(50);
        
        set_tris_b(TRISB_SAMPLE_SWITCHES); 
        
        delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        if(bit_test(PORTB, 5))
        {             
            SW_fridge_time = 0;
        }
        else 
        {
                    
            SW_fridge_time++; 
            if(SW_fridge_time > 2000 && SW_fridge_time < 4000)
                reset_cpu();            
            else if(SW_fridge_time < 500)
                Status.mode_supercool = !Status.mode_supercool;
                                
        }
                        
        if(bit_test(PORTB, 2))
        {             
            SW_freezer_time = 0;
        }
        else 
        {
                    
            SW_fridge_time++; 
            if(SW_freezer_time > 2000 && SW_freezer_time < 4000)
                reset_cpu();            
            else if(SW_freezer_time < 500)
                Status.mode_deepfreeze = !Status.mode_deepfreeze;
                
        }
        
                
        set_tris_b(TRISB_NORMAL);         
        Status.sampling_switches = 0;
        //clear_interrupt(INT_TIMER2);
        //enable_interrupts(INT_TIMER2);
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
        if(++sample_timer_sec == 10) //last_samples[6]; // last 6 samples need to be equal, before we call for actions , 10 sec between
        {
            sample_timer_sec = 0;                  
            SampleFridge.do_final_sample = 1;
            SampleFreezer.do_final_sample = 1;
        }    
    }    
}

// This Timer ONLY deals with LEDs, because it's disabled/enabled frequently, for reading AD and switches
#int_TIMER2
void  TIMER2_isr(void)  // ISR 1ms
{
        
    if(Status.sampling || Status.sampling_switches) return;
    
    //disable_interrupts(INT_TIMER2);
    
    
 /*   
    if(LEDs.DL1_Color == LED_AMBER && LEDs.DL2_Color == LED_AMBER)
    {
        //turn of DL1 & DL2, so they don't turn green
        if(LEDs.COM_Last == LED_GREEN) output_bit(LED_DL1, LED_GREEN); 
        if(LEDs.COM_Last == LED_GREEN) output_bit(LED_DL2, LED_GREEN); 
        
        // take care of DL3 (RED) first
        output_bit(LED_COM, LED_GREEN); // LED_GREEN = COM=LOW
        //turn on/off DL3 as requested (RED, needs COM=LOW, GREEN)
        if(LEDs.DL3 == LED_BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
        //delay_us(400);

        output_bit(LED_COM, LED_AMBER); // LED_AMBER = COM=HIGH
        LEDs.COM_Last = LED_AMBER;
        //turn on/off LEDs as requested (inverted because of AMBER=COM=HIGH)
        if(LEDs.DL1 == LED_BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, !LEDs.DL1);
        if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, !LEDs.DL2);         
    }    
    else if(LEDs.DL1_Color == LED_AMBER)
    {           
        if(LEDs.COM_Last == LED_GREEN) output_bit(LED_DL1, LED_GREEN); //turn of DL1, so it don't turn green        
        // take care of DL2 (green) first
        
        output_bit(LED_COM, LED_GREEN); // LED_GREEN = COM=LOW
        //turn on/off DL2 as requested (green)
        if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, LEDs.DL2);        
        //turn on/off DL3 as requested (RED, needs COM=LOW, GREEN)
        if(LEDs.DL3 == LED_BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);        
        //delay_us(400);
        
        output_bit(LED_COM, LED_AMBER); // LED_AMBER = COM=HIGH
        LEDs.COM_Last = LED_AMBER;
        //turn on/off DL1 as requested (inverted because of AMBER=COM=HIGH)
        if(LEDs.DL1 == LED_BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, !LEDs.DL1); 
        
    }    
    else if(LEDs.DL2_Color == LED_AMBER)
    {        
        if(LEDs.COM_Last == LED_GREEN) output_bit(LED_DL2, LED_GREEN); //turn of DL2, so it don't turn green        
        // take care of DL1 (green) first
        output_bit(LED_COM, LED_GREEN); // LED_GREEN = COM=LOW        
        //turn on/off DL1 as requested (green)
        if(LEDs.DL1 == LED_BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, LEDs.DL1);        
        //turn on/off DL3 as requested (RED, needs COM=LOW, GREEN)
        if(LEDs.DL3 == LED_BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
        //delay_us(400);
        output_bit(LED_COM, LED_AMBER); // LED_AMBER = COM=HIGH
        LEDs.COM_Last = LED_AMBER;
        //turn on/off DL2 as requested (inverted because of AMBER=COM=HIGH)
        if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, !LEDs.DL2); 
        
    }    
    else // both GREEN
    {
  */
        output_bit(LED_COM, LED_GREEN); // LED_GREEN = COM=LOW
        LEDs.COM_Last = LED_GREEN;

        if(LEDs.DL1 == LED_BLINK) 
            output_bit(LED_DL1, blink_toggle); 
        else if(LEDs.DL1 == LED_BLINK_FAST) 
            output_bit(LED_DL1, blink_toggle_fast); 
        else 
            output_bit(LED_DL1, LEDs.DL1);

        if(LEDs.DL2 == LED_BLINK) 
            output_bit(LED_DL2, blink_toggle); 
        else if(LEDs.DL2 == LED_BLINK_FAST) 
            output_bit(LED_DL2, blink_toggle_fast); 
        else 
            output_bit(LED_DL2, LEDs.DL2);
            
        if(LEDs.DL3 == LED_BLINK) 
            output_bit(LED_DL3, blink_toggle); 
        else if(LEDs.DL3 == LED_BLINK_FAST) 
            output_bit(LED_DL3, blink_toggle_fast); 
        else 
            output_bit(LED_DL3, LEDs.DL3);
                                    
        //if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, LEDs.DL2);
        //if(LEDs.DL3 == LED_BLINK) output_bit(LED_DL3, blink_toggle); else output_bit(LED_DL3, LEDs.DL3);
  //  }
    
    
    //clear_interrupt(INT_TIMER2);
    //enable_interrupts(INT_TIMER2);
}

#int_TIMER3
void  TIMER3_isr(void) // 524ms
{

}

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
  
    
  #ifdef DEBUG sprintf(s_debug, "** Startup, curr/prev reason: %u,%u", curr_restart_reason, prev_restart_reason); debug(); #endif
  
  
       
  while(TRUE)
  {
    restart_wdt();
  
    if(do_read_temp && !Status.sampling)
    {        
        // TODO: tune config.fridge_run_temp_offset etc in respect to runtime
/*
        //LEDs.DL1=LED_ON;
        LEDs.DL2=LED_BLINK;

    if(Status.mode_deepfreeze) 
        LEDs.DL1 = LED_BLINK_FAST;//        
    else
        LEDs.DL1=LED_OFF; 

        sprintf(s_debug, "DL1:%u,DL2=%u DL1_C:%u,DL2_C:%u", LEDs.DL1, LEDs.DL2, LEDs.DL1_Color, LEDs.DL2_Color); debug();
*/                
        do_read_temp = 0;
        readTempFreezer();
        readTempFridge();                 
        readPots();            
        updateAlarms();
        delay_ms(2000); 
        doDecision();
        if((SampleFridge.do_actions || SampleFreezer.do_actions) && poweredup_secs > 60) 
            doActions();
        
        checkAlarms();
        
        #ifdef DEBUG
        
        sprintf(s_debug, "mode_supercool: %u, mode_deepfreeze: %u", Status.mode_supercool, Status.mode_deepfreeze); debug();
        //sprintf(s_debug, "curr/prev boot reason: %u,%u", curr_restart_reason, prev_restart_reason); debug();
        sprintf(s_debug, "start count freezer/fridge: %u/%u", started1, started2); debug();        
        #endif
    }
    
                    
            
        
  }

}

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
    Temps.freezer = NOT_READY;
    Temps.fridge = NOT_READY;
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

/**************** Logic/Decisions *************************/            
#separate
void doDecision(void)
{
    signed int8 temp;
    
    if(Status.freezer_run_mins == 1) sample_run_f = Temps.freezer;
    if(Status.fridge_run_mins == 1) sample_run_k = Temps.fridge;
    if(Status.freezer_stopped_mins == 1) sample_stop_f = Temps.freezer;
    if(Status.fridge_stopped_mins == 1) sample_stop_k = Temps.fridge;
    sprintf(s_debug, "**R/S F/K %d %d %d %d", sample_run_f, sample_run_k, sample_stop_f, sample_stop_k); debug();
    
        
    if( (Temps.fridge == 0 && Temps.freezer == 0) || (Temps.fridge == NOT_READY || Temps.freezer == NOT_READY)) return; // not fully initialized yet
    
    /******* Logic for start/stop/keep running the FREEZER *******/    

    if(Status.fridge_running)
        temp = Temps.freezer + config.freezer_run_temp_offset;
    else
        temp = Temps.freezer + config.freezer_stop_temp_offset;

    sprintf(s_debug, "**Freezer FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u", temp, Temps.freezer, Status.freezer_stopped_mins, Status.freezer_run_mins); debug();                               
                
    if(Temps.pot_freezer == POT_OFF) // pot turned all off
    {
        Status.run_freezer = NO;
    }    
    else if(Status.freezer_run_mins >= FREEZER_MAX_RUN_MIN) // run too long
    {        
        Status.run_freezer = NO;
        LEDs.DL1=LED_BLINK_FAST;
        #ifdef DEBUG2 sprintf(s_debug, "freezer_run > FREEZER_MAX_RUN"); debug(); #endif
    }    
    else if(Alarms.freezer_sensor_fault)
    {
        Status.run_freezer = NO;
        LEDs.DL1=LED_BLINK_FAST;
    }    
    
    if(Status.freezer_running)
    {            
        if(temp > (Temps.pot_freezer - config.freezer_degrees_under))
        {
            Status.run_freezer = YES;
            //#ifdef DEBUG sprintf(s_debug, "DECIDE freezer running: %d > (%d-%u)= %d > %d", temp, Temps.pot_freezer, FREEZER_DEGREES_UNDER, Temps.freezer, Temps.pot_freezer-FREEZER_DEGREES_UNDER); debug(); #endif   
        }
        else
            Status.run_freezer = NO;
    }
    else
    {
        if(temp >= (Temps.pot_freezer + FREEZER_DEGREES_OVER))
        {
            Status.run_freezer = YES;
            //#ifdef DEBUG sprintf(s_debug, "DECIDE freezer running: %d >= (%d+%u)= %d >= %d", Temps.freezer, Temps.pot_freezer, FREEZER_DEGREES_OVER, Temps.freezer, Temps.pot_freezer+FREEZER_DEGREES_OVER); debug(); #endif   
        }
        else
            Status.run_freezer = NO;            
    }        
    
    if(Temps.freezer_to_warm) Status.run_freezer = YES;            
    
        
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
        if(temp > M_DEEPFREEZE_TEMP)
        {
            if(Status.freezer_run_mins <= FREEZER_MAX_RUN_MIN) // still under max runtime
            {
                Status.run_freezer = YES;   
                //#ifdef DEBUG sprintf(s_debug, "Deepfreeze mode, force running"); debug(); #endif
            }
            else
            {                
                //#ifdef DEBUG sprintf(s_debug, "Failed to reach Deepfreeze temp on given time!"); debug(); #endif
                Status.mode_deepfreeze = OFF; // SHOULD WE DO THIS ? OR KEEP RUNNING AFTER THE BREAK ?
            }        
        }
        else // reached deepfreeze temp        
            Status.mode_deepfreeze = OFF;                    
    }    

    if(Status.run_freezer)
        LEDs.DL1=LED_ON;

    if(Status.mode_deepfreeze) 
        LEDs.DL1=LED_BLINK; 
    
    /******* Logic for start/stop/keep running the FRIDGE *******/

    if(Status.fridge_running)
        temp = Temps.fridge + config.fridge_run_temp_offset;
    else
        temp = Temps.fridge + config.fridge_stop_temp_offset;
    
    sprintf(s_debug, "**Fridge FAKE/REAL Temp, Stop/Run-mins: %d/%d, %u/%u", temp, Temps.fridge, Status.fridge_stopped_mins, Status.fridge_run_mins); debug();                

    if(Temps.pot_fridge == POT_OFF) // pot turned all off
    {
        Status.run_fridge = NO;
    }    
    else if(Status.fridge_run_mins >= FRIDGE_MAX_RUN_MIN) // run too long
    {
        Status.run_fridge = NO;
        LEDs.DL2=LED_BLINK_FAST;
        #ifdef DEBUG2 sprintf(s_debug, "fridge_run > FRIDGE_MAX_RUN"); debug(); #endif
    }    
    else if(Alarms.fridge_sensor_fault)
    {
        Status.run_fridge = NO;
        LEDs.DL2=LED_BLINK_FAST;
    }    
    //else if( ((Temps.fridge >= (Temps.pot_fridge + FRIDGE_DEGREES_OVER) && Temps.fridge >= (Temps.pot_fridge - FRIDGE_DEGREES_UNDER)) || Temps.fridge_to_warm) ) 
    else 
    {
        if(Status.fridge_running)
        {
            if(temp > (Temps.pot_fridge - config.fridge_degrees_under))
            {
                Status.run_fridge = YES;
                //#ifdef DEBUG sprintf(s_debug, "DECIDE fridge running: %d > (%d-%u)= %d > %d", Temps.fridge, Temps.pot_fridge, FRIDGE_DEGREES_UNDER, Temps.fridge, Temps.pot_fridge-FRIDGE_DEGREES_UNDER); debug(); #endif   
            }
            else
                Status.run_fridge = NO;
        }
        else
        {
            if(temp >= (Temps.pot_fridge + FRIDGE_DEGREES_OVER))
            {
                Status.run_fridge = YES;
                //#ifdef DEBUG sprintf(s_debug, "DECIDE fridge running: %d >= (%d+%u)= %d >= %d", Temps.fridge, Temps.pot_fridge, FRIDGE_DEGREES_OVER, Temps.fridge, Temps.pot_fridge+FRIDGE_DEGREES_OVER); debug(); #endif   
            }
            else
                Status.run_fridge = NO;            
        }        
        
        if(Temps.fridge_to_warm) Status.run_fridge = YES;        
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
        if(temp > M_SUPERCOOL_TEMP)
        {
            if(Status.fridge_run_mins <= FRIDGE_MAX_RUN_MIN) // still under max runtime
            {
                Status.run_fridge = YES;                
                //#ifdef DEBUG sprintf(s_debug, "Supercool mode, force run"); debug(); #endif
            }
            else
            {                
                //#ifdef DEBUG sprintf(s_debug, "Failed to reach Supercool temp"); debug(); #endif
                Status.mode_supercool = OFF; // SHOULD WE DO THIS ? OR KEEP RUNNING AFTER THE BREAK ?
            }        
        }
        else // reached Supercool temp
            Status.mode_supercool = OFF;

    }
    
    if(Status.run_fridge)
        LEDs.DL2=LED_ON;    

    if(Status.mode_supercool) 
        LEDs.DL2=LED_BLINK; 
    //else
      //  LEDs.DL2_Color=LED_GREEN; 
    
}    

/**************** Actions *************************/            
void doActions(void)
{       
    
    if(SampleFridge.do_actions)
    {   //sprintf(s_debug, "**FRI ACT"); debug(); 
        SampleFridge.do_actions = NO;
        if(Status.run_fridge && !Status.fridge_running) 
        {           
            Start_Fridge_Motor;
            LEDs.DL2=LED_ON;
            //#ifdef DEBUG2 sprintf(s_debug, "Starting fridge"); debug(); #endif            
            
        }    
        
        if(!Status.run_fridge && Status.fridge_running)
        {
            Stop_Fridge_Motor;
            LEDs.DL2=LED_OFF;
            Status.fridge_stopped_mins = 0;        
            //#ifdef DEBUG2 sprintf(s_debug, "Stopping fridge"); debug();  #endif
        }    
    }   
    if(SampleFreezer.do_actions)
    {
        SampleFreezer.do_actions = NO;
        if(Status.run_freezer && !Status.freezer_running) 
        {           
            Start_Freezer_Motor;
            LEDs.DL1=LED_ON;
            //#ifdef DEBUG2 sprintf(s_debug, "Starting freezer"); debug();       #endif
        }    
        
        if(!Status.run_freezer && Status.freezer_running) 
        {
            Stop_Freezer_Motor;
            LEDs.DL1=LED_OFF;
            Status.freezer_stopped_mins = 0;
            //#ifdef DEBUG2 sprintf(s_debug, "Stopping freezer"); debug();       #endif
        }
    }
    
    #ifdef DEBUG
    sprintf(s_debug, "** Fridge/Freezer running: %u/%u", Status.fridge_running, Status.freezer_running); debug();            
    //sprintf(s_debug, "----------"); debug();
    #endif
    
}

void checkAlarms(void)
{
/********************* Alarming ********************************************/           
    if(Alarms.freezer_temp)
    {        
        LEDs.DL1 = LED_BLINK_FAST; 
    }    
    else
    {
        if(Status.freezer_running) 
            LEDs.DL1 = LED_ON; 
        else
            LEDs.DL1 = LED_OFF;         
    }        

    if(Alarms.fridge_temp)
    {        
        LEDs.DL2 = LED_BLINK_FAST; 
    }    
    else
    {
        if(Status.fridge_running) 
            LEDs.DL2 = LED_ON; 
        else
            LEDs.DL2 = LED_OFF;         
    }        
     
    if(Alarms.freezer_temp || Alarms.fridge_temp)
        LEDs.DL3 = LED_ON;
    else if (Alarms.fridge_sensor_fault || Alarms.freezer_sensor_fault)
        LEDs.DL3 = LED_BLINK;    
    else    
        LEDs.DL3 = LED_OFF;    
    
}        


/**
* 
*/
#separate
void readTempFreezer(void)
{
    //unsigned int16 adc_val;
    signed int8 tmp;
                
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
        
        max_val=-127;
        min_val=127;
        //min_val = SampleFreezer.samples[0]; // needs to > 0 at some point...
        for(x=0; x<SampleFreezer.index; x++)
        {
            tmp = (signed int8)SampleFreezer.samples[x];
            if(min_val >= tmp) min_val = tmp;
            if(max_val <= tmp) max_val = tmp;
            //if(min_val >= (signed int8)SampleFreezer.samples[x]) min_val = (signed int8)SampleFreezer.samples[x];
            //if(max_val <= (signed int8)SampleFreezer.samples[x]) max_val = (signed int8)SampleFreezer.samples[x];
        }    
        
        if(Temps.freezer != NOT_READY && min_val < max_val)
            if(max_val - min_val >=3 && SampleFreezer.tries < 3) // too big diff, but not too many tries, reject samples!
            {
                SampleFreezer.index = 0;            
                SampleFreezer.tries++;
                sprintf(s_debug, "Big diff (%d-%d), skip", max_val, min_val); debug();    
            }
        
        if(SampleFreezer.index == SAMPLE_COUNT) // We got a complete sample set, calc avg                        
        {                
            Temps.freezer = 0;

            for(x=0; x<SampleFreezer.index; x++)
                Temps.freezer += SampleFreezer.samples[x];                

            Temps.freezer /= (signed int8)(SAMPLE_COUNT);

            SampleFreezer.index = 0;
            SampleFreezer.tries = 0;

        
            if(SampleFreezer.do_final_sample)
            {                
                sprintf(s_debug, "Added l_sample #%u:%d", SampleFreezer.last_index, Temps.freezer); debug();            
                SampleFreezer.last_samples[SampleFreezer.last_index] = Temps.freezer;
                SampleFreezer.last_index++;
                SampleFreezer.do_final_sample = 0;
            }    
            
            
            // we need 6 equal samples with 10 secs between, before we approve it and set do any actions
            if(SampleFreezer.last_index==6)
            {
                max_val=-127;
                min_val=127;
                //min_val = SampleFreezer.last_samples[0]; // needs to > 0 at some point...                
                for(x=0; x<SampleFreezer.last_index; x++)
                {                                
                    tmp = (signed int8)SampleFreezer.last_samples[x];
                    if(min_val >= tmp) min_val = tmp;
                    if(max_val <= tmp) max_val = tmp;                    
                    //if(min_val > (signed int8)SampleFreezer.last_samples[x]) min_val = (signed int8)SampleFreezer.last_samples[x];                
                    //if(max_val < (signed int8)SampleFreezer.last_samples[x]) max_val = (signed int8)SampleFreezer.last_samples[x];                    
                }    
                
                
                if(min_val == max_val || SampleFreezer.last_tries > 2) 
                {
                    SampleFreezer.do_actions = 1; // OKAY to to any actions, temp seems stable
                    SampleFreezer.last_tries = 0;                    
                }    
                else
                {
                                
                    SampleFreezer.last_tries++;

                    sprintf(s_debug, "DIFF last_samples: %d-%d", min_val, max_val); debug();
                }
                SampleFreezer.last_index = 0;    
            }
        
        }
    }
                 
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    
    DisableSensors;
    
    
    
}


//#separate
void readTempFridge(void)
{
    signed int8 tmp;
                
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

        max_val=-127;
        min_val=127;
        //min_val = SampleFridge.samples[x]; // needs to > 0 at some point...            
        for(x=0; x<SampleFridge.index; x++)
        {
            tmp = (signed int8)SampleFridge.samples[x];
            if(min_val >= tmp) min_val = tmp;
            if(max_val <= tmp) max_val = tmp;            
            
            //if(min_val > (signed int8)SampleFridge.samples[x]) min_val = (signed int8)SampleFridge.samples[x];                
            //if(max_val < (signed int8)SampleFridge.samples[x]) max_val = (signed int8)SampleFridge.samples[x];
        }    
        
        if(Temps.fridge != NOT_READY && min_val < max_val)
            if(max_val - min_val >=3 && SampleFridge.tries < 3) // too big diff, but not too many tries, reject samples!
            {
                SampleFridge.index = 0;            
                SampleFridge.tries++;
                //sprintf(s_debug, "big diff (%d-%d), skip", max_val, min_val); debug();        
            }
        
        if(SampleFridge.index == SAMPLE_COUNT) // We got a complete sample set, calc avg                        
        {                
            Temps.fridge = 0;
                        
            for(x=0; x<SampleFridge.index; x++)
                Temps.fridge += SampleFridge.samples[x];                
                
            Temps.fridge /= (signed int8)(SAMPLE_COUNT);           

            SampleFridge.index = 0;
            SampleFridge.tries = 0;
            
            if(SampleFridge.do_final_sample)
            {                
                //sprintf(s_debug, "Added l_sample #%u:%d", SampleFridge.last_index, Temps.fridge); debug();            
                SampleFridge.last_samples[SampleFridge.last_index] = Temps.fridge;
                SampleFridge.last_index++;
                SampleFridge.do_final_sample = 0;
            }    
            
            
            // we need 6 equal samples with 10 secs between, before we approve it and set do any actions
            if(SampleFridge.last_index==6)
            {
                max_val=-127;
                min_val=127;
                //min_val = SampleFridge.last_samples[0]; // needs to > 0 at some point...                
                for(x=0; x<SampleFridge.last_index; x++)
                {                                
                    tmp = (signed int8)SampleFridge.last_samples[x];
                    if(min_val >= tmp) min_val = tmp;
                    if(max_val <= tmp) max_val = tmp;                    
                    //if(min_val > (signed int8)SampleFridge.last_samples[x]) min_val = (signed int8)SampleFridge.last_samples[x];                
                    //if(max_val < (signed int8)SampleFridge.last_samples[x]) max_val = (signed int8)SampleFridge.last_samples[x];                    
                }    
                
                if(min_val == max_val || SampleFridge.last_tries > 2) 
                {
                    SampleFridge.do_actions = 1; // OKAY to to any actions, temp seems stable
                    SampleFridge.last_tries = 0;                    
                }    
                else
                {                    
                    SampleFridge.last_tries++;

                    //sprintf(s_debug, "DIFF last_samples: %d-%d", min_val, max_val); debug();
                }
                SampleFridge.last_index = 0;    
            }
                
            
        }
    }

    #ifdef DEBUG
    //sprintf(s_debug, "Fridge REAL Temp, Stop/Run-mins: %d, %u/%Lu", Temps.fridge, fridge_stopped_mins, fridge_run_mins); debug();            
    //sprintf(s_debug, "** KJØL STOPP"); debug();            
    #endif

    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    
    DisableSensors;
    
    //updateStructs();
    
}    



void updateAlarms(void)
{
    signed int8 tmp;
    
    // Update FRIDGE temp struct    
    tmp = Temps.fridge + config.fridge_stop_temp_offset; // for now we assume this is accurate enough to avoid alarm when motors run...    
    if(tmp > FRIDGE_MAX_TEMP) Temps.fridge_to_warm = YES; else Temps.fridge_to_warm = NO;    
    if(tmp < FRIDGE_MIN_TEMP) Temps.fridge_to_cold = YES; else Temps.fridge_to_cold = NO;

    // Update FREEZER temp struct    
    tmp = Temps.freezer + config.freezer_stop_temp_offset; // for now we assume this is accurate enough to avoid alarm when motors run...    
    if(tmp > FREEZER_MAX_TEMP) Temps.freezer_to_warm = YES; else Temps.freezer_to_warm = NO;
    if(tmp < FREEZER_MIN_TEMP) Temps.freezer_to_cold = YES; else Temps.freezer_to_cold = NO;
    
    
    if(Temps.fridge == NOT_READY || Temps.freezer == NOT_READY) return;
    
    Alarms.fridge_temp = NO;
    
    if(Temps.fridge_to_warm) 
    { 
        Alarms.fridge_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "A: Fridge to WARM!!"); debug(); #endif
    }
    else if(Temps.fridge_to_cold)
    {
        Alarms.fridge_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "A: Fridge to COLD!!"); debug(); #endif        
    }        
           
    
    Alarms.freezer_temp = NO;
    
    if(Temps.freezer_to_warm) 
    { 
        Alarms.freezer_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "A: Freezer to WARM!!"); debug(); #endif
    }
    else if(Temps.freezer_to_cold)
    {
        Alarms.freezer_temp = YES;
        #ifdef DEBUG sprintf(s_debug, "A: Freezer to COLD!!"); debug(); #endif
    }    
    
    
    #ifdef DEBUG
    if(Alarms.fridge_sensor_fault) { sprintf(s_debug, "A: Fridge SENSOR_FAULT!!"); debug(); }
    if(Alarms.freezer_sensor_fault) { sprintf(s_debug, "A: Freezer SENSOR_FAULT!!"); debug(); }
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
    
    
    // 238 - 674
    if(adc_val > 800)      Temps.pot_fridge = POT_OFF;
    else if(adc_val > 600) Temps.pot_fridge = 6;
    else if(adc_val > 510) Temps.pot_fridge = 5;
    else if(adc_val > 410) Temps.pot_fridge = 4;
    else if(adc_val > 340) Temps.pot_fridge = 3;        
    else if(adc_val > 310) Temps.pot_fridge = 2;
    else
        Temps.pot_fridge = 1;

    // not allow temp go under 1C, if so ignore FRIDGE_DEGREES_UNDER 
    if((Temps.pot_fridge - FRIDGE_DEGREES_UNDER) < 1)
    {
        //config.fridge_degrees_under = 0;
        switch(Temps.pot_fridge)
        {
            case 1: config.fridge_degrees_under = 0; break;   
            case 2: config.fridge_degrees_under = 1; break;
            case 3: config.fridge_degrees_under = 2; break;
            default: config.fridge_degrees_under = 0; 
        }
    }        

    #ifdef DEBUG       
    //sprintf(s_debug, "POT Fridge: %d, UNDER: %u", Temps.pot_fridge, config.fridge_degrees_under); debug();       
    sprintf(s_debug, "POT Fridge %d/%Lu", Temps.pot_fridge, adc_val); debug();   
    #endif


    
    adc_val = getAvgADC(ADC_POT_FREEZER, 30);
    output_float(POT_FREEZER_PULLDOWN);   
    
    //pot_freezer = FridgeLookup[1/ adc_val_freezer_pot / 8];
    //259 - 752
    if(adc_val > 870) Temps.pot_freezer = POT_OFF;
    else if(adc_val > 700) Temps.pot_freezer = -18;
    else if(adc_val > 570) Temps.pot_freezer = -19;
    else if(adc_val > 480) Temps.pot_freezer = -20;
    else if(adc_val > 440) Temps.pot_freezer = -21;
    else if(adc_val > 380) Temps.pot_freezer = -22;
    else if(adc_val > 330) Temps.pot_freezer = -23;    
    else if(adc_val > 320) Temps.pot_freezer = -24;
    else if(adc_val > 300) Temps.pot_freezer = -25;
    
        

    // not allow temp go under 1C, if so ignore FRIDGE_DEGREES_UNDER 
    if((Temps.pot_freezer - FREEZER_DEGREES_UNDER) < 1)
    {
        //config.fridge_degrees_under = 0;
        switch(Temps.pot_freezer)
        {
            //case -18: config.freezer_degrees_under = 0; break;   
            case -19: config.freezer_degrees_under = 1; break;
            case -20: config.freezer_degrees_under = 2; break;
            case -21: config.freezer_degrees_under = 3; break;
            default: config.freezer_degrees_under = 0; 
        }
    }
                                                            
    // tris also set pulldown's to input, so we don't need to set them float.
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    // not needed, many cycles before tris is used again  //delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
    
    #ifdef DEBUG       
    sprintf(s_debug, "POT Freezer %d/%Lu", Temps.pot_freezer, adc_val); debug();   
    //sprintf(s_debug, "POT Freezer: %d, UNDER: %u", Temps.pot_freezer, config.freezer_degrees_under); debug();   
    sprintf(s_debug, "Q5: %d", input(TRANSISTOR_Q5C)); debug();
    sprintf(s_debug, "------------------------------------------"); debug();
    #endif
                
        
    DisableSensors;
    
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