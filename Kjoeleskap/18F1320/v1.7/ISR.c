#int_TIMER0
void  TIMER0_isr(void) // OF every 1ms
{
    
    if(++ISR_counter0 == 60000) // 60 sec
    {        
        if(Status.fridge_running)
        {
            Status.fridge_stopped_mins = 0;
            Status.fridge_run_mins++;
            if(Status.fridge_total_run_mins < 255) Status.fridge_total_run_mins++;
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
            if(Status.freezer_total_run_mins < 255) Status.freezer_total_run_mins++;
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
    }    
    else if(++ISR_counter0_2 == 1000) 
    {
        blink_toggle_fast ^= 1;
        blink_toggle ^= blink_toggle_fast;
        ISR_counter0_2 = 0;
    }
    
    if(!Status.sampling)
    {
        
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
            else if(SW_fridge_time > 100 && SW_fridge_time < 500)
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
            else if(SW_freezer_time > 100 && SW_freezer_time < 500)
                Status.mode_deepfreeze = !Status.mode_deepfreeze;
                
        }
        
                
        set_tris_b(TRISB_NORMAL);         
        Status.sampling_switches = 0;
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
        if(++sample_timer_sec == SAMPLE_INTERVAL_SECS) // last 6 samples need to be equal, before we call for actions , 10 sec between
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
    
    
    if(LEDs.DL1_Color == LED_AMBER && LEDs.DL2_Color == LED_AMBER)
    {

        output_bit(LED_COM, LED_AMBER); // LED_AMBER = COM=HIGH        
        // OBS: DL3 can't be ON when COM is high
        //turn on/off LEDs as requested (inverted because of AMBER=COM=HIGH)
        if(LEDs.DL1 == LED_BLINK) output_bit(LED_DL1, blink_toggle); else output_bit(LED_DL1, !LEDs.DL1);
        if(LEDs.DL2 == LED_BLINK) output_bit(LED_DL2, blink_toggle); else output_bit(LED_DL2, !LEDs.DL2);         
    }    
/*
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
    */
    else // both GREEN
    {
  
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
                                    
    }
    
    
    //clear_interrupt(INT_TIMER2);
    //enable_interrupts(INT_TIMER2);
}

#int_TIMER3
void  TIMER3_isr(void) // 524ms
{
    setLEDs();
}
