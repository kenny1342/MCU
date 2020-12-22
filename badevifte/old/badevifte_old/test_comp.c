///////////////////////////////////////////////////////
//Definitions

#include <16F628.H>
#define COMPARATOR_OP 2
#define LED 4

///////////////////////////////////////////////////////
void init_ports(void) {
   //GP0 & GP1 are inputs
   TRISIO = 0 | (1<<GP0) | (1<<GP1); // 0 - op, 1 - ip

   ANSEL = (1<<GP0) | (1<<GP1); ; // Ana. ip on GP0 GP1
}

///////////////////////////////////////////////////////
void init_comparator(void) {
   // Comparator with external input and output.
   // Cout = 0 (comparator output), Cinv =0 (inversion)
   CMCON = 0x01;
}

///////////////////////////////////////////////////////
// Start here
void main() {
int i;

   init_ports();

   // Show device is active on power up.
   for (i=0;i<5;i++) {

      GPIO |= (1<<COMPARATOR_OP);
      delay_ms(100);

      GPIO &= ~(1<<COMPARATOR_OP);
      delay_ms(100);
   }

   init_comparator();

   while(1) {;
      GPIO |= (1<<LED);
      delay_ms(100);

      GPIO &= ~(1<<LED);
      delay_ms(100);
   }
}
